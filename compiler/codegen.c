#include "codegen.h"

#include "opcode.h"
#include "pphp/pphp.h"
#include "pclass.h"
#include "value_ops.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct loop_context {
    size_t continue_target;
    unsigned finally_count;
    int is_foreach;
    int is_switch;
} loop_context;

typedef struct jump_record {
    size_t operand;
    unsigned loop_index;
    int is_continue;
    int patched;
} jump_record;

typedef struct generator {
    pmodule *module;
    pproto *proto;
    pc_codegen_error *error;
    loop_context loops[PPHP_PARSE_DEPTH_MAX];
    unsigned loop_count;
    jump_record jumps[256];
    size_t jump_count;
    const pc_ast *finally_blocks[PPHP_PARSE_DEPTH_MAX];
    unsigned finally_count;
    int class_scope;
    int class_has_parent;
    int return_void;
    int failed;
} generator;

static void fail(generator *gen, uint32_t line, const char *format, ...) {
    va_list arguments;
    if (gen->failed) {
        return;
    }
    gen->failed = 1;
    gen->error->line = line;
    va_start(arguments, format);
    (void)vsnprintf(gen->error->message, sizeof(gen->error->message), format, arguments);
    va_end(arguments);
}

typedef enum declared_type_context {
    TYPE_CONTEXT_NONE,
    TYPE_CONTEXT_CLASS,
    TYPE_CONTEXT_METHOD
} declared_type_context;

static int token_bytes_equal_ci(pc_token token, const char *bytes,
                                size_t length) {
    size_t i;
    if (token.length != length) return 0;
    for (i = 0U; i < length; i++) {
        unsigned char left = (unsigned char)token.start[i];
        unsigned char right = (unsigned char)bytes[i];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

static int type_is_only(const pc_ast *type, pc_token_type token) {
    return type != NULL && type->next == NULL &&
           type->as.type_decl.name.type == token;
}

static int validate_type_spec(generator *gen, const pc_ast *type,
                              declared_type_context context, int has_parent,
                              int is_return, int is_property) {
    while (type != NULL && !gen->failed) {
        pc_token_type token = type->as.type_decl.name.type;
#if !PPHP_ENABLE_FLOAT
        if (token == T_FLOAT_TYPE) {
            fail(gen, type->line,
                 "float type declarations require PPHP_ENABLE_FLOAT=1");
            return 0;
        }
#endif
        if ((token == T_SELF || token == T_PARENT) &&
            context == TYPE_CONTEXT_NONE) {
            fail(gen, type->line,
                 token == T_SELF ? "self type requires class scope"
                                 : "parent type requires class scope");
            return 0;
        }
        if (token == T_PARENT && !has_parent) {
            fail(gen, type->line, "parent type requires a parent class");
            return 0;
        }
        if (token == T_STATIC &&
            (context != TYPE_CONTEXT_METHOD || !is_return)) {
            fail(gen, type->line,
                 "static type is only valid as a method return type");
            return 0;
        }
        if (is_property &&
            (token == T_VOID || token == T_STATIC || token == T_CALLABLE)) {
            fail(gen, type->line, "invalid property type");
            return 0;
        }
        type = type->next;
    }
    return !gen->failed;
}

static int validate_callable_types(generator *gen, const pc_ast *parameters,
                                   const pc_ast *return_type,
                                   declared_type_context context,
                                   int has_parent) {
    while (parameters != NULL) {
        if (!validate_type_spec(gen, parameters->as.parameter.type, context,
                                has_parent, 0, 0)) return 0;
        parameters = parameters->next;
    }
    return validate_type_spec(gen, return_type, context, has_parent, 1, 0);
}

#if PPHP_TYPECHECK
static uint8_t declared_type_kind(pc_token_type token) {
    switch (token) {
        case T_INT_TYPE: return PTYPE_INT;
        case T_FLOAT_TYPE: return PTYPE_FLOAT;
        case T_STRING_TYPE: return PTYPE_STRING;
        case T_BOOL_TYPE: return PTYPE_BOOL;
        case T_ARRAY: return PTYPE_ARRAY;
        case T_CALLABLE: return PTYPE_CALLABLE;
        case T_MIXED: return PTYPE_MIXED;
        case T_VOID: return PTYPE_VOID;
        case T_NULL: return PTYPE_NULL;
        case T_SELF: return PTYPE_SELF;
        case T_STATIC: return PTYPE_STATIC;
        case T_PARENT: return PTYPE_PARENT;
        case T_IDENTIFIER: return PTYPE_NAMED;
        default: return 0U;
    }
}

static int compile_type_spec(generator *gen, const pc_ast *type,
                             ptype_spec *spec) {
    while (type != NULL && !gen->failed) {
        pc_token token = type->as.type_decl.name;
        uint8_t kind = declared_type_kind(token.type);
        if (kind == 0U) {
            fail(gen, type->line, "unsupported declared type");
            return 0;
        }
        if (!ptype_spec_add(spec, kind,
                            kind == PTYPE_NAMED ? token.start : NULL,
                            kind == PTYPE_NAMED ? token.length : 0U)) {
            fail(gen, type->line, "out of memory storing declared type");
            return 0;
        }
        type = type->next;
    }
    return !gen->failed;
}

static int compile_callable_types(generator *gen, pproto *proto,
                                  const pc_ast *parameters,
                                  const pc_ast *return_type) {
    size_t index = 0U;
    if (proto->n_params != 0U) {
        proto->parameter_types = pphp_alloc(
            (size_t)proto->n_params * sizeof(*proto->parameter_types));
        if (proto->parameter_types == NULL) {
            fail(gen, parameters == NULL ? 0U : parameters->line,
                 "out of memory storing parameter types");
            return 0;
        }
        memset(proto->parameter_types, 0,
               (size_t)proto->n_params * sizeof(*proto->parameter_types));
    }
    while (parameters != NULL && index < proto->n_params) {
        if (!compile_type_spec(gen, parameters->as.parameter.type,
                               &proto->parameter_types[index])) return 0;
        index++;
        parameters = parameters->next;
    }
    return compile_type_spec(gen, return_type, &proto->return_type);
}

static uint16_t declared_type_constant(generator *gen, const pc_ast *type) {
    const pc_ast *cursor = type;
    size_t length = 0U;
    size_t offset = 0U;
    char *bytes;
    pstring *string;
    pvalue value;
    uint16_t index = UINT16_MAX;
    while (cursor != NULL) {
        if (cursor->as.type_decl.name.length > SIZE_MAX - length ||
            (cursor->next != NULL && length == SIZE_MAX)) {
            fail(gen, cursor->line, "declared property type is too long");
            return UINT16_MAX;
        }
        length += cursor->as.type_decl.name.length;
        if (cursor->next != NULL) length++;
        cursor = cursor->next;
    }
    if (type == NULL) return UINT16_MAX;
    bytes = pphp_alloc(length);
    if (bytes == NULL) {
        fail(gen, type->line, "out of memory storing property type");
        return UINT16_MAX;
    }
    cursor = type;
    while (cursor != NULL) {
        memcpy(bytes + offset, cursor->as.type_decl.name.start,
               cursor->as.type_decl.name.length);
        offset += cursor->as.type_decl.name.length;
        if (cursor->next != NULL) bytes[offset++] = '|';
        cursor = cursor->next;
    }
    string = ps_new(bytes, length);
    pphp_free(bytes);
    if (string == NULL) {
        fail(gen, type->line, "out of memory storing property type");
        return UINT16_MAX;
    }
    value = pv_heap(PT_STRING, &string->header);
    if (!pproto_add_constant(gen->proto, value, &index)) {
        fail(gen, type->line, "constant pool limit exceeded");
    }
    pv_release(value);
    return index;
}
#endif

static void emit_byte(generator *gen, uint8_t byte, uint32_t line) {
    if (!gen->failed && !pproto_emit_u8(gen->proto, byte)) {
        fail(gen, line, "out of memory while emitting bytecode");
    }
}

static void emit_u16(generator *gen, uint16_t value, uint32_t line) {
    if (!gen->failed && !pproto_emit_u16(gen->proto, value)) {
        fail(gen, line, "out of memory while emitting bytecode");
    }
}

static void emit_i32(generator *gen, int32_t value, uint32_t line) {
    if (!gen->failed && !pproto_emit_i32(gen->proto, value)) {
        fail(gen, line, "out of memory while emitting bytecode");
    }
}

static size_t emit_jump(generator *gen, pphp_opcode opcode, uint32_t line) {
    size_t operand;
    emit_byte(gen, (uint8_t)opcode, line);
    operand = gen->proto->code_length;
    emit_u16(gen, 0U, line);
    return operand;
}

static void patch_jump(generator *gen, size_t operand, size_t target,
                       uint32_t line) {
    if (!gen->failed && !pproto_patch_i16(gen->proto, operand, target)) {
        fail(gen, line, "jump exceeds 16-bit bytecode range");
    }
}

static void patch_foreach_jump(generator *gen, size_t operand, size_t target,
                               uint32_t line) {
    ptrdiff_t relative = (ptrdiff_t)target - (ptrdiff_t)(operand + 3U);
    int16_t encoded;
    if (gen->failed) return;
    if (operand + 3U > gen->proto->code_length || relative < INT16_MIN ||
        relative > INT16_MAX) {
        fail(gen, line, "foreach jump exceeds 16-bit bytecode range");
        return;
    }
    encoded = (int16_t)relative;
    gen->proto->code[operand] = (uint8_t)((uint16_t)encoded & 0xffU);
    gen->proto->code[operand + 1U] = (uint8_t)((uint16_t)encoded >> 8U);
}

static void emit_back_jump(generator *gen, size_t target, uint32_t line) {
    size_t operand = emit_jump(gen, OP_JMP, line);
    patch_jump(gen, operand, target, line);
}

static int token_equal(pc_token token, const pstring *string) {
    size_t i;
    if (string->length != token.length) return 0;
    for (i = 0U; i < token.length; i++) {
        unsigned char left = (unsigned char)token.start[i];
        unsigned char right = (unsigned char)ps_data(string)[i];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

static void integer_token_prefix(pc_token token, size_t *position,
                                 unsigned *base) {
    *position = 0U;
    *base = 10U;
    if (token.length >= 2U && token.start[0] == '0') {
        if (token.start[1] == 'x' || token.start[1] == 'X') {
            *base = 16U;
            *position = 2U;
        } else if (token.start[1] == 'b' || token.start[1] == 'B') {
            *base = 2U;
            *position = 2U;
        } else if (token.start[1] == 'o' || token.start[1] == 'O') {
            *base = 8U;
            *position = 2U;
        } else if (token.start[1] >= '0' && token.start[1] <= '7') {
            *base = 8U;
            *position = 1U;
        }
    }
}

static unsigned integer_token_digit(char value) {
    if (value >= '0' && value <= '9') {
        return (unsigned)(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return (unsigned)(value - 'a') + 10U;
    }
    return (unsigned)(value - 'A') + 10U;
}

static uint64_t parse_integer_token(pc_token token, int *overflow) {
    size_t i;
    unsigned base;
    uint64_t value = 0U;
    *overflow = 0;
    integer_token_prefix(token, &i, &base);
    for (; i < token.length; i++) {
        unsigned digit;
        char value_char = token.start[i];
        if (value_char == '_') {
            continue;
        }
        digit = integer_token_digit(value_char);
        if (value > (UINT64_MAX - digit) / base) {
            *overflow = 1;
        } else {
            value = value * base + digit;
        }
    }
    return value;
}

#if PPHP_ENABLE_FLOAT
static pphp_float parse_integer_float_token(pc_token token) {
    size_t i;
    unsigned base;
    integer_token_prefix(token, &i, &base);
    return pphp_integer_digits_to_float(token.start + i, token.length - i,
                                        base);
}

static pphp_float parse_float_token(pc_token token) {
    size_t i = 0U;
    pphp_float value = 0;
    pphp_float fraction = (pphp_float)0.1;
    int after_dot = 0;
    int exponent = 0;
    int exponent_sign = 1;
    while (i < token.length && token.start[i] != 'e' && token.start[i] != 'E') {
        char c = token.start[i++];
        if (c == '_') {
            continue;
        }
        if (c == '.') {
            after_dot = 1;
            continue;
        }
        if (after_dot) {
            value += (pphp_float)(c - '0') * fraction;
            fraction *= (pphp_float)0.1;
        } else {
            value = value * (pphp_float)10 + (pphp_float)(c - '0');
        }
    }
    if (i < token.length) {
        i++;
        if (i < token.length && (token.start[i] == '+' || token.start[i] == '-')) {
            exponent_sign = token.start[i++] == '-' ? -1 : 1;
        }
        while (i < token.length) {
            char c = token.start[i++];
            if (c != '_') {
                exponent = exponent * 10 + (c - '0');
            }
        }
    }
    while (exponent-- > 0) {
        value *= exponent_sign > 0 ? (pphp_float)10 : (pphp_float)0.1;
    }
    return value;
}
#endif

static int escape_hex_digit(char byte) {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}

static size_t encode_utf8(char *output, uint32_t codepoint) {
    if (codepoint <= 0x7fU) {
        output[0] = (char)codepoint;
        return 1U;
    }
    if (codepoint <= 0x7ffU) {
        output[0] = (char)(0xc0U | (codepoint >> 6U));
        output[1] = (char)(0x80U | (codepoint & 0x3fU));
        return 2U;
    }
    if (codepoint <= 0xffffU &&
        !(codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
        output[0] = (char)(0xe0U | (codepoint >> 12U));
        output[1] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        output[2] = (char)(0x80U | (codepoint & 0x3fU));
        return 3U;
    }
    if (codepoint <= 0x10ffffU) {
        output[0] = (char)(0xf0U | (codepoint >> 18U));
        output[1] = (char)(0x80U | ((codepoint >> 12U) & 0x3fU));
        output[2] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        output[3] = (char)(0x80U | (codepoint & 0x3fU));
        return 4U;
    }
    return 0U;
}

static pstring *decode_string(pc_token token) {
    const char *source = token.start;
    size_t length = token.length;
    size_t begin = 0U;
    size_t end = length;
    char *decoded;
    size_t output = 0U;
    size_t i;
    int escapes = token.type == T_DOUBLE_QUOTED || token.type == T_INTERP_PART;

    if ((token.type == T_SINGLE_QUOTED || token.type == T_DOUBLE_QUOTED) && length >= 2U) {
        begin = 1U;
        end = length - 1U;
        escapes = token.type == T_DOUBLE_QUOTED;
    } else if (token.type == T_HEREDOC || token.type == T_NOWDOC) {
        const char *first_newline = memchr(source, '\n', length);
        const char *last_newline = NULL;
        if (first_newline != NULL) {
            const char *cursor = source + length;
            while (cursor > first_newline + 1) {
                cursor--;
                if (*cursor == '\n') {
                    last_newline = cursor;
                    break;
                }
            }
            begin = (size_t)(first_newline + 1 - source);
            end = last_newline != NULL ? (size_t)(last_newline - source) : length;
        }
        escapes = token.type == T_HEREDOC;
    }
    decoded = pphp_alloc(end - begin + 1U);
    if (decoded == NULL) {
        return NULL;
    }
    for (i = begin; i < end; i++) {
        char c = source[i];
        if (c != '\\' || i + 1U >= end) {
            decoded[output++] = c;
            continue;
        }
        if (!escapes && token.type != T_SINGLE_QUOTED) {
            decoded[output++] = c;
            continue;
        }
        c = source[++i];
        if (token.type == T_SINGLE_QUOTED && c != '\\' && c != '\'') {
            decoded[output++] = '\\';
            decoded[output++] = c;
            continue;
        }
        switch (c) {
            case 'n': decoded[output++] = '\n'; break;
            case 'r': decoded[output++] = '\r'; break;
            case 't': decoded[output++] = '\t'; break;
            case 'v': decoded[output++] = '\v'; break;
            case 'f': decoded[output++] = '\f'; break;
            case 'e': decoded[output++] = 27; break;
            case '\\': decoded[output++] = '\\'; break;
            case '\'': decoded[output++] = '\''; break;
            case '"': decoded[output++] = '"'; break;
            case '$': decoded[output++] = '$'; break;
            case 'x': {
                int digit;
                unsigned value = 0U;
                unsigned count = 0U;
                while (count < 2U && i + 1U < end &&
                       (digit = escape_hex_digit(source[i + 1U])) >= 0) {
                    i++;
                    value = value * 16U + (unsigned)digit;
                    count++;
                }
                if (count == 0U) {
                    decoded[output++] = '\\';
                    decoded[output++] = 'x';
                } else {
                    decoded[output++] = (char)value;
                }
                break;
            }
            case 'u': {
                size_t cursor = i + 1U;
                uint32_t codepoint = 0U;
                unsigned digits = 0U;
                int valid = cursor < end && source[cursor] == '{';
                if (valid) cursor++;
                while (valid && cursor < end && source[cursor] != '}') {
                    int digit = escape_hex_digit(source[cursor]);
                    if (digit < 0 || digits >= 6U) {
                        valid = 0;
                        break;
                    }
                    codepoint = codepoint * 16U + (uint32_t)digit;
                    digits++;
                    cursor++;
                }
                if (valid && digits != 0U && cursor < end &&
                    source[cursor] == '}') {
                    size_t encoded = encode_utf8(decoded + output,
                                                 codepoint);
                    if (encoded != 0U) {
                        output += encoded;
                        i = cursor;
                        break;
                    }
                }
                decoded[output++] = '\\';
                decoded[output++] = 'u';
                break;
            }
            default:
                if (c >= '0' && c <= '7') {
                    unsigned value = (unsigned)(c - '0');
                    unsigned count = 1U;
                    while (count < 3U && i + 1U < end &&
                           source[i + 1U] >= '0' &&
                           source[i + 1U] <= '7') {
                        i++;
                        value = value * 8U +
                                (unsigned)(source[i] - '0');
                        count++;
                    }
                    decoded[output++] = (char)(value & 0xffU);
                } else {
                    decoded[output++] = '\\';
                    decoded[output++] = c;
                }
                break;
        }
    }
    {
        pstring *string = ps_new(decoded, output);
        pphp_free(decoded);
        return string;
    }
}

static void emit_constant(generator *gen, pvalue value, uint32_t line) {
    uint16_t index;
    if (!pproto_add_constant(gen->proto, value, &index)) {
        fail(gen, line, "constant pool limit exceeded");
        return;
    }
    emit_byte(gen, OP_LOAD_CONST, line);
    emit_u16(gen, index, line);
}

static uint16_t name_constant(generator *gen, pc_token token) {
    pstring *name = ps_new(token.start, token.length);
    pvalue value;
    uint16_t index = UINT16_MAX;
    if (name == NULL) {
        fail(gen, token.line, "out of memory for name");
        return index;
    }
    value = pv_heap(PT_STRING, &name->header);
    if (!pproto_add_constant(gen->proto, value, &index)) {
        fail(gen, token.line, "constant pool limit exceeded");
    }
    pv_release(value);
    return index;
}

static uint16_t member_name_constant(generator *gen, pc_token token) {
    if (token.length != 0U && token.start[0] == '$') {
        token.start++;
        token.length--;
    }
    return name_constant(gen, token);
}

static int variable_slot(generator *gen, pc_token token, uint8_t *slot) {
    const char *name = token.start;
    size_t length = token.length;
    if (length != 0U && *name == '$') {
        name++;
        length--;
    }
    if (!pproto_add_local(gen->proto, name, length, slot)) {
        fail(gen, token.line, "too many local variables or out of memory");
        return 0;
    }
    return 1;
}

static int exception_slot(generator *gen, uint32_t line, uint8_t *slot) {
    char name[32];
    int length = snprintf(name, sizeof(name), "\001exception%u",
                          (unsigned)gen->proto->n_locals);
    if (length <= 0 || (size_t)length >= sizeof(name) ||
        !pproto_add_local(gen->proto, name, (size_t)length, slot)) {
        fail(gen, line, "cannot allocate pending exception slot");
        return 0;
    }
    return 1;
}

static int temporary_slot(generator *gen, uint32_t line, uint8_t *slot) {
    char name[32];
    int length = snprintf(name, sizeof(name), "\001temporary%u",
                          (unsigned)gen->proto->n_locals);
    if (length <= 0 || (size_t)length >= sizeof(name) ||
        !pproto_add_local(gen->proto, name, (size_t)length, slot)) {
        fail(gen, line, "cannot allocate compiler temporary slot");
        return 0;
    }
    return 1;
}

static void compile_expression(generator *gen, const pc_ast *node);
static void compile_quiet_expression(generator *gen, const pc_ast *node);
static void compile_statement(generator *gen, const pc_ast *node);
static void compile_class_definition(generator *gen, const pc_ast *class_node);
static void compile_parameter_defaults(generator *gen,
                                       const pc_ast *parameters);

typedef struct capture_set {
    pc_token tokens[64];
    size_t count;
    const pc_ast *parameters;
} capture_set;

static int variable_name_equal(pc_token left, pc_token right) {
    if (left.length != 0U && left.start[0] == '$') {
        left.start++;
        left.length--;
    }
    if (right.length != 0U && right.start[0] == '$') {
        right.start++;
        right.length--;
    }
    return left.length == right.length &&
           memcmp(left.start, right.start, left.length) == 0;
}

static int capture_is_parameter(const capture_set *set, pc_token token) {
    const pc_ast *parameter = set->parameters;
    while (parameter != NULL) {
        if (variable_name_equal(parameter->as.parameter.name, token)) return 1;
        parameter = parameter->next;
    }
    return 0;
}

static int capture_add(capture_set *set, pc_token token) {
    size_t i;
    if (capture_is_parameter(set, token)) return 1;
    for (i = 0U; i < set->count; i++) {
        if (variable_name_equal(set->tokens[i], token)) return 1;
    }
    if (set->count >= sizeof(set->tokens) / sizeof(set->tokens[0])) return 0;
    set->tokens[set->count++] = token;
    return 1;
}

static int collect_captures(capture_set *set, const pc_ast *node);

static int collect_capture_list(capture_set *set, const pc_ast *node) {
    while (node != NULL) {
        if (!collect_captures(set, node)) return 0;
        node = node->next;
    }
    return 1;
}

static int collect_captures(capture_set *set, const pc_ast *node) {
    if (node == NULL) return 1;
    switch (node->kind) {
        case AST_VARIABLE:
            return capture_add(set, node->as.literal.token);
        case AST_PROGRAM:
        case AST_BLOCK:
        case AST_ARRAY:
        case AST_ECHO:
            return collect_capture_list(set, node->as.list.items);
        case AST_GLOBAL:
            return 1;
        case AST_UNARY:
        case AST_UNSET:
        case AST_ISSET:
        case AST_EMPTY:
            return collect_captures(set, node->as.unary.operand);
        case AST_BINARY:
        case AST_ASSIGN:
            return collect_captures(set, node->as.binary.left) &&
                   collect_captures(set, node->as.binary.right);
        case AST_TERNARY:
            return collect_captures(set, node->as.ternary.condition) &&
                   collect_captures(set, node->as.ternary.then_expr) &&
                   collect_captures(set, node->as.ternary.else_expr);
        case AST_MATCH:
            return collect_captures(set, node->as.match_expr.subject) &&
                   collect_capture_list(set, node->as.match_expr.arms);
        case AST_MATCH_ARM:
            return collect_capture_list(set, node->as.match_arm.conditions) &&
                   collect_captures(set, node->as.match_arm.result);
        case AST_CALL:
            return collect_captures(set, node->as.call.callee) &&
                   collect_capture_list(set, node->as.call.arguments);
        case AST_INDEX:
            return collect_captures(set, node->as.index.base) &&
                   collect_captures(set, node->as.index.key);
        case AST_MEMBER:
            return collect_captures(set, node->as.member.base) &&
                   collect_captures(set, node->as.member.dynamic_name);
        case AST_ARRAY_ITEM:
            return collect_captures(set, node->as.array_item.key) &&
                   collect_captures(set, node->as.array_item.value);
        case AST_EXPR_STMT:
        case AST_RETURN:
        case AST_THROW:
            return collect_captures(set, node->as.expression.expression);
        case AST_IF:
            return collect_captures(set, node->as.if_stmt.condition) &&
                   collect_captures(set, node->as.if_stmt.then_branch) &&
                   collect_captures(set, node->as.if_stmt.else_branch);
        case AST_WHILE:
        case AST_DO_WHILE:
            return collect_captures(set, node->as.loop.condition) &&
                   collect_captures(set, node->as.loop.body);
        case AST_FOR:
            return collect_capture_list(set, node->as.for_stmt.initializers) &&
                   collect_capture_list(set, node->as.for_stmt.conditions) &&
                   collect_capture_list(set, node->as.for_stmt.increments) &&
                   collect_captures(set, node->as.for_stmt.body);
        case AST_FOREACH:
            return collect_captures(set, node->as.foreach_stmt.iterable) &&
                   collect_captures(set, node->as.foreach_stmt.key) &&
                   collect_captures(set, node->as.foreach_stmt.value) &&
                   collect_captures(set, node->as.foreach_stmt.body);
        case AST_SWITCH:
            return collect_captures(set, node->as.switch_stmt.subject) &&
                   collect_capture_list(set, node->as.switch_stmt.cases);
        case AST_CASE:
            return collect_captures(set, node->as.case_stmt.condition) &&
                   collect_captures(set, node->as.case_stmt.body);
        case AST_STATIC:
        case AST_CONST:
            return collect_captures(set, node->as.binding.value);
        case AST_INCLUDE:
            return collect_captures(set, node->as.include_stmt.path);
        case AST_NEW:
            return collect_capture_list(set, node->as.new_expr.arguments);
        case AST_TRY:
            return collect_captures(set, node->as.try_stmt.try_block) &&
                   collect_capture_list(set, node->as.try_stmt.catches) &&
                   collect_captures(set, node->as.try_stmt.finally_block);
        case AST_CATCH:
            return collect_captures(set, node->as.catch_stmt.body);
        case AST_CLOSURE:
        {
            capture_set nested;
            size_t i;
            memset(&nested, 0, sizeof(nested));
            nested.parameters = node->as.closure.parameters;
            if (node->as.closure.is_arrow) {
                if (!collect_captures(&nested, node->as.closure.body)) return 0;
            } else {
                const pc_ast *capture = node->as.closure.captures;
                while (capture != NULL) {
                    if (!capture_add(&nested, capture->as.literal.token)) return 0;
                    capture = capture->next;
                }
                if (!node->as.closure.is_static) {
                    capture_set referenced;
                    memset(&referenced, 0, sizeof(referenced));
                    referenced.parameters = node->as.closure.parameters;
                    if (!collect_captures(&referenced, node->as.closure.body)) return 0;
                    for (i = 0U; i < referenced.count; i++) {
                        pc_token token = referenced.tokens[i];
                        if (token.length == 5U &&
                            memcmp(token.start, "$this", 5U) == 0 &&
                            !capture_add(&nested, token)) return 0;
                    }
                }
            }
            for (i = 0U; i < nested.count; i++) {
                if (!capture_add(set, nested.tokens[i])) return 0;
            }
            return 1;
        }
        default:
            return 1;
    }
}

static void compile_closure_expression(generator *gen, const pc_ast *node) {
    capture_set captures;
    const pc_ast *capture;
    const pc_ast *parameter;
    pproto *proto;
    generator child;
    uint8_t parent_slots[64];
    uint16_t proto_index;
    char name[32];
    int name_length;
    int saw_default = 0;
    size_t i;
    memset(&captures, 0, sizeof(captures));
    captures.parameters = node->as.closure.parameters;
    if (node->as.closure.is_arrow) {
        if (!collect_captures(&captures, node->as.closure.body)) {
            fail(gen, node->line, "too many variables captured by arrow function");
            return;
        }
        if (node->as.closure.is_static) {
            for (i = 0U; i < captures.count; i++) {
                pc_token token = captures.tokens[i];
                if (token.length == 5U && memcmp(token.start, "$this", 5U) == 0) {
                    fail(gen, node->line,
                         "static arrow function cannot use $this");
                    return;
                }
            }
        }
    } else {
        capture = node->as.closure.captures;
        while (capture != NULL) {
            if (capture_is_parameter(&captures, capture->as.literal.token)) {
                fail(gen, capture->line,
                     "closure cannot capture a parameter with the same name");
                return;
            }
            if (!capture_add(&captures, capture->as.literal.token)) {
                fail(gen, capture->line, "too many closure captures");
                return;
            }
            capture = capture->next;
        }
        {
            capture_set referenced;
            memset(&referenced, 0, sizeof(referenced));
            referenced.parameters = node->as.closure.parameters;
            if (!collect_captures(&referenced, node->as.closure.body)) {
                fail(gen, node->line, "too many variables referenced by closure");
                return;
            }
            for (i = 0U; i < referenced.count; i++) {
                pc_token token = referenced.tokens[i];
                if (token.length == 5U && memcmp(token.start, "$this", 5U) == 0) {
                    if (node->as.closure.is_static) {
                        fail(gen, node->line,
                             "static closure cannot use $this");
                        return;
                    }
                    if (!capture_add(&captures, token)) {
                        fail(gen, node->line, "too many closure captures");
                        return;
                    }
                }
            }
        }
    }
    if (gen->module->count > UINT16_MAX) {
        fail(gen, node->line, "too many function prototypes");
        return;
    }
    proto_index = (uint16_t)gen->module->count;
    name_length = snprintf(name, sizeof(name), "{closure#%u}", proto_index);
    if (name_length <= 0 || (size_t)name_length >= sizeof(name)) {
        fail(gen, node->line, "cannot create closure name");
        return;
    }
    proto = pproto_new(name, (size_t)name_length);
    if (proto == NULL || !pmodule_add(gen->module, proto)) {
        pproto_destroy(proto);
        fail(gen, node->line, "out of memory creating closure");
        return;
    }
    parameter = node->as.closure.parameters;
    while (parameter != NULL) {
        pc_token parameter_name = parameter->as.parameter.name;
        uint8_t slot;
        if (parameter_name.length != 0U && parameter_name.start[0] == '$') {
            parameter_name.start++;
            parameter_name.length--;
        }
        if (!pproto_add_local(proto, parameter_name.start,
                              parameter_name.length, &slot)) {
            fail(gen, parameter->line, "too many closure parameters");
            return;
        }
        proto->n_params++;
        if (proto->n_params > 31U) {
            fail(gen, parameter->line, "a closure may have at most 31 parameters");
            return;
        }
        if (parameter->as.parameter.variadic && parameter->next != NULL) {
            fail(gen, parameter->line, "variadic parameter must be last");
            return;
        }
        if (parameter->as.parameter.variadic &&
            parameter->as.parameter.default_value != NULL) {
            fail(gen, parameter->line,
                 "variadic parameter cannot have a default value");
            return;
        }
        if (parameter->as.parameter.default_value == NULL &&
            !parameter->as.parameter.variadic) {
            if (saw_default) {
                fail(gen, parameter->line,
                     "required closure parameter follows optional parameter");
                return;
            }
            proto->n_required++;
        } else {
            saw_default = 1;
        }
        proto->variadic = (uint8_t)parameter->as.parameter.variadic;
        parameter = parameter->next;
    }
    for (i = 0U; i < captures.count; i++) {
        pc_token capture_name = captures.tokens[i];
        uint8_t ignored;
        if (!variable_slot(gen, capture_name, &parent_slots[i])) return;
        if (capture_name.length != 0U && capture_name.start[0] == '$') {
            capture_name.start++;
            capture_name.length--;
        }
        if (!pproto_add_local(proto, capture_name.start, capture_name.length,
                              &ignored)) {
            fail(gen, node->line, "too many closure captures");
            return;
        }
    }
    memset(&child, 0, sizeof(child));
    child.module = gen->module;
    child.proto = proto;
    child.error = gen->error;
    child.class_scope = gen->class_scope;
    child.class_has_parent = gen->class_has_parent;
    child.return_void = type_is_only(node->as.closure.return_type, T_VOID);
    if (!validate_callable_types(
            gen, node->as.closure.parameters, node->as.closure.return_type,
            gen->class_scope ? TYPE_CONTEXT_CLASS : TYPE_CONTEXT_NONE,
            gen->class_has_parent)) return;
#if PPHP_TYPECHECK
    if (!compile_callable_types(gen, proto, node->as.closure.parameters,
                                node->as.closure.return_type)) return;
#endif
    compile_parameter_defaults(&child, node->as.closure.parameters);
#if PPHP_TYPECHECK
    emit_byte(&child, OP_TYPECHECK_ARGS, node->line);
#endif
    if (node->as.closure.is_arrow) {
        if (child.return_void) {
            fail(&child, node->line,
                 "a void function must not return a value");
        }
        compile_expression(&child, node->as.closure.body);
        emit_byte(&child, OP_RET, node->line);
    } else {
        compile_statement(&child, node->as.closure.body);
        emit_byte(&child, OP_RET_NULL, node->line);
    }
    proto->max_stack = PPHP_STACK_SLOTS;
    if (child.failed) {
        gen->failed = 1;
        return;
    }
    emit_byte(gen, OP_CLOSURE, node->line);
    emit_u16(gen, proto_index, node->line);
    emit_byte(gen, (uint8_t)captures.count, node->line);
    for (i = 0U; i < captures.count; i++) {
        emit_byte(gen, 0U, node->line);
        emit_byte(gen, parent_slots[i], node->line);
    }
}

static void compile_transfer_finally_blocks(generator *gen,
                                            unsigned preserve_count) {
    unsigned original = gen->finally_count;
    unsigned remaining = original;
    while (remaining > preserve_count && !gen->failed) {
        remaining--;
        gen->finally_count = remaining;
        compile_statement(gen, gen->finally_blocks[remaining]);
    }
    gen->finally_count = original;
}

static pphp_opcode binary_opcode(pc_token_type type) {
    switch (type) {
        case T_PLUS: return OP_ADD;
        case T_MINUS: return OP_SUB;
        case T_STAR: return OP_MUL;
        case T_SLASH: return OP_DIV;
        case T_PERCENT: return OP_MOD;
        case T_POW: return OP_POW;
        case T_DOT: return OP_CONCAT;
        case T_AMP: return OP_BAND;
        case T_PIPE: return OP_BOR;
        case T_CARET: return OP_BXOR;
        case T_SHIFT_LEFT: return OP_SHL;
        case T_SHIFT_RIGHT: return OP_SHR;
        case T_EQUAL_EQUAL: return OP_EQ;
        case T_NOT_EQUAL: return OP_NE;
        case T_IDENTICAL: return OP_IDENT;
        case T_NOT_IDENTICAL: return OP_NIDENT;
        case T_LT: return OP_LT;
        case T_LT_EQUAL: return OP_LE;
        case T_GT: return OP_GT;
        case T_GT_EQUAL: return OP_GE;
        case T_SPACESHIP: return OP_CMP;
        default: return OP_NOP;
    }
}

static int is_constant_default(const pc_ast *node) {
    const pc_ast *item;
    if (node == NULL) return 0;
    switch (node->kind) {
        case AST_NULL:
        case AST_BOOL:
        case AST_INT:
        case AST_FLOAT:
        case AST_STRING:
        case AST_IDENTIFIER:
            return 1;
        case AST_UNARY:
            return node->as.unary.op != T_ELLIPSIS &&
                   node->as.unary.op != T_CLONE &&
                   is_constant_default(node->as.unary.operand);
        case AST_BINARY:
            return binary_opcode(node->as.binary.op) != OP_NOP &&
                   is_constant_default(node->as.binary.left) &&
                   is_constant_default(node->as.binary.right);
        case AST_TERNARY:
            return is_constant_default(node->as.ternary.condition) &&
                   (node->as.ternary.then_expr == NULL ||
                    is_constant_default(node->as.ternary.then_expr)) &&
                   is_constant_default(node->as.ternary.else_expr);
        case AST_ARRAY:
            item = node->as.list.items;
            while (item != NULL) {
                if (item->kind != AST_ARRAY_ITEM || item->as.array_item.spread ||
                    (item->as.array_item.key != NULL &&
                     !is_constant_default(item->as.array_item.key)) ||
                    !is_constant_default(item->as.array_item.value)) {
                    return 0;
                }
                item = item->next;
            }
            return 1;
        default:
            return 0;
    }
}

static void compile_parameter_defaults(generator *gen,
                                       const pc_ast *parameters) {
    const pc_ast *parameter = parameters;
    size_t index = 0U;
    while (parameter != NULL && !gen->failed) {
        if (parameter->as.parameter.default_value != NULL) {
            uint8_t slot;
            size_t supplied;
            if (!is_constant_default(parameter->as.parameter.default_value)) {
                fail(gen, parameter->line,
                     "parameter default must be a constant expression");
                return;
            }
            emit_byte(gen, OP_LOAD_ARGC, parameter->line);
            emit_byte(gen, OP_LOAD_I8, parameter->line);
            emit_byte(gen, (uint8_t)index, parameter->line);
            emit_byte(gen, OP_GT, parameter->line);
            supplied = emit_jump(gen, OP_JMP_IF, parameter->line);
            compile_expression(gen, parameter->as.parameter.default_value);
            if (!variable_slot(gen, parameter->as.parameter.name, &slot)) return;
            emit_byte(gen, OP_STORE_LOCAL, parameter->line);
            emit_byte(gen, slot, parameter->line);
            patch_jump(gen, supplied, gen->proto->code_length,
                       parameter->line);
        }
        index++;
        parameter = parameter->next;
    }
}

static pc_token_type compound_operator(pc_token_type type) {
    switch (type) {
        case T_PLUS_EQUAL: return T_PLUS;
        case T_MINUS_EQUAL: return T_MINUS;
        case T_STAR_EQUAL: return T_STAR;
        case T_SLASH_EQUAL: return T_SLASH;
        case T_DOT_EQUAL: return T_DOT;
        case T_PERCENT_EQUAL: return T_PERCENT;
        case T_POW_EQUAL: return T_POW;
        case T_AMP_EQUAL: return T_AMP;
        case T_PIPE_EQUAL: return T_PIPE;
        case T_CARET_EQUAL: return T_CARET;
        case T_SHIFT_LEFT_EQUAL: return T_SHIFT_LEFT;
        case T_SHIFT_RIGHT_EQUAL: return T_SHIFT_RIGHT;
        default: return type;
    }
}

typedef struct index_lvalue {
    const pc_ast *indices[PPHP_PARSE_DEPTH_MAX];
    uint8_t key_slots[PPHP_PARSE_DEPTH_MAX];
    uint8_t parent_slots[PPHP_PARSE_DEPTH_MAX];
    uint8_t root_slot;
    uint8_t root_object_slot;
    uint8_t root_name_slot;
    const pc_ast *root_member;
    int root_is_static;
    int root_is_dynamic;
    int root_quiet;
    size_t depth;
} index_lvalue;

static void emit_load_slot(generator *gen, uint8_t slot, uint32_t line) {
    emit_byte(gen, OP_LOAD_LOCAL, line);
    emit_byte(gen, slot, line);
}

static void emit_store_slot(generator *gen, uint8_t slot, uint32_t line) {
    emit_byte(gen, OP_STORE_LOCAL, line);
    emit_byte(gen, slot, line);
}

static void clear_temporary_slot(generator *gen, uint8_t slot,
                                 uint32_t line) {
    emit_byte(gen, OP_LOAD_NULL, line);
    emit_store_slot(gen, slot, line);
}

static int prepare_index_lvalue(generator *gen, const pc_ast *leaf,
                                index_lvalue *lvalue, int quiet_root) {
    const pc_ast *reverse[PPHP_PARSE_DEPTH_MAX];
    const pc_ast *cursor = leaf;
    size_t count = 0U;
    size_t i;
    memset(lvalue, 0, sizeof(*lvalue));
    lvalue->root_quiet = quiet_root;
    while (cursor != NULL && cursor->kind == AST_INDEX) {
        if (count >= PPHP_PARSE_DEPTH_MAX) {
            fail(gen, leaf->line, "array assignment nesting exceeds limit");
            return 0;
        }
        reverse[count++] = cursor;
        cursor = cursor->as.index.base;
    }
    if (count == 0U || cursor == NULL) {
        if (!gen->failed) {
            fail(gen, leaf->line,
                 "array assignment must ultimately target a variable");
        }
        return 0;
    }
    if (cursor->kind == AST_VARIABLE) {
        if (!variable_slot(gen, cursor->as.literal.token,
                           &lvalue->root_slot)) return 0;
    } else if (cursor->kind == AST_MEMBER &&
               cursor->as.member.op != T_NULLSAFE_ARROW &&
               (cursor->as.member.op == T_ARROW ||
                (cursor->as.member.op == T_SCOPE &&
                 cursor->as.member.base->kind == AST_IDENTIFIER))) {
        uint16_t name = 0U;
        lvalue->root_member = cursor;
        lvalue->root_is_static = cursor->as.member.op == T_SCOPE;
        lvalue->root_is_dynamic = cursor->as.member.dynamic_name != NULL;
        if (lvalue->root_is_static && lvalue->root_is_dynamic) {
            fail(gen, cursor->line, "dynamic static property is invalid");
            return 0;
        }
        if (!temporary_slot(gen, cursor->line, &lvalue->root_slot)) return 0;
        if (lvalue->root_is_static) {
            uint16_t class_name = name_constant(
                gen, cursor->as.member.base->as.literal.token);
            name = member_name_constant(gen, cursor->as.member.name);
            emit_byte(gen, OP_SPROP_GET, cursor->line);
            emit_u16(gen, class_name, cursor->line);
            emit_u16(gen, name, cursor->line);
        } else {
            if (!temporary_slot(gen, cursor->line,
                                &lvalue->root_object_slot)) return 0;
            compile_expression(gen, cursor->as.member.base);
            emit_store_slot(gen, lvalue->root_object_slot, cursor->line);
            emit_load_slot(gen, lvalue->root_object_slot, cursor->line);
            if (lvalue->root_is_dynamic) {
                if (!temporary_slot(gen, cursor->line,
                                    &lvalue->root_name_slot)) return 0;
                compile_expression(gen, cursor->as.member.dynamic_name);
                emit_store_slot(gen, lvalue->root_name_slot, cursor->line);
                emit_load_slot(gen, lvalue->root_name_slot, cursor->line);
                emit_byte(gen, OP_PROP_GET_DYNAMIC, cursor->line);
            } else {
                name = name_constant(gen, cursor->as.member.name);
                emit_byte(gen, OP_PROP_GET, cursor->line);
                emit_u16(gen, name, cursor->line);
            }
        }
        emit_store_slot(gen, lvalue->root_slot, cursor->line);
    } else {
        fail(gen, leaf->line,
             "array assignment must ultimately target a variable or property");
        return 0;
    }
    lvalue->depth = count;
    for (i = 0U; i < count; i++) {
        const pc_ast *index = reverse[count - i - 1U];
        lvalue->indices[i] = index;
        if (index->as.index.key == NULL) {
            if (i + 1U != count) {
                fail(gen, index->line,
                     "only the final array index may be empty");
                return 0;
            }
        } else {
            if (!temporary_slot(gen, index->line, &lvalue->key_slots[i])) {
                return 0;
            }
            compile_expression(gen, index->as.index.key);
            emit_store_slot(gen, lvalue->key_slots[i], index->line);
        }
        if (i + 1U < count) {
            if (i == 0U) {
                if (lvalue->root_member == NULL && lvalue->root_quiet) {
                    emit_byte(gen, OP_LOAD_LOCAL_QUIET, index->line);
                    emit_byte(gen, lvalue->root_slot, index->line);
                } else {
                    emit_load_slot(gen, lvalue->root_slot, index->line);
                }
            } else {
                emit_load_slot(gen, lvalue->parent_slots[i - 1U],
                               index->line);
            }
            emit_load_slot(gen, lvalue->key_slots[i], index->line);
            emit_byte(gen, OP_IDX_GET_QUIET, index->line);
            if (!temporary_slot(gen, index->line,
                                &lvalue->parent_slots[i])) {
                return 0;
            }
            emit_store_slot(gen, lvalue->parent_slots[i], index->line);
        }
    }
    return !gen->failed;
}

static void emit_index_parent(generator *gen, const index_lvalue *lvalue,
                              size_t index, uint32_t line) {
    uint8_t slot = index == 0U ? lvalue->root_slot
                               : lvalue->parent_slots[index - 1U];
    if (index == 0U && lvalue->root_member == NULL && lvalue->root_quiet) {
        emit_byte(gen, OP_LOAD_LOCAL_QUIET, line);
        emit_byte(gen, slot, line);
    } else {
        emit_load_slot(gen, slot, line);
    }
}

static void emit_index_current(generator *gen, const index_lvalue *lvalue,
                               uint32_t line) {
    size_t leaf = lvalue->depth - 1U;
    emit_index_parent(gen, lvalue, leaf, line);
    emit_load_slot(gen, lvalue->key_slots[leaf], line);
    emit_byte(gen, OP_IDX_GET, line);
}

static void clear_index_lvalue(generator *gen, const index_lvalue *lvalue,
                               uint32_t line) {
    size_t i;
    for (i = 0U; i < lvalue->depth; i++) {
        if (lvalue->indices[i]->as.index.key != NULL) {
            clear_temporary_slot(gen, lvalue->key_slots[i], line);
        }
        if (i + 1U < lvalue->depth) {
            clear_temporary_slot(gen, lvalue->parent_slots[i], line);
        }
    }
    if (lvalue->root_member != NULL) {
        clear_temporary_slot(gen, lvalue->root_slot, line);
        if (!lvalue->root_is_static) {
            clear_temporary_slot(gen, lvalue->root_object_slot, line);
            if (lvalue->root_is_dynamic) {
                clear_temporary_slot(gen, lvalue->root_name_slot, line);
            }
        }
    }
}

static void store_index_root(generator *gen, const index_lvalue *lvalue,
                             uint32_t line) {
    emit_store_slot(gen, lvalue->root_slot, line);
    if (lvalue->root_member != NULL) {
        uint16_t name = 0U;
        if (lvalue->root_is_static) {
            uint16_t class_name = name_constant(
                gen, lvalue->root_member->as.member.base->as.literal.token);
            name = member_name_constant(gen,
                                        lvalue->root_member->as.member.name);
            emit_load_slot(gen, lvalue->root_slot, line);
            emit_byte(gen, OP_SPROP_SET, line);
            emit_u16(gen, class_name, line);
            emit_u16(gen, name, line);
        } else {
            emit_load_slot(gen, lvalue->root_object_slot, line);
            if (lvalue->root_is_dynamic) {
                emit_load_slot(gen, lvalue->root_name_slot, line);
            } else {
                name = name_constant(gen,
                                     lvalue->root_member->as.member.name);
            }
            emit_load_slot(gen, lvalue->root_slot, line);
            emit_byte(gen,
                      lvalue->root_is_dynamic ? OP_PROP_SET_DYNAMIC
                                              : OP_PROP_SET,
                      line);
            if (!lvalue->root_is_dynamic) emit_u16(gen, name, line);
        }
        emit_byte(gen, OP_POP, line);
    }
}

static void propagate_index_update(generator *gen,
                                   const index_lvalue *lvalue,
                                   uint32_t line) {
    size_t index = lvalue->depth;
    uint8_t update_slot;
    if (!temporary_slot(gen, line, &update_slot)) return;
    while (index > 1U && !gen->failed) {
        index--;
        emit_store_slot(gen, update_slot, line);
        emit_index_parent(gen, lvalue, index - 1U, line);
        emit_load_slot(gen, lvalue->key_slots[index - 1U], line);
        emit_load_slot(gen, update_slot, line);
        emit_byte(gen, OP_IDX_SET, line);
        emit_byte(gen, OP_POP, line);
    }
    store_index_root(gen, lvalue, line);
    clear_temporary_slot(gen, update_slot, line);
}

static void store_index_value(generator *gen, const index_lvalue *lvalue,
                              uint8_t value_slot, uint32_t line) {
    size_t leaf = lvalue->depth - 1U;
    emit_index_parent(gen, lvalue, leaf, line);
    if (lvalue->indices[leaf]->as.index.key != NULL) {
        emit_load_slot(gen, lvalue->key_slots[leaf], line);
    }
    emit_load_slot(gen, value_slot, line);
    emit_byte(gen,
              lvalue->indices[leaf]->as.index.key == NULL
                  ? OP_IDX_APPEND : OP_IDX_SET,
              line);
    emit_byte(gen, OP_POP, line);
    propagate_index_update(gen, lvalue, line);
    emit_load_slot(gen, value_slot, line);
}

static void unset_index_value(generator *gen, const index_lvalue *lvalue,
                              uint32_t line) {
    size_t leaf = lvalue->depth - 1U;
    if (lvalue->indices[leaf]->as.index.key == NULL) {
        fail(gen, line, "cannot unset an empty array index");
        return;
    }
    emit_index_parent(gen, lvalue, leaf, line);
    emit_load_slot(gen, lvalue->key_slots[leaf], line);
    emit_byte(gen, OP_IDX_UNSET, line);
    propagate_index_update(gen, lvalue, line);
    emit_byte(gen, OP_LOAD_NULL, line);
    clear_index_lvalue(gen, lvalue, line);
}

static void compile_index_assignment(generator *gen, const pc_ast *node) {
    index_lvalue lvalue;
    pc_token_type operation = node->as.binary.op;
    uint8_t value_slot;
    size_t complete = 0U;
    if (!prepare_index_lvalue(gen, node->as.binary.left, &lvalue,
                              operation == T_EQUAL ||
                                  operation == T_COALESCE_EQUAL) ||
        !temporary_slot(gen, node->line, &value_slot)) {
        return;
    }
    if (operation != T_EQUAL &&
        lvalue.indices[lvalue.depth - 1U]->as.index.key == NULL) {
        fail(gen, node->line, "an empty array index only supports assignment");
        return;
    }
    if (operation == T_COALESCE_EQUAL) {
        size_t leaf = lvalue.depth - 1U;
        emit_index_parent(gen, &lvalue, leaf, node->line);
        emit_load_slot(gen, lvalue.key_slots[leaf], node->line);
        emit_byte(gen, OP_IDX_GET_QUIET, node->line);
        complete = emit_jump(gen, OP_JMP_NOTNULL_KEEP, node->line);
    } else if (operation != T_EQUAL) {
        pphp_opcode opcode = binary_opcode(compound_operator(operation));
        if (opcode == OP_NOP) {
            fail(gen, node->line, "unsupported compound assignment");
            return;
        }
        emit_index_current(gen, &lvalue, node->line);
        compile_expression(gen, node->as.binary.right);
        emit_byte(gen, (uint8_t)opcode, node->line);
        emit_store_slot(gen, value_slot, node->line);
        store_index_value(gen, &lvalue, value_slot, node->line);
        clear_index_lvalue(gen, &lvalue, node->line);
        clear_temporary_slot(gen, value_slot, node->line);
        return;
    }
    compile_expression(gen, node->as.binary.right);
    emit_store_slot(gen, value_slot, node->line);
    store_index_value(gen, &lvalue, value_slot, node->line);
    if (operation == T_COALESCE_EQUAL) {
        patch_jump(gen, complete, gen->proto->code_length, node->line);
    }
    clear_index_lvalue(gen, &lvalue, node->line);
    clear_temporary_slot(gen, value_slot, node->line);
}

static void compile_member_assignment(generator *gen, const pc_ast *node) {
    const pc_ast *member = node->as.binary.left;
    pc_token_type operation = node->as.binary.op;
    uint16_t name;
    uint8_t value_slot;
    uint8_t object_slot = 0U;
    uint8_t name_slot = 0U;
    size_t complete = 0U;
    pphp_opcode opcode = OP_NOP;
    int is_static = member->as.member.op == T_SCOPE;
    int is_dynamic = member->as.member.dynamic_name != NULL;
    if (member->as.member.op == T_NULLSAFE_ARROW) {
        fail(gen, node->line, "nullsafe property access is not assignable");
        return;
    }
    if (!is_static && member->as.member.op != T_ARROW) {
        fail(gen, node->line, "invalid property assignment target");
        return;
    }
    if (is_static && member->as.member.base->kind != AST_IDENTIFIER) {
        fail(gen, node->line, "invalid static property target");
        return;
    }
    if (is_static && is_dynamic) {
        fail(gen, node->line, "dynamic static property target is invalid");
        return;
    }
    if (!temporary_slot(gen, node->line, &value_slot)) return;
    name = is_dynamic ? 0U
                      : (is_static
                             ? member_name_constant(gen, member->as.member.name)
                             : name_constant(gen, member->as.member.name));
    if (!is_static) {
        if (!temporary_slot(gen, node->line, &object_slot)) return;
        compile_expression(gen, member->as.member.base);
        emit_store_slot(gen, object_slot, node->line);
        if (is_dynamic) {
            if (!temporary_slot(gen, node->line, &name_slot)) return;
            compile_expression(gen, member->as.member.dynamic_name);
            emit_store_slot(gen, name_slot, node->line);
        }
    }
    if (operation != T_EQUAL) {
        if (is_static) {
            uint16_t class_name = name_constant(
                gen, member->as.member.base->as.literal.token);
            emit_byte(gen, OP_SPROP_GET, node->line);
            emit_u16(gen, class_name, node->line);
            emit_u16(gen, name, node->line);
        } else {
            emit_load_slot(gen, object_slot, node->line);
            if (is_dynamic) {
                emit_load_slot(gen, name_slot, node->line);
                emit_byte(gen, OP_PROP_GET_DYNAMIC, node->line);
            } else {
                emit_byte(gen, OP_PROP_GET, node->line);
                emit_u16(gen, name, node->line);
            }
        }
        if (operation == T_COALESCE_EQUAL) {
            complete = emit_jump(gen, OP_JMP_NOTNULL_KEEP, node->line);
        } else {
            opcode = binary_opcode(compound_operator(operation));
            if (opcode == OP_NOP) {
                fail(gen, node->line, "unsupported compound assignment");
                return;
            }
        }
    }
    compile_expression(gen, node->as.binary.right);
    if (operation != T_EQUAL && operation != T_COALESCE_EQUAL) {
        emit_byte(gen, (uint8_t)opcode, node->line);
    }
    emit_store_slot(gen, value_slot, node->line);
    if (is_static) {
        uint16_t class_name = name_constant(
            gen, member->as.member.base->as.literal.token);
        emit_load_slot(gen, value_slot, node->line);
        emit_byte(gen, OP_SPROP_SET, node->line);
        emit_u16(gen, class_name, node->line);
        emit_u16(gen, name, node->line);
    } else {
        emit_load_slot(gen, object_slot, node->line);
        if (is_dynamic) emit_load_slot(gen, name_slot, node->line);
        emit_load_slot(gen, value_slot, node->line);
        emit_byte(gen, is_dynamic ? OP_PROP_SET_DYNAMIC : OP_PROP_SET,
                  node->line);
        if (!is_dynamic) emit_u16(gen, name, node->line);
    }
    if (operation == T_COALESCE_EQUAL) {
        patch_jump(gen, complete, gen->proto->code_length, node->line);
    }
    if (!is_static) clear_temporary_slot(gen, object_slot, node->line);
    if (is_dynamic) clear_temporary_slot(gen, name_slot, node->line);
    clear_temporary_slot(gen, value_slot, node->line);
}

static void compile_assignment(generator *gen, const pc_ast *node) {
    uint8_t slot;
    pc_token_type operation = node->as.binary.op;
    if (node->as.binary.left->kind == AST_INDEX) {
        compile_index_assignment(gen, node);
        return;
    }
    if (node->as.binary.left->kind == AST_MEMBER) {
        compile_member_assignment(gen, node);
        return;
    }
    if (node->as.binary.left->kind != AST_VARIABLE) {
        fail(gen, node->line, "assignment target is not executable");
        return;
    }
    if (!variable_slot(gen, node->as.binary.left->as.literal.token, &slot)) {
        return;
    }
    if (operation == T_COALESCE_EQUAL) {
        size_t complete;
        emit_byte(gen, OP_LOAD_LOCAL_QUIET, node->line);
        emit_byte(gen, slot, node->line);
        complete = emit_jump(gen, OP_JMP_NOTNULL_KEEP, node->line);
        compile_expression(gen, node->as.binary.right);
        emit_byte(gen, OP_DUP, node->line);
        emit_store_slot(gen, slot, node->line);
        patch_jump(gen, complete, gen->proto->code_length, node->line);
        return;
    }
    if (operation != T_EQUAL) {
        emit_byte(gen, OP_LOAD_LOCAL, node->line);
        emit_byte(gen, slot, node->line);
    }
    compile_expression(gen, node->as.binary.right);
    if (operation != T_EQUAL) {
        pphp_opcode opcode = binary_opcode(compound_operator(operation));
        if (opcode == OP_NOP) {
            fail(gen, node->line, "unsupported compound assignment");
            return;
        }
        emit_byte(gen, (uint8_t)opcode, node->line);
    }
    emit_byte(gen, OP_DUP, node->line);
    emit_byte(gen, OP_STORE_LOCAL, node->line);
    emit_byte(gen, slot, node->line);
}

static void compile_increment(generator *gen, const pc_ast *node) {
    uint8_t slot;
    if (node->as.unary.operand->kind == AST_INDEX) {
        index_lvalue lvalue;
        uint8_t value_slot;
        uint8_t old_slot = 0U;
        if (!prepare_index_lvalue(gen, node->as.unary.operand, &lvalue, 0) ||
            lvalue.indices[lvalue.depth - 1U]->as.index.key == NULL ||
            !temporary_slot(gen, node->line, &value_slot) ||
            (node->as.unary.postfix &&
             !temporary_slot(gen, node->line, &old_slot))) {
            if (!gen->failed &&
                lvalue.indices[lvalue.depth - 1U]->as.index.key == NULL) {
                fail(gen, node->line, "cannot increment an empty array index");
            }
            return;
        }
        emit_index_current(gen, &lvalue, node->line);
        if (node->as.unary.postfix) {
            emit_byte(gen, OP_DUP, node->line);
            emit_store_slot(gen, old_slot, node->line);
        }
        emit_byte(gen, OP_LOAD_I8, node->line);
        emit_byte(gen, 1U, node->line);
        emit_byte(gen, node->as.unary.op == T_PLUS_PLUS ? OP_ADD : OP_SUB,
                  node->line);
        emit_store_slot(gen, value_slot, node->line);
        store_index_value(gen, &lvalue, value_slot, node->line);
        if (node->as.unary.postfix) {
            emit_byte(gen, OP_POP, node->line);
            emit_load_slot(gen, old_slot, node->line);
        }
        clear_index_lvalue(gen, &lvalue, node->line);
        clear_temporary_slot(gen, value_slot, node->line);
        if (node->as.unary.postfix) {
            clear_temporary_slot(gen, old_slot, node->line);
        }
        return;
    }
    if (node->as.unary.operand->kind == AST_MEMBER) {
        const pc_ast *member = node->as.unary.operand;
        uint8_t object_slot = 0U;
        uint8_t value_slot;
        uint8_t name_slot = 0U;
        uint8_t old_slot = 0U;
        uint16_t name;
        int is_static = member->as.member.op == T_SCOPE;
        int is_dynamic = member->as.member.dynamic_name != NULL;
        if (member->as.member.op == T_NULLSAFE_ARROW ||
            (!is_static && member->as.member.op != T_ARROW) ||
            (is_static && member->as.member.base->kind != AST_IDENTIFIER) ||
            (is_static && is_dynamic) ||
            !temporary_slot(gen, node->line, &value_slot) ||
            (node->as.unary.postfix &&
             !temporary_slot(gen, node->line, &old_slot))) {
            if (!gen->failed) fail(gen, node->line, "invalid property increment");
            return;
        }
        name = is_dynamic ? 0U
                          : (is_static
                                 ? member_name_constant(gen,
                                                        member->as.member.name)
                                 : name_constant(gen,
                                                 member->as.member.name));
        if (is_static) {
            uint16_t class_name = name_constant(
                gen, member->as.member.base->as.literal.token);
            emit_byte(gen, OP_SPROP_GET, node->line);
            emit_u16(gen, class_name, node->line);
            emit_u16(gen, name, node->line);
        } else {
            if (!temporary_slot(gen, node->line, &object_slot)) return;
            compile_expression(gen, member->as.member.base);
            emit_store_slot(gen, object_slot, node->line);
            if (is_dynamic) {
                if (!temporary_slot(gen, node->line, &name_slot)) return;
                compile_expression(gen, member->as.member.dynamic_name);
                emit_store_slot(gen, name_slot, node->line);
            }
            emit_load_slot(gen, object_slot, node->line);
            if (is_dynamic) {
                emit_load_slot(gen, name_slot, node->line);
                emit_byte(gen, OP_PROP_GET_DYNAMIC, node->line);
            } else {
                emit_byte(gen, OP_PROP_GET, node->line);
                emit_u16(gen, name, node->line);
            }
        }
        if (node->as.unary.postfix) {
            emit_byte(gen, OP_DUP, node->line);
            emit_store_slot(gen, old_slot, node->line);
        }
        emit_byte(gen, OP_LOAD_I8, node->line);
        emit_byte(gen, 1U, node->line);
        emit_byte(gen, node->as.unary.op == T_PLUS_PLUS ? OP_ADD : OP_SUB,
                  node->line);
        emit_store_slot(gen, value_slot, node->line);
        if (is_static) {
            uint16_t class_name = name_constant(
                gen, member->as.member.base->as.literal.token);
            emit_load_slot(gen, value_slot, node->line);
            emit_byte(gen, OP_SPROP_SET, node->line);
            emit_u16(gen, class_name, node->line);
            emit_u16(gen, name, node->line);
        } else {
            emit_load_slot(gen, object_slot, node->line);
            if (is_dynamic) emit_load_slot(gen, name_slot, node->line);
            emit_load_slot(gen, value_slot, node->line);
            emit_byte(gen, is_dynamic ? OP_PROP_SET_DYNAMIC : OP_PROP_SET,
                      node->line);
            if (!is_dynamic) emit_u16(gen, name, node->line);
        }
        if (node->as.unary.postfix) {
            emit_byte(gen, OP_POP, node->line);
            emit_load_slot(gen, old_slot, node->line);
        }
        if (!is_static) clear_temporary_slot(gen, object_slot, node->line);
        if (is_dynamic) clear_temporary_slot(gen, name_slot, node->line);
        clear_temporary_slot(gen, value_slot, node->line);
        if (node->as.unary.postfix) {
            clear_temporary_slot(gen, old_slot, node->line);
        }
        return;
    }
    if (node->as.unary.operand->kind != AST_VARIABLE ||
        !variable_slot(gen, node->as.unary.operand->as.literal.token, &slot)) {
        fail(gen, node->line, "increment target is not assignable");
        return;
    }
    emit_byte(gen, OP_LOAD_LOCAL, node->line);
    emit_byte(gen, slot, node->line);
    if (node->as.unary.postfix) {
        emit_byte(gen, OP_DUP, node->line);
    }
    emit_byte(gen, OP_LOAD_I8, node->line);
    emit_byte(gen, 1U, node->line);
    emit_byte(gen, node->as.unary.op == T_PLUS_PLUS ? OP_ADD : OP_SUB, node->line);
    if (!node->as.unary.postfix) {
        emit_byte(gen, OP_DUP, node->line);
    }
    emit_byte(gen, OP_STORE_LOCAL, node->line);
    emit_byte(gen, slot, node->line);
}

static void compile_short_circuit(generator *gen, const pc_ast *node) {
    pphp_opcode opcode;
    size_t jump;
    if (node->as.binary.op == T_COALESCE) {
        compile_quiet_expression(gen, node->as.binary.left);
        opcode = OP_JMP_NOTNULL_KEEP;
    } else if (node->as.binary.op == T_BOOL_OR || node->as.binary.op == T_OR) {
        compile_expression(gen, node->as.binary.left);
        opcode = OP_JMP_IF_KEEP;
    } else {
        compile_expression(gen, node->as.binary.left);
        opcode = OP_JMP_UNLESS_KEEP;
    }
    jump = emit_jump(gen, opcode, node->line);
    compile_expression(gen, node->as.binary.right);
    patch_jump(gen, jump, gen->proto->code_length, node->line);
}

static int argument_list_has_spread(const pc_ast *argument) {
    while (argument != NULL) {
        if (argument->kind == AST_UNARY &&
            argument->as.unary.op == T_ELLIPSIS) {
            return 1;
        }
        argument = argument->next;
    }
    return 0;
}

static int token_is_array_mutator(pc_token token) {
    static const char names[] =
        "array_pop\0array_push\0array_shift\0array_unshift\0"
        "arsort\0asort\0krsort\0ksort\0rsort\0sort\0"
        "uasort\0uksort\0usort\0";
    const char *name = names;
    while (*name != '\0') {
        size_t length = strlen(name);
        size_t i;
        int equal = length == token.length;
        for (i = 0U; equal && i < length; i++) {
            unsigned char left = (unsigned char)token.start[i];
            unsigned char right = (unsigned char)name[i];
            if (left >= 'A' && left <= 'Z') {
                left = (unsigned char)(left + ('a' - 'A'));
            }
            equal = left == right;
        }
        if (equal) return 1;
        name += length + 1U;
    }
    return 0;
}

static void compile_argument_array(generator *gen, const pc_ast *argument,
                                   size_t count, uint32_t line) {
    emit_byte(gen, OP_NEW_ARRAY, line);
    emit_u16(gen, (uint16_t)count, line);
    while (argument != NULL && !gen->failed) {
        if (argument->kind == AST_UNARY &&
            argument->as.unary.op == T_ELLIPSIS) {
            compile_expression(gen, argument->as.unary.operand);
            emit_byte(gen, OP_ARR_EXTEND, argument->line);
        } else {
            compile_expression(gen, argument);
            emit_byte(gen, OP_ARR_PUSH, argument->line);
        }
        argument = argument->next;
    }
}

typedef struct access_chain_jumps {
    size_t operands[PPHP_PARSE_DEPTH_MAX];
    uint32_t lines[PPHP_PARSE_DEPTH_MAX];
    size_t count;
    int quiet;
} access_chain_jumps;

static int access_chain_contains_nullsafe(const pc_ast *node) {
    if (node == NULL) return 0;
    if (node->kind == AST_MEMBER &&
        (node->as.member.op == T_ARROW ||
         node->as.member.op == T_NULLSAFE_ARROW)) {
        return node->as.member.op == T_NULLSAFE_ARROW ||
               access_chain_contains_nullsafe(node->as.member.base);
    }
    if (node->kind == AST_INDEX) {
        return access_chain_contains_nullsafe(node->as.index.base);
    }
    if (node->kind == AST_CALL && node->as.call.callee != NULL &&
        node->as.call.callee->kind == AST_MEMBER &&
        (node->as.call.callee->as.member.op == T_ARROW ||
         node->as.call.callee->as.member.op == T_NULLSAFE_ARROW)) {
        return node->as.call.callee->as.member.op == T_NULLSAFE_ARROW ||
               access_chain_contains_nullsafe(
                   node->as.call.callee->as.member.base);
    }
    return 0;
}

static int access_base_can_inherit_quiet(const pc_ast *base,
                                         pc_token_type op) {
    if (access_chain_contains_nullsafe(base)) return 1;
    /* Coalesce quiets a directly-nullsafe variable, but not a normal prefix
     * that must be evaluated before reaching the nullsafe boundary. */
    return op == T_NULLSAFE_ARROW && base != NULL &&
           base->kind == AST_VARIABLE;
}

static int add_access_chain_jump(generator *gen, access_chain_jumps *jumps,
                                 uint32_t line) {
    if (jumps->count >= PPHP_PARSE_DEPTH_MAX) {
        fail(gen, line, "nullsafe access nesting exceeds limit");
        return 0;
    }
    jumps->operands[jumps->count] =
        emit_jump(gen, OP_JMP_IFNULL_KEEP, line);
    jumps->lines[jumps->count] = line;
    jumps->count++;
    return 1;
}

static void compile_access_chain_part(generator *gen, const pc_ast *node,
                                      access_chain_jumps *jumps) {
    if (node == NULL || gen->failed) return;
    if (jumps->quiet && node->kind == AST_VARIABLE) {
        uint8_t slot;
        if (variable_slot(gen, node->as.literal.token, &slot)) {
            emit_byte(gen, OP_LOAD_LOCAL_QUIET, node->line);
            emit_byte(gen, slot, node->line);
        }
        return;
    }
    if (node->kind == AST_CALL &&
        node->as.call.callee->kind == AST_MEMBER &&
        (node->as.call.callee->as.member.op == T_ARROW ||
         node->as.call.callee->as.member.op == T_NULLSAFE_ARROW)) {
        const pc_ast *member = node->as.call.callee;
        const pc_ast *argument = node->as.call.arguments;
        uint16_t method_name;
        int has_spread = argument_list_has_spread(argument);
        int saved_quiet = jumps->quiet;
        if (node->as.call.count > 31U) {
            fail(gen, node->line, "a call may have at most 31 arguments");
            return;
        }
        if (jumps->quiet &&
            !access_base_can_inherit_quiet(member->as.member.base,
                                           member->as.member.op)) {
            jumps->quiet = 0;
        }
        compile_access_chain_part(gen, member->as.member.base, jumps);
        jumps->quiet = saved_quiet;
        if (member->as.member.op == T_NULLSAFE_ARROW &&
            !add_access_chain_jump(gen, jumps, node->line)) {
            return;
        }
        method_name = member->as.member.dynamic_name == NULL
                          ? name_constant(gen, member->as.member.name)
                          : 0U;
        if (member->as.member.dynamic_name != NULL) {
            compile_expression(gen, member->as.member.dynamic_name);
        }
        if (has_spread) {
            compile_argument_array(gen, argument, node->as.call.count,
                                   node->line);
            if (member->as.member.dynamic_name != NULL) {
                emit_byte(gen, OP_MCALL_DYNAMIC_ARRAY, node->line);
            } else {
                emit_byte(gen, OP_MCALL_ARRAY, node->line);
                emit_u16(gen, method_name, node->line);
            }
        } else {
            while (argument != NULL) {
                compile_expression(gen, argument);
                argument = argument->next;
            }
            if (member->as.member.dynamic_name != NULL) {
                emit_byte(gen, OP_MCALL_DYNAMIC, node->line);
                emit_byte(gen, (uint8_t)node->as.call.count, node->line);
            } else {
                emit_byte(gen, OP_MCALL, node->line);
                emit_u16(gen, method_name, node->line);
                emit_byte(gen, (uint8_t)node->as.call.count, node->line);
            }
        }
        return;
    }
    if (node->kind == AST_INDEX) {
        if (node->as.index.key == NULL) {
            fail(gen, node->line, "cannot read from an empty array index");
            return;
        }
        compile_access_chain_part(gen, node->as.index.base, jumps);
        compile_expression(gen, node->as.index.key);
        emit_byte(gen, jumps->quiet ? OP_IDX_GET_QUIET : OP_IDX_GET,
                  node->line);
        return;
    }
    if (node->kind == AST_MEMBER &&
        (node->as.member.op == T_ARROW ||
         node->as.member.op == T_NULLSAFE_ARROW)) {
        int saved_quiet = jumps->quiet;
        if (node->as.member.op == T_NULLSAFE_ARROW && jumps->quiet &&
            !access_base_can_inherit_quiet(node->as.member.base,
                                           node->as.member.op)) {
            jumps->quiet = 0;
        }
        compile_access_chain_part(gen, node->as.member.base, jumps);
        jumps->quiet = saved_quiet;
        if (node->as.member.op == T_NULLSAFE_ARROW) {
            if (!add_access_chain_jump(gen, jumps, node->line)) return;
        }
        if (node->as.member.dynamic_name != NULL) {
            compile_expression(gen, node->as.member.dynamic_name);
            emit_byte(gen, jumps->quiet ? OP_PROP_GET_DYNAMIC_QUIET
                                        : OP_PROP_GET_DYNAMIC,
                      node->line);
        } else {
            uint16_t name = name_constant(gen, node->as.member.name);
            emit_byte(gen, jumps->quiet ? OP_PROP_GET_QUIET : OP_PROP_GET,
                      node->line);
            emit_u16(gen, name, node->line);
        }
        return;
    }
    compile_expression(gen, node);
}

static void compile_access_chain(generator *gen, const pc_ast *node,
                                 int quiet) {
    access_chain_jumps jumps;
    size_t i;
    memset(&jumps, 0, sizeof(jumps));
    jumps.quiet = quiet;
    compile_access_chain_part(gen, node, &jumps);
    if (gen->failed) return;
    for (i = 0U; i < jumps.count; i++) {
        patch_jump(gen, jumps.operands[i], gen->proto->code_length,
                   jumps.lines[i]);
    }
}

static void compile_quiet_expression(generator *gen, const pc_ast *node) {
    compile_access_chain(gen, node, 1);
}

static void compile_expression(generator *gen, const pc_ast *node) {
    if (node == NULL || gen->failed) {
        return;
    }
    switch (node->kind) {
        case AST_NULL:
            emit_byte(gen, OP_LOAD_NULL, node->line);
            break;
        case AST_BOOL:
            emit_byte(gen, node->as.literal.token.type == T_TRUE ? OP_LOAD_TRUE : OP_LOAD_FALSE,
                      node->line);
            break;
        case AST_INT: {
            int overflow;
            uint64_t magnitude = parse_integer_token(node->as.literal.token,
                                                     &overflow);
            if (overflow) {
#if PPHP_ENABLE_FLOAT
                emit_constant(gen, pv_float(parse_integer_float_token(
                                  node->as.literal.token)), node->line);
#else
                fail(gen, node->line, "integer literal is out of range");
#endif
            } else if (magnitude <= INT8_MAX) {
                emit_byte(gen, OP_LOAD_I8, node->line);
                emit_byte(gen, (uint8_t)magnitude, node->line);
            } else if (magnitude <= INT32_MAX) {
                emit_byte(gen, OP_LOAD_I32, node->line);
                emit_i32(gen, (int32_t)magnitude, node->line);
            } else if (magnitude <= (uint64_t)PPHP_INT_MAXIMUM) {
                emit_constant(gen, pv_int((pphp_int)magnitude), node->line);
            } else {
#if PPHP_ENABLE_FLOAT
                emit_constant(gen, pv_float(parse_integer_float_token(
                                  node->as.literal.token)), node->line);
#elif PPHP_INT64
                fail(gen, node->line, "integer literal is out of range");
#else
                fail(gen, node->line, "integer literal requires float support");
#endif
            }
            break;
        }
        case AST_FLOAT:
#if PPHP_ENABLE_FLOAT
            emit_constant(gen, pv_float(parse_float_token(node->as.literal.token)), node->line);
#else
            fail(gen, node->line, "float support disabled");
#endif
            break;
        case AST_STRING: {
            pstring *string = decode_string(node->as.literal.token);
            pvalue value;
            if (string == NULL) {
                fail(gen, node->line, "out of memory while decoding string");
                break;
            }
            value = pv_heap(PT_STRING, &string->header);
            emit_constant(gen, value, node->line);
            pv_release(value);
            break;
        }
        case AST_IDENTIFIER: {
            uint16_t name = name_constant(gen, node->as.literal.token);
            emit_byte(gen, OP_LOAD_NAMED_CONST, node->line);
            emit_u16(gen, name, node->line);
            break;
        }
        case AST_VARIABLE: {
            uint8_t slot;
            if (variable_slot(gen, node->as.literal.token, &slot)) {
                emit_byte(gen, OP_LOAD_LOCAL, node->line);
                emit_byte(gen, slot, node->line);
            }
            break;
        }
        case AST_ASSIGN:
            compile_assignment(gen, node);
            break;
        case AST_BINARY:
            if (node->as.binary.op == T_BOOL_AND || node->as.binary.op == T_BOOL_OR ||
                node->as.binary.op == T_AND || node->as.binary.op == T_OR ||
                node->as.binary.op == T_COALESCE) {
                compile_short_circuit(gen, node);
            } else if (node->as.binary.op == T_XOR) {
                compile_expression(gen, node->as.binary.left);
                emit_byte(gen, OP_NOT, node->line);
                emit_byte(gen, OP_NOT, node->line);
                compile_expression(gen, node->as.binary.right);
                emit_byte(gen, OP_NOT, node->line);
                emit_byte(gen, OP_NOT, node->line);
                emit_byte(gen, OP_BXOR, node->line);
            } else if (node->as.binary.op == T_INSTANCEOF) {
                compile_expression(gen, node->as.binary.left);
                if (node->as.binary.right->kind == AST_IDENTIFIER) {
                    uint16_t class_name = name_constant(
                        gen, node->as.binary.right->as.literal.token);
                    emit_byte(gen, OP_INSTANCEOF, node->line);
                    emit_u16(gen, class_name, node->line);
                } else {
                    compile_expression(gen, node->as.binary.right);
                    emit_byte(gen, OP_INSTANCEOF_DYNAMIC, node->line);
                }
            } else {
                pphp_opcode opcode = binary_opcode(node->as.binary.op);
                if (opcode == OP_NOP) {
                    fail(gen, node->line, "unsupported binary operator %s",
                         pc_token_name(node->as.binary.op));
                    break;
                }
                compile_expression(gen, node->as.binary.left);
                compile_expression(gen, node->as.binary.right);
                emit_byte(gen, (uint8_t)opcode, node->line);
            }
            break;
        case AST_UNARY:
            if (node->as.unary.op == T_PLUS_PLUS || node->as.unary.op == T_MINUS_MINUS) {
                compile_increment(gen, node);
                break;
            }
#if !PPHP_ENABLE_FLOAT
            if (node->as.unary.op == T_FLOAT_TYPE) {
                fail(gen, node->line, "float support disabled");
                break;
            }
#endif
            if (node->as.unary.op == T_MINUS &&
                node->as.unary.operand != NULL &&
                node->as.unary.operand->kind == AST_INT) {
                int overflow;
                uint64_t magnitude = parse_integer_token(
                    node->as.unary.operand->as.literal.token, &overflow);
                if (!overflow &&
                    magnitude == (uint64_t)PPHP_INT_MAXIMUM + 1U) {
                    emit_constant(gen, pv_int(PPHP_INT_MINIMUM), node->line);
                    break;
                }
            }
            compile_expression(gen, node->as.unary.operand);
            if (node->as.unary.op == T_MINUS) emit_byte(gen, OP_NEG, node->line);
            else if (node->as.unary.op == T_PLUS) {
                emit_byte(gen, OP_LOAD_I8, node->line);
                emit_byte(gen, 0U, node->line);
                emit_byte(gen, OP_ADD, node->line);
            }
            else if (node->as.unary.op == T_BANG) emit_byte(gen, OP_NOT, node->line);
            else if (node->as.unary.op == T_TILDE) emit_byte(gen, OP_BNOT, node->line);
            else if (node->as.unary.op == T_CLONE) emit_byte(gen, OP_CLONE, node->line);
            else if (node->as.unary.op == T_INT_TYPE ||
                     node->as.unary.op == T_FLOAT_TYPE ||
                     node->as.unary.op == T_STRING_TYPE ||
                     node->as.unary.op == T_BOOL_TYPE ||
                     node->as.unary.op == T_ARRAY) {
                uint8_t target = node->as.unary.op == T_INT_TYPE ? PT_INT :
                                 (node->as.unary.op == T_FLOAT_TYPE ? PT_FLOAT :
                                  (node->as.unary.op == T_STRING_TYPE ? PT_STRING :
                                   (node->as.unary.op == T_BOOL_TYPE ? PT_TRUE :
                                                                    PT_ARRAY)));
                emit_byte(gen, OP_CAST, node->line);
                emit_byte(gen, target, node->line);
            }
            else {
                fail(gen, node->line, "unsupported unary operator");
            }
            break;
        case AST_TERNARY:
            if (node->as.ternary.then_expr == NULL) {
                size_t end;
                compile_expression(gen, node->as.ternary.condition);
                end = emit_jump(gen, OP_JMP_IF_KEEP, node->line);
                compile_expression(gen, node->as.ternary.else_expr);
                patch_jump(gen, end, gen->proto->code_length, node->line);
            } else {
                size_t otherwise;
                size_t end;
                compile_expression(gen, node->as.ternary.condition);
                otherwise = emit_jump(gen, OP_JMP_UNLESS, node->line);
                compile_expression(gen, node->as.ternary.then_expr);
                end = emit_jump(gen, OP_JMP, node->line);
                patch_jump(gen, otherwise, gen->proto->code_length, node->line);
                compile_expression(gen, node->as.ternary.else_expr);
                patch_jump(gen, end, gen->proto->code_length, node->line);
            }
            break;
        case AST_CLOSURE:
            compile_closure_expression(gen, node);
            break;
        case AST_MATCH: {
            const pc_ast *arm = node->as.match_expr.arms;
            size_t condition_jumps[256];
            size_t condition_count = 0U;
            size_t condition_index = 0U;
            size_t end_jumps[256];
            size_t end_count = 0U;
            size_t no_match;
            size_t default_target = SIZE_MAX;
            compile_expression(gen, node->as.match_expr.subject);
            while (arm != NULL && !gen->failed) {
                const pc_ast *condition = arm->as.match_arm.conditions;
                while (condition != NULL) {
                    if (condition_count >=
                        sizeof(condition_jumps) / sizeof(condition_jumps[0])) {
                        fail(gen, node->line, "too many match conditions");
                        break;
                    }
                    emit_byte(gen, OP_DUP, condition->line);
                    compile_expression(gen, condition);
                    emit_byte(gen, OP_IDENT, condition->line);
                    condition_jumps[condition_count++] = emit_jump(
                        gen, OP_JMP_IF, condition->line);
                    condition = condition->next;
                }
                arm = arm->next;
            }
            no_match = emit_jump(gen, OP_JMP, node->line);
            arm = node->as.match_expr.arms;
            while (arm != NULL && !gen->failed) {
                const pc_ast *condition = arm->as.match_arm.conditions;
                size_t handler = gen->proto->code_length;
                if (condition == NULL) {
                    default_target = handler;
                } else {
                    while (condition != NULL) {
                        patch_jump(gen, condition_jumps[condition_index++], handler,
                                   condition->line);
                        condition = condition->next;
                    }
                }
                emit_byte(gen, OP_POP, arm->line);
                compile_expression(gen, arm->as.match_arm.result);
                if (end_count >= sizeof(end_jumps) / sizeof(end_jumps[0])) {
                    fail(gen, node->line, "too many match arms");
                    break;
                }
                end_jumps[end_count++] = emit_jump(gen, OP_JMP, arm->line);
                arm = arm->next;
            }
            if (default_target == SIZE_MAX && !gen->failed) {
                pc_token class_token;
                uint16_t class_name;
                memset(&class_token, 0, sizeof(class_token));
                class_token.type = T_IDENTIFIER;
                class_token.start = "UnhandledMatchError";
                class_token.length = 19U;
                class_token.line = node->line;
                patch_jump(gen, no_match, gen->proto->code_length, node->line);
                emit_byte(gen, OP_POP, node->line);
                class_name = name_constant(gen, class_token);
                emit_byte(gen, OP_NEW_OBJ, node->line);
                emit_u16(gen, class_name, node->line);
                emit_byte(gen, 0U, node->line);
                emit_byte(gen, OP_THROW, node->line);
            } else if (!gen->failed) {
                patch_jump(gen, no_match, default_target, node->line);
            }
            while (end_count > 0U) {
                end_count--;
                patch_jump(gen, end_jumps[end_count], gen->proto->code_length,
                           node->line);
            }
            break;
        }
        case AST_CALL: {
            const pc_ast *argument = node->as.call.arguments;
            pstring *name;
            pvalue name_value;
            uint16_t name_index;
            int has_spread = argument_list_has_spread(argument);
            int separated_array = 0;
            if (node->as.call.count > 31U) {
                fail(gen, node->line, "a call may have at most 31 arguments");
                break;
            }
            if (node->as.call.callee->kind == AST_MEMBER &&
                (node->as.call.callee->as.member.op == T_ARROW ||
                 node->as.call.callee->as.member.op == T_NULLSAFE_ARROW)) {
                compile_access_chain(gen, node, 0);
                break;
            }
            if (node->as.call.callee->kind == AST_MEMBER &&
                node->as.call.callee->as.member.op == T_SCOPE) {
                const pc_ast *member = node->as.call.callee;
                uint16_t class_name;
                uint16_t method_name;
                if (member->as.member.base->kind != AST_IDENTIFIER) {
                    fail(gen, node->line, "invalid static method target");
                    break;
                }
                class_name = name_constant(
                    gen, member->as.member.base->as.literal.token);
                method_name = member_name_constant(gen, member->as.member.name);
                if (has_spread) {
                    compile_argument_array(gen, argument, node->as.call.count,
                                           node->line);
                    emit_byte(gen, OP_SCALL_ARRAY, node->line);
                    emit_u16(gen, class_name, node->line);
                    emit_u16(gen, method_name, node->line);
                } else {
                    while (argument != NULL) {
                        compile_expression(gen, argument);
                        argument = argument->next;
                    }
                    emit_byte(gen, OP_SCALL, node->line);
                    emit_u16(gen, class_name, node->line);
                    emit_u16(gen, method_name, node->line);
                    emit_byte(gen, (uint8_t)node->as.call.count, node->line);
                }
                break;
            }
            if (node->as.call.callee->kind != AST_IDENTIFIER) {
                compile_expression(gen, node->as.call.callee);
                if (has_spread) {
                    compile_argument_array(gen, argument, node->as.call.count,
                                           node->line);
                    emit_byte(gen, OP_CALL_VALUE_ARRAY, node->line);
                } else {
                    while (argument != NULL) {
                        compile_expression(gen, argument);
                        argument = argument->next;
                    }
                    emit_byte(gen, OP_CALL_VALUE, node->line);
                    emit_byte(gen, (uint8_t)node->as.call.count, node->line);
                }
                break;
            }
            if (!has_spread && argument != NULL &&
                token_is_array_mutator(
                    node->as.call.callee->as.literal.token)) {
                uint8_t slot;
                if (argument->kind == AST_VARIABLE &&
                    variable_slot(gen, argument->as.literal.token, &slot)) {
                    emit_byte(gen, OP_LOAD_LOCAL, node->line);
                    emit_byte(gen, slot, node->line);
                    emit_byte(gen, OP_ARR_SEPARATE, node->line);
                    emit_byte(gen, OP_DUP, node->line);
                    emit_byte(gen, OP_STORE_LOCAL, node->line);
                    emit_byte(gen, slot, node->line);
                } else if (argument->kind == AST_MEMBER &&
                           argument->as.member.op != T_NULLSAFE_ARROW &&
                           argument->as.member.dynamic_name == NULL &&
                           (argument->as.member.op == T_ARROW ||
                            (argument->as.member.op == T_SCOPE &&
                             argument->as.member.base->kind ==
                                 AST_IDENTIFIER))) {
                    uint8_t array_slot;
                    uint8_t object_slot = 0U;
                    uint16_t member_name;
                    int is_static = argument->as.member.op == T_SCOPE;
                    if (!is_static &&
                        argument->as.member.name.type == T_VARIABLE) {
                        fail(gen, node->line,
                             "dynamic property mutation is not supported");
                        break;
                    }
                    if (!temporary_slot(gen, node->line, &array_slot)) break;
                    member_name = is_static
                                      ? member_name_constant(
                                            gen, argument->as.member.name)
                                      : name_constant(gen,
                                                      argument->as.member.name);
                    if (is_static) {
                        uint16_t class_name = name_constant(
                            gen, argument->as.member.base->as.literal.token);
                        emit_byte(gen, OP_SPROP_GET, node->line);
                        emit_u16(gen, class_name, node->line);
                        emit_u16(gen, member_name, node->line);
                    } else {
                        if (!temporary_slot(gen, node->line, &object_slot)) break;
                        compile_expression(gen, argument->as.member.base);
                        emit_store_slot(gen, object_slot, node->line);
                        emit_load_slot(gen, object_slot, node->line);
                        emit_byte(gen, OP_PROP_GET, node->line);
                        emit_u16(gen, member_name, node->line);
                    }
                    emit_byte(gen, OP_ARR_SEPARATE, node->line);
                    emit_store_slot(gen, array_slot, node->line);
                    if (is_static) {
                        uint16_t class_name = name_constant(
                            gen, argument->as.member.base->as.literal.token);
                        emit_load_slot(gen, array_slot, node->line);
                        emit_byte(gen, OP_SPROP_SET, node->line);
                        emit_u16(gen, class_name, node->line);
                        emit_u16(gen, member_name, node->line);
                    } else {
                        emit_load_slot(gen, object_slot, node->line);
                        emit_load_slot(gen, array_slot, node->line);
                        emit_byte(gen, OP_PROP_SET, node->line);
                        emit_u16(gen, member_name, node->line);
                    }
                    emit_byte(gen, OP_POP, node->line);
                    emit_load_slot(gen, array_slot, node->line);
                    clear_temporary_slot(gen, array_slot, node->line);
                    if (!is_static) {
                        clear_temporary_slot(gen, object_slot, node->line);
                    }
                } else {
                    fail(gen, node->line,
                         "array mutation target must be a variable or property");
                    break;
                }
                argument = argument->next;
                separated_array = 1;
            }
            if (has_spread) {
                compile_argument_array(gen, argument, node->as.call.count,
                                       node->line);
            } else {
                while (argument != NULL) {
                    compile_expression(gen, argument);
                    argument = argument->next;
                }
            }
            name = ps_new(node->as.call.callee->as.literal.token.start,
                          node->as.call.callee->as.literal.token.length);
            if (name == NULL) {
                fail(gen, node->line, "out of memory for function name");
                break;
            }
            name_value = pv_heap(PT_STRING, &name->header);
            if (!pproto_add_constant(gen->proto, name_value, &name_index)) {
                fail(gen, node->line, "constant pool limit exceeded");
            }
            pv_release(name_value);
            emit_byte(gen, has_spread ? OP_CALL_ARRAY : OP_CALL, node->line);
            emit_u16(gen, name_index, node->line);
            if (!has_spread) {
                emit_byte(gen, (uint8_t)node->as.call.count, node->line);
            }
            (void)separated_array;
            break;
        }
        case AST_MEMBER: {
            if (node->as.member.op == T_SCOPE) {
                uint16_t class_name;
                uint16_t name;
                if (node->as.member.base->kind != AST_IDENTIFIER) {
                    fail(gen, node->line, "invalid static member target");
                    break;
                }
                class_name = name_constant(
                    gen, node->as.member.base->as.literal.token);
                name = member_name_constant(gen, node->as.member.name);
                emit_byte(gen, node->as.member.name.type == T_VARIABLE
                                   ? OP_SPROP_GET : OP_CLSCONST,
                          node->line);
                emit_u16(gen, class_name, node->line);
                emit_u16(gen, name, node->line);
                break;
            }
            if (node->as.member.op != T_ARROW &&
                node->as.member.op != T_NULLSAFE_ARROW) {
                fail(gen, node->line, "invalid member access");
                break;
            }
            compile_access_chain(gen, node, 0);
            break;
        }
        case AST_NEW: {
            const pc_ast *argument = node->as.new_expr.arguments;
            if (node->as.new_expr.class_name == NULL ||
                (node->as.new_expr.class_name->kind != AST_IDENTIFIER &&
                 node->as.new_expr.class_name->kind != AST_VARIABLE) ||
                node->as.new_expr.count > 31U) {
                fail(gen, node->line, "invalid class construction");
                break;
            }
            if (node->as.new_expr.class_name->kind == AST_VARIABLE) {
                compile_expression(gen, node->as.new_expr.class_name);
            }
            if (argument_list_has_spread(argument)) {
                compile_argument_array(gen, argument, node->as.new_expr.count,
                                       node->line);
                if (node->as.new_expr.class_name->kind == AST_VARIABLE) {
                    emit_byte(gen, OP_NEW_OBJ_DYNAMIC_ARRAY, node->line);
                } else {
                    uint16_t class_name = name_constant(
                        gen,
                        node->as.new_expr.class_name->as.literal.token);
                    emit_byte(gen, OP_NEW_OBJ_ARRAY, node->line);
                    emit_u16(gen, class_name, node->line);
                }
            } else {
                while (argument != NULL) {
                    compile_expression(gen, argument);
                    argument = argument->next;
                }
                if (node->as.new_expr.class_name->kind == AST_VARIABLE) {
                    emit_byte(gen, OP_NEW_OBJ_DYNAMIC, node->line);
                    emit_byte(gen, (uint8_t)node->as.new_expr.count,
                              node->line);
                } else {
                    uint16_t class_name = name_constant(
                        gen,
                        node->as.new_expr.class_name->as.literal.token);
                    emit_byte(gen, OP_NEW_OBJ, node->line);
                    emit_u16(gen, class_name, node->line);
                    emit_byte(gen, (uint8_t)node->as.new_expr.count,
                              node->line);
                }
            }
            break;
        }
        case AST_ARRAY: {
            const pc_ast *item = node->as.list.items;
            emit_byte(gen, OP_NEW_ARRAY, node->line);
            emit_u16(gen, (uint16_t)node->as.list.count, node->line);
            while (item != NULL) {
                if (item->as.array_item.spread) {
                    compile_expression(gen, item->as.array_item.value);
                    emit_byte(gen, OP_ARR_EXTEND, item->line);
                } else if (item->as.array_item.key != NULL) {
                    compile_expression(gen, item->as.array_item.key);
                    compile_expression(gen, item->as.array_item.value);
                    emit_byte(gen, OP_ARR_SET, item->line);
                } else {
                    compile_expression(gen, item->as.array_item.value);
                    emit_byte(gen, OP_ARR_PUSH, item->line);
                }
                item = item->next;
            }
            break;
        }
        case AST_INDEX:
            compile_access_chain(gen, node, 0);
            break;
        case AST_ISSET:
            compile_quiet_expression(gen, node->as.unary.operand);
            emit_byte(gen, OP_LOAD_NULL, node->line);
            emit_byte(gen, OP_NIDENT, node->line);
            break;
        case AST_EMPTY:
            compile_quiet_expression(gen, node->as.unary.operand);
            emit_byte(gen, OP_NOT, node->line);
            break;
        case AST_UNSET:
            if (node->as.unary.operand->kind == AST_VARIABLE) {
                uint8_t slot;
                if (variable_slot(gen, node->as.unary.operand->as.literal.token, &slot)) {
                    emit_byte(gen, OP_UNSET_LOCAL, node->line);
                    emit_byte(gen, slot, node->line);
                    emit_byte(gen, OP_LOAD_NULL, node->line);
                }
            } else if (node->as.unary.operand->kind == AST_INDEX) {
                index_lvalue lvalue;
                if (prepare_index_lvalue(gen, node->as.unary.operand,
                                         &lvalue, 1)) {
                    unset_index_value(gen, &lvalue, node->line);
                }
            } else {
                fail(gen, node->line, "unset target is not supported");
            }
            break;
        case AST_THROW:
            compile_expression(gen, node->as.expression.expression);
            emit_byte(gen, OP_THROW, node->line);
            break;
        default:
            fail(gen, node->line, "AST node %s is not executable yet",
                 pc_ast_kind_name(node->kind));
            break;
    }
}

static void record_loop_jump(generator *gen, const pc_ast *node, int is_continue) {
    unsigned level = node->as.jump.level;
    unsigned target;
    unsigned current;
    size_t operand;
    jump_record *record;
    if (level == 0U || level > gen->loop_count) {
        fail(gen, node->line, "%s %u is outside a loop",
             is_continue ? "continue" : "break", level);
        return;
    }
    if (gen->jump_count >= sizeof(gen->jumps) / sizeof(gen->jumps[0])) {
        fail(gen, node->line, "too many loop exits in one function");
        return;
    }
    target = gen->loop_count - level;
    compile_transfer_finally_blocks(gen, gen->loops[target].finally_count);
    current = gen->loop_count;
    while (current > target +
           ((is_continue && !gen->loops[target].is_switch) ? 1U : 0U)) {
        current--;
        if (gen->loops[current].is_foreach) {
            emit_byte(gen, OP_FE_FREE, node->line);
        } else if (gen->loops[current].is_switch) {
            emit_byte(gen, OP_POP, node->line);
        }
    }
    operand = emit_jump(gen, OP_JMP, node->line);
    record = &gen->jumps[gen->jump_count++];
    record->operand = operand;
    record->loop_index = target;
    record->is_continue = is_continue;
    record->patched = 0;
}

static void patch_loop_jumps(generator *gen, unsigned index, size_t break_target,
                             size_t continue_target, uint32_t line) {
    size_t i;
    for (i = 0U; i < gen->jump_count; i++) {
        if (!gen->jumps[i].patched && gen->jumps[i].loop_index == index) {
            patch_jump(gen, gen->jumps[i].operand,
                       gen->jumps[i].is_continue &&
                               !gen->loops[index].is_switch
                           ? continue_target
                           : break_target,
                       line);
            gen->jumps[i].patched = 1;
        }
    }
}

static void compile_statement_list(generator *gen, const pc_ast *node) {
    while (node != NULL && !gen->failed) {
        compile_statement(gen, node);
        node = node->next;
    }
}

static void compile_statement(generator *gen, const pc_ast *node) {
    if (node == NULL || gen->failed) {
        return;
    }
#if PPHP_LINE_INFO
    emit_byte(gen, OP_LINE, node->line);
    emit_u16(gen, (uint16_t)(node->line > UINT16_MAX ? UINT16_MAX : node->line), node->line);
#endif
    switch (node->kind) {
        case AST_PROGRAM:
        case AST_BLOCK:
            compile_statement_list(gen, node->as.list.items);
            break;
        case AST_EXPR_STMT:
            compile_expression(gen, node->as.expression.expression);
            emit_byte(gen, OP_POP, node->line);
            break;
        case AST_ECHO: {
            const pc_ast *expression = node->as.list.items;
            while (expression != NULL) {
                compile_expression(gen, expression);
                expression = expression->next;
            }
            emit_byte(gen, OP_ECHO, node->line);
            emit_byte(gen, (uint8_t)node->as.list.count, node->line);
            break;
        }
        case AST_IF: {
            size_t otherwise;
            size_t end = SIZE_MAX;
            compile_expression(gen, node->as.if_stmt.condition);
            otherwise = emit_jump(gen, OP_JMP_UNLESS, node->line);
            compile_statement(gen, node->as.if_stmt.then_branch);
            if (node->as.if_stmt.else_branch != NULL) {
                end = emit_jump(gen, OP_JMP, node->line);
            }
            patch_jump(gen, otherwise, gen->proto->code_length, node->line);
            if (node->as.if_stmt.else_branch != NULL) {
                compile_statement(gen, node->as.if_stmt.else_branch);
                patch_jump(gen, end, gen->proto->code_length, node->line);
            }
            break;
        }
        case AST_WHILE: {
            size_t start = gen->proto->code_length;
            size_t exit;
            unsigned index = gen->loop_count;
            compile_expression(gen, node->as.loop.condition);
            exit = emit_jump(gen, OP_JMP_UNLESS, node->line);
            if (gen->loop_count >= PPHP_PARSE_DEPTH_MAX) {
                fail(gen, node->line, "loop nesting limit exceeded");
                break;
            }
            gen->loops[gen->loop_count].continue_target = start;
            gen->loops[gen->loop_count].finally_count = gen->finally_count;
            gen->loops[gen->loop_count].is_foreach = 0;
            gen->loops[gen->loop_count].is_switch = 0;
            gen->loop_count++;
            compile_statement(gen, node->as.loop.body);
            emit_back_jump(gen, start, node->line);
            patch_jump(gen, exit, gen->proto->code_length, node->line);
            patch_loop_jumps(gen, index, gen->proto->code_length, start, node->line);
            gen->loop_count--;
            break;
        }
        case AST_DO_WHILE: {
            size_t start = gen->proto->code_length;
            size_t condition;
            unsigned index = gen->loop_count;
            if (gen->loop_count >= PPHP_PARSE_DEPTH_MAX) {
                fail(gen, node->line, "loop nesting limit exceeded");
                break;
            }
            gen->loops[gen->loop_count].continue_target = SIZE_MAX;
            gen->loops[gen->loop_count].finally_count = gen->finally_count;
            gen->loops[gen->loop_count].is_foreach = 0;
            gen->loops[gen->loop_count].is_switch = 0;
            gen->loop_count++;
            compile_statement(gen, node->as.loop.body);
            condition = gen->proto->code_length;
            compile_expression(gen, node->as.loop.condition);
            {
                size_t jump = emit_jump(gen, OP_JMP_IF, node->line);
                patch_jump(gen, jump, start, node->line);
            }
            patch_loop_jumps(gen, index, gen->proto->code_length, condition, node->line);
            gen->loop_count--;
            break;
        }
        case AST_FOR: {
            const pc_ast *expression;
            size_t condition_start;
            size_t exit = SIZE_MAX;
            size_t increment_start;
            unsigned index = gen->loop_count;
            expression = node->as.for_stmt.initializers;
            while (expression != NULL) {
                compile_expression(gen, expression);
                emit_byte(gen, OP_POP, node->line);
                expression = expression->next;
            }
            condition_start = gen->proto->code_length;
            expression = node->as.for_stmt.conditions;
            if (expression != NULL) {
                while (expression->next != NULL) {
                    compile_expression(gen, expression);
                    emit_byte(gen, OP_POP, node->line);
                    expression = expression->next;
                }
                compile_expression(gen, expression);
                exit = emit_jump(gen, OP_JMP_UNLESS, node->line);
            }
            if (gen->loop_count >= PPHP_PARSE_DEPTH_MAX) {
                fail(gen, node->line, "loop nesting limit exceeded");
                break;
            }
            gen->loops[gen->loop_count].continue_target = SIZE_MAX;
            gen->loops[gen->loop_count].finally_count = gen->finally_count;
            gen->loops[gen->loop_count].is_foreach = 0;
            gen->loops[gen->loop_count].is_switch = 0;
            gen->loop_count++;
            compile_statement(gen, node->as.for_stmt.body);
            increment_start = gen->proto->code_length;
            expression = node->as.for_stmt.increments;
            while (expression != NULL) {
                compile_expression(gen, expression);
                emit_byte(gen, OP_POP, node->line);
                expression = expression->next;
            }
            emit_back_jump(gen, condition_start, node->line);
            if (exit != SIZE_MAX) {
                patch_jump(gen, exit, gen->proto->code_length, node->line);
            }
            patch_loop_jumps(gen, index, gen->proto->code_length, increment_start, node->line);
            gen->loop_count--;
            break;
        }
        case AST_BREAK:
            record_loop_jump(gen, node, 0);
            break;
        case AST_CONTINUE:
            record_loop_jump(gen, node, 1);
            break;
        case AST_FOREACH: {
            size_t loop_start;
            size_t exit;
            uint8_t key_slot = 0U;
            uint8_t value_slot;
            unsigned index = gen->loop_count;
            if (gen->loop_count >= PPHP_PARSE_DEPTH_MAX ||
                node->as.foreach_stmt.value == NULL ||
                node->as.foreach_stmt.value->kind != AST_VARIABLE ||
                !variable_slot(gen, node->as.foreach_stmt.value->as.literal.token,
                               &value_slot)) {
                fail(gen, node->line, "invalid foreach target");
                break;
            }
            if (node->as.foreach_stmt.key != NULL &&
                (node->as.foreach_stmt.key->kind != AST_VARIABLE ||
                 !variable_slot(gen, node->as.foreach_stmt.key->as.literal.token,
                                &key_slot))) {
                fail(gen, node->line, "invalid foreach key target");
                break;
            }
            compile_expression(gen, node->as.foreach_stmt.iterable);
            emit_byte(gen, OP_FE_INIT, node->line);
            loop_start = gen->proto->code_length;
            emit_byte(gen, OP_FE_NEXT, node->line);
            exit = gen->proto->code_length;
            emit_u16(gen, 0U, node->line);
            emit_byte(gen, node->as.foreach_stmt.key != NULL ? 1U : 0U, node->line);
            emit_byte(gen, OP_STORE_LOCAL, node->line);
            emit_byte(gen, value_slot, node->line);
            if (node->as.foreach_stmt.key != NULL) {
                emit_byte(gen, OP_STORE_LOCAL, node->line);
                emit_byte(gen, key_slot, node->line);
            }
            gen->loops[gen->loop_count].continue_target = loop_start;
            gen->loops[gen->loop_count].finally_count = gen->finally_count;
            gen->loops[gen->loop_count].is_foreach = 1;
            gen->loops[gen->loop_count].is_switch = 0;
            gen->loop_count++;
            compile_statement(gen, node->as.foreach_stmt.body);
            emit_back_jump(gen, loop_start, node->line);
            patch_foreach_jump(gen, exit, gen->proto->code_length, node->line);
            patch_loop_jumps(gen, index, gen->proto->code_length, loop_start, node->line);
            gen->loop_count--;
            break;
        }
        case AST_SWITCH: {
            const pc_ast *case_node = node->as.switch_stmt.cases;
            size_t case_jumps[256];
            size_t case_count = 0U;
            size_t case_index = 0U;
            size_t no_match;
            size_t default_target = SIZE_MAX;
            size_t cleanup_target;
            size_t end_target;
            unsigned index = gen->loop_count;
            if (gen->loop_count >= PPHP_PARSE_DEPTH_MAX) {
                fail(gen, node->line, "switch nesting limit exceeded");
                break;
            }
            compile_expression(gen, node->as.switch_stmt.subject);
            while (case_node != NULL && !gen->failed) {
                if (case_node->as.case_stmt.condition != NULL) {
                    if (case_count >= sizeof(case_jumps) / sizeof(case_jumps[0])) {
                        fail(gen, node->line, "too many switch cases");
                        break;
                    }
                    emit_byte(gen, OP_DUP, case_node->line);
                    compile_expression(gen, case_node->as.case_stmt.condition);
                    emit_byte(gen, OP_EQ, case_node->line);
                    case_jumps[case_count++] = emit_jump(
                        gen, OP_JMP_IF, case_node->line);
                }
                case_node = case_node->next;
            }
            no_match = emit_jump(gen, OP_JMP, node->line);
            gen->loops[gen->loop_count].continue_target = SIZE_MAX;
            gen->loops[gen->loop_count].finally_count = gen->finally_count;
            gen->loops[gen->loop_count].is_foreach = 0;
            gen->loops[gen->loop_count].is_switch = 1;
            gen->loop_count++;
            case_node = node->as.switch_stmt.cases;
            while (case_node != NULL && !gen->failed) {
                size_t handler = gen->proto->code_length;
                if (case_node->as.case_stmt.condition == NULL) {
                    default_target = handler;
                } else {
                    patch_jump(gen, case_jumps[case_index++], handler,
                               case_node->line);
                }
                compile_statement(gen, case_node->as.case_stmt.body);
                case_node = case_node->next;
            }
            cleanup_target = gen->proto->code_length;
            emit_byte(gen, OP_POP, node->line);
            end_target = gen->proto->code_length;
            patch_jump(gen, no_match,
                       default_target == SIZE_MAX ? cleanup_target : default_target,
                       node->line);
            patch_loop_jumps(gen, index, end_target, end_target, node->line);
            gen->loop_count--;
            break;
        }
        case AST_RETURN:
            if (gen->return_void && node->as.expression.expression != NULL) {
                fail(gen, node->line,
                     "a void function must not return a value");
                break;
            }
            if (gen->finally_count != 0U) {
                uint8_t slot;
                if (gen->return_void &&
                    node->as.expression.expression == NULL) {
                    compile_transfer_finally_blocks(gen, 0U);
                    emit_byte(gen, OP_RET_NULL, node->line);
                    break;
                }
                if (!pproto_add_local(gen->proto, "\x01return", 7U, &slot)) {
                    fail(gen, node->line, "cannot allocate pending return slot");
                    break;
                }
                if (node->as.expression.expression != NULL) {
                    compile_expression(gen, node->as.expression.expression);
                } else {
                    emit_byte(gen, OP_LOAD_NULL, node->line);
                }
                emit_byte(gen, OP_STORE_LOCAL, node->line);
                emit_byte(gen, slot, node->line);
                compile_transfer_finally_blocks(gen, 0U);
                emit_byte(gen, OP_LOAD_LOCAL, node->line);
                emit_byte(gen, slot, node->line);
                emit_byte(gen, OP_RET, node->line);
            } else if (node->as.expression.expression != NULL) {
                compile_expression(gen, node->as.expression.expression);
                emit_byte(gen, OP_RET, node->line);
            } else {
                emit_byte(gen, OP_RET_NULL, node->line);
            }
            break;
        case AST_THROW:
            compile_expression(gen, node->as.expression.expression);
            emit_byte(gen, OP_THROW, node->line);
            break;
        case AST_TRY: {
            size_t try_start = gen->proto->code_length;
            size_t try_end;
            size_t normal_jump;
            size_t exit_jumps[64];
            size_t exit_count = 0U;
            size_t catch_starts[64];
            size_t catch_ends[64];
            size_t catch_range_count = 0U;
            const pc_ast *catch_node;
            size_t normal_target;
            uint8_t pending_slot = UINT8_MAX;
            if (node->as.try_stmt.finally_block != NULL &&
                !exception_slot(gen, node->line, &pending_slot)) {
                break;
            }
            if (node->as.try_stmt.finally_block != NULL) {
                if (gen->finally_count >= PPHP_PARSE_DEPTH_MAX) {
                    fail(gen, node->line, "finally nesting limit exceeded");
                    break;
                }
                gen->finally_blocks[gen->finally_count++] =
                    node->as.try_stmt.finally_block;
            }
            compile_statement(gen, node->as.try_stmt.try_block);
            if (node->as.try_stmt.finally_block != NULL) gen->finally_count--;
            try_end = gen->proto->code_length;
            normal_jump = emit_jump(gen, OP_JMP, node->line);
            catch_node = node->as.try_stmt.catches;
            while (catch_node != NULL && !gen->failed) {
                size_t handler = gen->proto->code_length;
                const pc_ast *type = catch_node->as.catch_stmt.types;
                uint8_t catch_slot = UINT8_MAX;
                if (catch_node->as.catch_stmt.variable.length != 0U &&
                    !variable_slot(gen, catch_node->as.catch_stmt.variable,
                                   &catch_slot)) {
                    break;
                }
                while (type != NULL) {
                    pcatch entry;
                    uint16_t class_name = name_constant(gen, type->as.literal.token);
                    if (try_start > UINT16_MAX || try_end > UINT16_MAX ||
                        handler > UINT16_MAX) {
                        fail(gen, node->line, "try block exceeds bytecode range");
                        break;
                    }
                    entry.try_start = (uint16_t)try_start;
                    entry.try_end = (uint16_t)try_end;
                    entry.handler_pc = (uint16_t)handler;
                    entry.class_constant = class_name;
                    entry.variable_slot = catch_slot;
                    entry.reserved = 0U;
                    if (!pproto_add_catch(gen->proto, entry)) {
                        fail(gen, node->line, "too many catch handlers");
                        break;
                    }
                    type = type->next;
                }
                if (node->as.try_stmt.finally_block != NULL) {
                    gen->finally_blocks[gen->finally_count++] =
                        node->as.try_stmt.finally_block;
                }
                compile_statement(gen, catch_node->as.catch_stmt.body);
                if (node->as.try_stmt.finally_block != NULL) {
                    if (catch_range_count >=
                        sizeof(catch_starts) / sizeof(catch_starts[0])) {
                        fail(gen, node->line, "too many catch clauses");
                        break;
                    }
                    catch_starts[catch_range_count] = handler;
                    catch_ends[catch_range_count] = gen->proto->code_length;
                    catch_range_count++;
                    gen->finally_count--;
                    compile_statement(gen, node->as.try_stmt.finally_block);
                }
                if (exit_count >= sizeof(exit_jumps) / sizeof(exit_jumps[0])) {
                    fail(gen, node->line, "too many catch clauses");
                    break;
                }
                exit_jumps[exit_count++] = emit_jump(gen, OP_JMP, node->line);
                catch_node = catch_node->next;
            }
            if (node->as.try_stmt.finally_block != NULL && !gen->failed) {
                pcatch entry;
                size_t handler = gen->proto->code_length;
                size_t range_index;
                if (try_start > UINT16_MAX || try_end > UINT16_MAX ||
                    handler > UINT16_MAX) {
                    fail(gen, node->line, "finally handler exceeds bytecode range");
                    break;
                }
                entry.try_start = (uint16_t)try_start;
                entry.try_end = (uint16_t)try_end;
                entry.handler_pc = (uint16_t)handler;
                entry.class_constant = UINT16_MAX;
                entry.variable_slot = pending_slot;
                entry.reserved = 0U;
                if (!pproto_add_catch(gen->proto, entry)) {
                    fail(gen, node->line, "too many exception handlers");
                    break;
                }
                for (range_index = 0U;
                     range_index < catch_range_count && !gen->failed;
                     range_index++) {
                    if (catch_starts[range_index] > UINT16_MAX ||
                        catch_ends[range_index] > UINT16_MAX) {
                        fail(gen, node->line, "catch block exceeds bytecode range");
                        break;
                    }
                    if (catch_starts[range_index] == catch_ends[range_index]) {
                        continue;
                    }
                    entry.try_start = (uint16_t)catch_starts[range_index];
                    entry.try_end = (uint16_t)catch_ends[range_index];
                    if (!pproto_add_catch(gen->proto, entry)) {
                        fail(gen, node->line, "too many exception handlers");
                        break;
                    }
                }
                compile_statement(gen, node->as.try_stmt.finally_block);
                emit_byte(gen, OP_LOAD_LOCAL, node->line);
                emit_byte(gen, pending_slot, node->line);
                emit_byte(gen, OP_THROW, node->line);
            }
            normal_target = gen->proto->code_length;
            patch_jump(gen, normal_jump, normal_target, node->line);
            if (node->as.try_stmt.finally_block != NULL) {
                compile_statement(gen, node->as.try_stmt.finally_block);
            }
            while (exit_count > 0U) {
                exit_count--;
                patch_jump(gen, exit_jumps[exit_count], gen->proto->code_length,
                           node->line);
            }
            break;
        }
        case AST_FUNCTION: {
            size_t i;
            for (i = 1U; i < gen->module->count && i <= UINT16_MAX; i++) {
                const pproto *proto = gen->module->protos[i];
                if (proto->declaration == node) {
                    if (proto->conditional) {
                        emit_byte(gen, OP_DEF_FUNC, node->line);
                        emit_u16(gen, (uint16_t)i, node->line);
                    }
                    break;
                }
            }
            if (i >= gen->module->count || i > UINT16_MAX) {
                fail(gen, node->line, "function prototype is missing");
            }
            break;
        }
        case AST_CLASS:
            compile_class_definition(gen, node);
            break;
        case AST_STATIC: {
            uint8_t slot;
            size_t skip;
            if (!variable_slot(gen, node->as.binding.name, &slot)) break;
            if (node->as.binding.value != NULL &&
                !is_constant_default(node->as.binding.value)) {
                fail(gen, node->line,
                     "static variable initializer must be a constant expression");
                break;
            }
            emit_byte(gen, OP_STATIC_INIT, node->line);
            emit_byte(gen, slot, node->line);
            skip = gen->proto->code_length;
            emit_u16(gen, 0U, node->line);
            if (node->as.binding.value != NULL) {
                compile_expression(gen, node->as.binding.value);
            } else {
                emit_byte(gen, OP_LOAD_NULL, node->line);
            }
            emit_byte(gen, OP_STORE_LOCAL, node->line);
            emit_byte(gen, slot, node->line);
            patch_jump(gen, skip, gen->proto->code_length, node->line);
            break;
        }
        case AST_GLOBAL: {
            const pc_ast *variable = node->as.list.items;
            while (variable != NULL) {
                uint8_t slot;
                if (!variable_slot(gen, variable->as.literal.token, &slot)) break;
                emit_byte(gen, OP_BIND_GLOBAL, variable->line);
                emit_byte(gen, slot, variable->line);
                variable = variable->next;
            }
            break;
        }
        case AST_CONST: {
            uint16_t name;
            if (node->as.binding.value == NULL ||
                !is_constant_default(node->as.binding.value)) {
                fail(gen, node->line,
                     "constant initializer must be a constant expression");
                break;
            }
            compile_expression(gen, node->as.binding.value);
            name = name_constant(gen, node->as.binding.name);
            emit_byte(gen, OP_DEF_CONST, node->line);
            emit_u16(gen, name, node->line);
            break;
        }
        case AST_INCLUDE:
            compile_expression(gen, node->as.include_stmt.path);
            emit_byte(gen, OP_INCLUDE, node->line);
            emit_byte(gen, (uint8_t)node->as.include_stmt.mode, node->line);
            emit_byte(gen, OP_POP, node->line);
            break;
        default:
            fail(gen, node->line, "statement node %s is not executable",
                 pc_ast_kind_name(node->kind));
            break;
    }
}

static int compile_function(generator *gen, const pc_ast *function,
                            int conditional) {
    pproto *proto;
    const pc_ast *parameter;
    generator child;
    int saw_default = 0;
    if (function->as.function.declaration_only) {
        fail(gen, function->line, "function declaration requires a body");
        return 0;
    }
    proto = pproto_new(function->as.function.name.start,
                       function->as.function.name.length);
    if (proto == NULL || !pmodule_add(gen->module, proto)) {
        pproto_destroy(proto);
        fail(gen, function->line, "out of memory while creating function");
        return 0;
    }
    proto->declaration = function;
    proto->conditional = (uint8_t)conditional;
    parameter = function->as.function.parameters;
    while (parameter != NULL) {
        uint8_t slot;
        pc_token name = parameter->as.parameter.name;
        if (parameter->as.parameter.flags != 0U) {
            fail(gen, parameter->line,
                 "property promotion is only valid in a constructor");
            return 0;
        }
        if (name.length != 0U && name.start[0] == '$') {
            name.start++;
            name.length--;
        }
        if (!pproto_add_local(proto, name.start, name.length, &slot)) {
            fail(gen, parameter->line, "too many function parameters");
            return 0;
        }
        proto->n_params++;
        if (proto->n_params > 31U) {
            fail(gen, parameter->line, "a function may have at most 31 parameters");
            return 0;
        }
        if (parameter->as.parameter.variadic && parameter->next != NULL) {
            fail(gen, parameter->line, "variadic parameter must be last");
            return 0;
        }
        if (parameter->as.parameter.variadic &&
            parameter->as.parameter.default_value != NULL) {
            fail(gen, parameter->line,
                 "variadic parameter cannot have a default value");
            return 0;
        }
        if (parameter->as.parameter.default_value == NULL && !parameter->as.parameter.variadic) {
            if (saw_default) {
                fail(gen, parameter->line, "required parameter follows optional parameter");
                return 0;
            }
            proto->n_required++;
        } else {
            saw_default = 1;
        }
        proto->variadic = (uint8_t)parameter->as.parameter.variadic;
        parameter = parameter->next;
    }
    memset(&child, 0, sizeof(child));
    child.module = gen->module;
    child.proto = proto;
    child.error = gen->error;
    child.return_void = type_is_only(function->as.function.return_type, T_VOID);
    if (!validate_callable_types(gen, function->as.function.parameters,
                                 function->as.function.return_type,
                                 TYPE_CONTEXT_NONE, 0)) return 0;
#if PPHP_TYPECHECK
    if (!compile_callable_types(gen, proto,
                                function->as.function.parameters,
                                function->as.function.return_type)) return 0;
#endif
    compile_parameter_defaults(&child, function->as.function.parameters);
#if PPHP_TYPECHECK
    emit_byte(&child, OP_TYPECHECK_ARGS, function->line);
#endif
    compile_statement(&child, function->as.function.body);
    if (!child.failed) {
        emit_byte(&child, OP_RET_NULL, function->line);
        proto->max_stack = PPHP_STACK_SLOTS;
    }
    if (child.failed) {
        gen->failed = 1;
        return 0;
    }
    return 1;
}

static int compile_method(generator *gen, const pc_ast *class_node,
                          const pc_ast *method) {
    pc_token class_name = class_node->as.class_decl.name;
    size_t qualified_length = class_name.length + 2U + method->as.function.name.length;
    char *qualified;
    pproto *proto;
    const pc_ast *parameter;
    generator child;
    int saw_default = 0;
    int is_constructor = token_bytes_equal_ci(method->as.function.name,
                                              "__construct", 11U);
    int is_destructor = token_bytes_equal_ci(method->as.function.name,
                                             "__destruct", 10U);
    uint8_t ignored;
    if ((method->as.function.flags & PC_MOD_ABSTRACT) != 0U &&
        !method->as.function.declaration_only) {
        fail(gen, method->line, "abstract method must not have a body");
        return 0;
    }
    if ((method->as.function.flags & PC_MOD_ABSTRACT) == 0U &&
        method->as.function.declaration_only) {
        fail(gen, method->line, "non-abstract method requires a body");
        return 0;
    }
    if ((method->as.function.flags & (PC_MOD_ABSTRACT | PC_MOD_FINAL)) ==
        (PC_MOD_ABSTRACT | PC_MOD_FINAL)) {
        fail(gen, method->line, "method cannot be both abstract and final");
        return 0;
    }
    qualified = pphp_alloc(qualified_length);
    if (qualified == NULL) {
        fail(gen, method->line, "out of memory creating method name");
        return 0;
    }
    memcpy(qualified, class_name.start, class_name.length);
    memcpy(qualified + class_name.length, "::", 2U);
    memcpy(qualified + class_name.length + 2U, method->as.function.name.start,
           method->as.function.name.length);
    proto = pproto_new(qualified, qualified_length);
    pphp_free(qualified);
    if (proto == NULL || !pmodule_add(gen->module, proto)) {
        pproto_destroy(proto);
        fail(gen, method->line, "out of memory creating method");
        return 0;
    }
    proto->is_method = (uint8_t)((method->as.function.flags & PC_MOD_STATIC) == 0U);
    if (proto->is_method && !pproto_add_local(proto, "this", 4U, &ignored)) {
        fail(gen, method->line, "cannot allocate $this slot");
        return 0;
    }
    parameter = method->as.function.parameters;
    while (parameter != NULL) {
        pc_token name = parameter->as.parameter.name;
        uint8_t slot;
        if (parameter->as.parameter.flags != 0U) {
            if (!is_constructor) {
                fail(gen, parameter->line,
                     "property promotion is only valid in a constructor");
                return 0;
            }
            if (parameter->as.parameter.variadic) {
                fail(gen, parameter->line,
                     "a promoted property cannot be variadic");
                return 0;
            }
            if (((parameter->as.parameter.flags & PC_MOD_READONLY) != 0U ||
                 (class_node->as.class_decl.flags & PC_MOD_READONLY) != 0U) &&
                parameter->as.parameter.type == NULL) {
                fail(gen, parameter->line,
                     "a readonly property must have a type");
                return 0;
            }
            if (!validate_type_spec(
                    gen, parameter->as.parameter.type, TYPE_CONTEXT_CLASS,
                    class_node->as.class_decl.parent.length != 0U, 0, 1)) {
                return 0;
            }
        }
        if (name.length != 0U && name.start[0] == '$') {
            name.start++;
            name.length--;
        }
        if (!pproto_add_local(proto, name.start, name.length, &slot)) {
            fail(gen, parameter->line, "too many method parameters");
            return 0;
        }
        proto->n_params++;
        if (proto->n_params > 31U) {
            fail(gen, parameter->line, "a method may have at most 31 parameters");
            return 0;
        }
        if (parameter->as.parameter.variadic && parameter->next != NULL) {
            fail(gen, parameter->line, "variadic parameter must be last");
            return 0;
        }
        if (parameter->as.parameter.variadic &&
            parameter->as.parameter.default_value != NULL) {
            fail(gen, parameter->line,
                 "variadic parameter cannot have a default value");
            return 0;
        }
        if (parameter->as.parameter.default_value == NULL && !parameter->as.parameter.variadic) {
            if (saw_default) {
                fail(gen, parameter->line, "required parameter follows optional parameter");
                return 0;
            }
            proto->n_required++;
        } else {
            saw_default = 1;
        }
        proto->variadic = (uint8_t)parameter->as.parameter.variadic;
        parameter = parameter->next;
    }
    memset(&child, 0, sizeof(child));
    child.module = gen->module;
    child.proto = proto;
    child.error = gen->error;
    child.class_scope = 1;
    child.class_has_parent = class_node->as.class_decl.parent.length != 0U;
    child.return_void = is_constructor || is_destructor ||
                        type_is_only(method->as.function.return_type, T_VOID);
    if ((is_constructor || is_destructor) &&
        method->as.function.return_type != NULL &&
        !type_is_only(method->as.function.return_type, T_VOID)) {
        fail(gen, method->line,
             is_constructor
                 ? "a constructor may only declare void as its return type"
                 : "a destructor may only declare void as its return type");
        return 0;
    }
    if (!validate_callable_types(gen, method->as.function.parameters,
                                 method->as.function.return_type,
                                 TYPE_CONTEXT_METHOD,
                                 child.class_has_parent)) return 0;
#if PPHP_TYPECHECK
    if (!compile_callable_types(gen, proto,
                                method->as.function.parameters,
                                method->as.function.return_type)) return 0;
#endif
    compile_parameter_defaults(&child, method->as.function.parameters);
#if PPHP_TYPECHECK
    emit_byte(&child, OP_TYPECHECK_ARGS, method->line);
#endif
    if (is_constructor) {
        parameter = method->as.function.parameters;
        while (parameter != NULL && !child.failed) {
            if (parameter->as.parameter.flags != 0U) {
                uint8_t slot;
                uint16_t property_name;
                if (!variable_slot(&child, parameter->as.parameter.name, &slot)) {
                    break;
                }
                emit_byte(&child, OP_LOAD_LOCAL, parameter->line);
                emit_byte(&child, 0U, parameter->line);
                emit_byte(&child, OP_LOAD_LOCAL, parameter->line);
                emit_byte(&child, slot, parameter->line);
                property_name = member_name_constant(
                    &child, parameter->as.parameter.name);
                emit_byte(&child, OP_PROP_SET, parameter->line);
                emit_u16(&child, property_name, parameter->line);
                emit_byte(&child, OP_POP, parameter->line);
            }
            parameter = parameter->next;
        }
    }
    compile_statement(&child, method->as.function.body);
    if (!child.failed) {
        emit_byte(&child, OP_RET_NULL, method->line);
        proto->max_stack = PPHP_STACK_SLOTS;
    }
    if (child.failed) {
        gen->failed = 1;
        return 0;
    }
    return 1;
}

static int method_proto_index(const pmodule *module, pc_token class_name,
                              pc_token method_name, uint16_t *index) {
    size_t i;
    size_t length = class_name.length + 2U + method_name.length;
    for (i = 1U; i < module->count && i <= UINT16_MAX; i++) {
        const pstring *name = module->protos[i]->name;
        if (name->length == length &&
            memcmp(ps_data(name), class_name.start, class_name.length) == 0 &&
            memcmp(ps_data(name) + class_name.length, "::", 2U) == 0 &&
            memcmp(ps_data(name) + class_name.length + 2U, method_name.start,
                   method_name.length) == 0) {
            *index = (uint16_t)i;
            return 1;
        }
    }
    return 0;
}

static void compile_class_definition(generator *gen, const pc_ast *class_node) {
    const pc_ast *member = class_node->as.class_decl.members;
    uint16_t class_name = name_constant(gen, class_node->as.class_decl.name);
    uint16_t parent_name = UINT16_MAX;
    const int readonly_class =
        (class_node->as.class_decl.flags & PC_MOD_READONLY) != 0U;
    {
        const pc_ast *check = member;
        while (check != NULL && !gen->failed) {
            if (check->kind == AST_PROPERTY) {
                uint8_t flags = check->as.property.flags;
                if (readonly_class) flags |= PC_MOD_READONLY;
                if (!validate_type_spec(
                        gen, check->as.property.type, TYPE_CONTEXT_CLASS,
                        class_node->as.class_decl.parent.length != 0U, 0, 1)) {
                    break;
                }
                if ((flags & PC_MOD_READONLY) != 0U &&
                    check->as.property.type == NULL) {
                    fail(gen, check->line,
                         "a readonly property must have a type");
                } else if ((flags & PC_MOD_READONLY) != 0U &&
                           check->as.property.default_value != NULL) {
                    fail(gen, check->line,
                         "a readonly property cannot have a default value");
                } else if ((flags & (PC_MOD_STATIC | PC_MOD_READONLY)) ==
                           (PC_MOD_STATIC | PC_MOD_READONLY)) {
                    fail(gen, check->line,
                         "a readonly property cannot be static");
                }
            }
            check = check->next;
        }
        if (gen->failed) return;
    }
    if (class_node->as.class_decl.parent.length != 0U) {
        parent_name = name_constant(gen, class_node->as.class_decl.parent);
    }
    emit_byte(gen, OP_DEF_CLASS, class_node->line);
    emit_u16(gen, class_name, class_node->line);
    emit_u16(gen, parent_name, class_node->line);
    emit_byte(gen, class_node->as.class_decl.flags, class_node->line);
    {
        const pc_ast *interface_node = class_node->as.class_decl.interfaces;
        while (interface_node != NULL) {
            uint16_t interface_name = name_constant(
                gen, interface_node->as.literal.token);
            emit_byte(gen, OP_DEF_INTERFACE, interface_node->line);
            emit_u16(gen, interface_name, interface_node->line);
            interface_node = interface_node->next;
        }
    }
    {
        const pc_ast *method = member;
        while (method != NULL && !gen->failed) {
            if (method->kind == AST_FUNCTION &&
                token_bytes_equal_ci(method->as.function.name,
                                     "__construct", 11U)) {
                const pc_ast *parameter = method->as.function.parameters;
                while (parameter != NULL) {
                    if (parameter->as.parameter.flags != 0U) {
                        uint16_t property_name;
                        uint8_t flags = parameter->as.parameter.flags;
                        if (readonly_class) flags |= PC_MOD_READONLY;
                        emit_byte(gen, OP_LOAD_NULL, parameter->line);
                        property_name = member_name_constant(
                            gen, parameter->as.parameter.name);
                        emit_byte(gen, OP_DEF_PROP, parameter->line);
                        emit_u16(gen, property_name, parameter->line);
                        emit_byte(gen, flags, parameter->line);
#if PPHP_TYPECHECK
                        emit_u16(gen, declared_type_constant(
                                          gen, parameter->as.parameter.type),
                                 parameter->line);
                        emit_byte(gen, 0U, parameter->line);
#endif
                    }
                    parameter = parameter->next;
                }
                break;
            }
            method = method->next;
        }
    }
    while (member != NULL && !gen->failed) {
        if (member->kind == AST_PROPERTY) {
            uint16_t name;
            uint8_t flags = member->as.property.flags;
            pc_token property_name = member->as.property.name;
            if (readonly_class) flags |= PC_MOD_READONLY;
            if (member->as.property.default_value != NULL) {
                compile_expression(gen, member->as.property.default_value);
            } else {
                emit_byte(gen, OP_LOAD_NULL, member->line);
            }
            if (property_name.length != 0U && property_name.start[0] == '$') {
                property_name.start++;
                property_name.length--;
            }
            name = name_constant(gen, property_name);
            emit_byte(gen, OP_DEF_PROP, member->line);
            emit_u16(gen, name, member->line);
            emit_byte(gen, flags, member->line);
#if PPHP_TYPECHECK
            emit_u16(gen, declared_type_constant(gen, member->as.property.type),
                     member->line);
            emit_byte(gen, member->as.property.default_value != NULL,
                      member->line);
#endif
        } else if (member->kind == AST_FUNCTION) {
            uint16_t name = name_constant(gen, member->as.function.name);
            uint16_t proto_index;
            if (!method_proto_index(gen->module, class_node->as.class_decl.name,
                                    member->as.function.name, &proto_index)) {
                fail(gen, member->line, "method prototype is missing");
                break;
            }
            emit_byte(gen, OP_DEF_METHOD, member->line);
            emit_u16(gen, name, member->line);
            emit_u16(gen, proto_index, member->line);
            emit_byte(gen, member->as.function.flags, member->line);
        } else if (member->kind == AST_CONST) {
            uint16_t name;
            if (member->as.binding.value == NULL ||
                !is_constant_default(member->as.binding.value)) {
                fail(gen, member->line,
                     "class constant initializer must be constant");
                break;
            }
            compile_expression(gen, member->as.binding.value);
            name = name_constant(gen, member->as.binding.name);
            emit_byte(gen, OP_DEF_CCONST, member->line);
            emit_u16(gen, name, member->line);
        }
        member = member->next;
    }
    emit_byte(gen, OP_DEF_END, class_node->line);
}

static void compile_named_functions(generator *gen, const pc_ast *node,
                                    int direct_top_level);

static void compile_named_function_list(generator *gen, const pc_ast *node,
                                        int direct_top_level) {
    while (node != NULL && !gen->failed) {
        compile_named_functions(gen, node, direct_top_level);
        node = node->next;
    }
}

static void compile_named_functions(generator *gen, const pc_ast *node,
                                    int direct_top_level) {
    const pc_ast *member;
    size_t i;
    if (node == NULL || gen->failed) return;
    switch (node->kind) {
        case AST_PROGRAM:
            compile_named_function_list(gen, node->as.list.items, 1);
            break;
        case AST_BLOCK:
        case AST_ARRAY:
        case AST_ECHO:
            compile_named_function_list(gen, node->as.list.items, 0);
            break;
        case AST_FUNCTION:
            compile_named_functions(gen, node->as.function.body, 0);
            if (direct_top_level) {
                for (i = 1U; i < gen->module->count; i++) {
                    const pproto *existing = gen->module->protos[i];
                    if (!existing->is_method && !existing->conditional &&
                        token_equal(node->as.function.name, existing->name)) {
                        fail(gen, node->line, "function %.*s is already defined",
                             (int)node->as.function.name.length,
                             node->as.function.name.start);
                        return;
                    }
                }
            }
            if (gen->module->count >= 512U) {
                fail(gen, node->line, "module function limit exceeded");
            } else {
                (void)compile_function(gen, node, !direct_top_level);
            }
            break;
        case AST_CLASS:
            member = node->as.class_decl.members;
            while (member != NULL && !gen->failed) {
                if (member->kind == AST_FUNCTION) {
                    compile_named_functions(gen, member->as.function.body, 0);
                } else if (member->kind == AST_PROPERTY) {
                    compile_named_functions(gen,
                                            member->as.property.default_value,
                                            0);
                } else if (member->kind == AST_CONST) {
                    compile_named_functions(gen, member->as.binding.value, 0);
                }
                member = member->next;
            }
            break;
        case AST_CLOSURE:
            compile_named_functions(gen, node->as.closure.body, 0);
            break;
        case AST_UNARY:
        case AST_UNSET:
        case AST_ISSET:
        case AST_EMPTY:
            compile_named_functions(gen, node->as.unary.operand, 0);
            break;
        case AST_BINARY:
        case AST_ASSIGN:
            compile_named_functions(gen, node->as.binary.left, 0);
            compile_named_functions(gen, node->as.binary.right, 0);
            break;
        case AST_TERNARY:
            compile_named_functions(gen, node->as.ternary.condition, 0);
            compile_named_functions(gen, node->as.ternary.then_expr, 0);
            compile_named_functions(gen, node->as.ternary.else_expr, 0);
            break;
        case AST_MATCH:
            compile_named_functions(gen, node->as.match_expr.subject, 0);
            compile_named_function_list(gen, node->as.match_expr.arms, 0);
            break;
        case AST_MATCH_ARM:
            compile_named_function_list(gen, node->as.match_arm.conditions, 0);
            compile_named_functions(gen, node->as.match_arm.result, 0);
            break;
        case AST_CALL:
            compile_named_functions(gen, node->as.call.callee, 0);
            compile_named_function_list(gen, node->as.call.arguments, 0);
            break;
        case AST_INDEX:
            compile_named_functions(gen, node->as.index.base, 0);
            compile_named_functions(gen, node->as.index.key, 0);
            break;
        case AST_MEMBER:
            compile_named_functions(gen, node->as.member.base, 0);
            compile_named_functions(gen, node->as.member.dynamic_name, 0);
            break;
        case AST_ARRAY_ITEM:
            compile_named_functions(gen, node->as.array_item.key, 0);
            compile_named_functions(gen, node->as.array_item.value, 0);
            break;
        case AST_EXPR_STMT:
        case AST_RETURN:
        case AST_THROW:
            compile_named_functions(gen, node->as.expression.expression, 0);
            break;
        case AST_IF:
            compile_named_functions(gen, node->as.if_stmt.condition, 0);
            compile_named_functions(gen, node->as.if_stmt.then_branch, 0);
            compile_named_functions(gen, node->as.if_stmt.else_branch, 0);
            break;
        case AST_WHILE:
        case AST_DO_WHILE:
            compile_named_functions(gen, node->as.loop.condition, 0);
            compile_named_functions(gen, node->as.loop.body, 0);
            break;
        case AST_FOR:
            compile_named_function_list(gen, node->as.for_stmt.initializers, 0);
            compile_named_function_list(gen, node->as.for_stmt.conditions, 0);
            compile_named_function_list(gen, node->as.for_stmt.increments, 0);
            compile_named_functions(gen, node->as.for_stmt.body, 0);
            break;
        case AST_FOREACH:
            compile_named_functions(gen, node->as.foreach_stmt.iterable, 0);
            compile_named_functions(gen, node->as.foreach_stmt.body, 0);
            break;
        case AST_SWITCH:
            compile_named_functions(gen, node->as.switch_stmt.subject, 0);
            compile_named_function_list(gen, node->as.switch_stmt.cases, 0);
            break;
        case AST_CASE:
            compile_named_functions(gen, node->as.case_stmt.condition, 0);
            compile_named_functions(gen, node->as.case_stmt.body, 0);
            break;
        case AST_STATIC:
        case AST_CONST:
            compile_named_functions(gen, node->as.binding.value, 0);
            break;
        case AST_INCLUDE:
            compile_named_functions(gen, node->as.include_stmt.path, 0);
            break;
        case AST_NEW:
            compile_named_functions(gen, node->as.new_expr.class_name, 0);
            compile_named_function_list(gen, node->as.new_expr.arguments, 0);
            break;
        case AST_TRY:
            compile_named_functions(gen, node->as.try_stmt.try_block, 0);
            compile_named_function_list(gen, node->as.try_stmt.catches, 0);
            compile_named_functions(gen, node->as.try_stmt.finally_block, 0);
            break;
        case AST_CATCH:
            compile_named_functions(gen, node->as.catch_stmt.body, 0);
            break;
        default:
            break;
    }
}

int pc_codegen_program(const pc_ast *program, pmodule *module,
                       pc_codegen_error *error) {
    generator gen;
    const pc_ast *statement;
    pproto *entry;
    memset(error, 0, sizeof(*error));
    if (!pmodule_init(module)) {
        return 0;
    }
    entry = pproto_new("{main}", 6U);
    if (entry == NULL || !pmodule_add(module, entry)) {
        pproto_destroy(entry);
        (void)snprintf(error->message, sizeof(error->message), "out of memory creating module");
        return 0;
    }
    memset(&gen, 0, sizeof(gen));
    gen.module = module;
    gen.proto = entry;
    gen.error = error;
    compile_named_functions(&gen, program, 0);
    statement = program->as.list.items;
    while (statement != NULL && !gen.failed) {
        if (statement->kind == AST_CLASS) {
            const pc_ast *member = statement->as.class_decl.members;
            while (member != NULL && !gen.failed) {
                if (member->kind == AST_FUNCTION) {
                    (void)compile_method(&gen, statement, member);
                }
                member = member->next;
            }
        }
        statement = statement->next;
    }
    if (!gen.failed) {
        compile_statement(&gen, program);
        emit_byte(&gen, OP_HALT, program->line);
        entry->max_stack = PPHP_STACK_SLOTS;
    }
    if (gen.failed) {
        pmodule_destroy(module);
        return 0;
    }
    return 1;
}
