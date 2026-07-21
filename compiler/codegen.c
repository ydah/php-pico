#include "codegen.h"

#include "opcode.h"
#include "pphp/pphp.h"
#include "pclass.h"

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
    return ps_equal_bytes(string, token.start, token.length);
}

static pphp_int parse_integer_token(pc_token token, int *overflow) {
    size_t i = 0U;
    unsigned base = 10U;
    uint64_t value = 0U;
    *overflow = 0;
    if (token.length >= 2U && token.start[0] == '0') {
        if (token.start[1] == 'x' || token.start[1] == 'X') {
            base = 16U;
            i = 2U;
        } else if (token.start[1] == 'b' || token.start[1] == 'B') {
            base = 2U;
            i = 2U;
        } else if (token.start[1] == 'o' || token.start[1] == 'O') {
            base = 8U;
            i = 2U;
        } else if (token.start[1] >= '0' && token.start[1] <= '7') {
            base = 8U;
            i = 1U;
        }
    }
    for (; i < token.length; i++) {
        unsigned digit;
        char value_char = token.start[i];
        if (value_char == '_') {
            continue;
        }
        if (value_char >= '0' && value_char <= '9') {
            digit = (unsigned)(value_char - '0');
        } else if (value_char >= 'a' && value_char <= 'f') {
            digit = (unsigned)(value_char - 'a') + 10U;
        } else {
            digit = (unsigned)(value_char - 'A') + 10U;
        }
        if (value > (uint64_t)INT32_MAX / base ||
            value * base > (uint64_t)INT32_MAX - digit) {
            *overflow = 1;
        }
        value = value * base + digit;
    }
    return (pphp_int)value;
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
            default: decoded[output++] = c; break;
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

static void compile_expression(generator *gen, const pc_ast *node);
static void compile_statement(generator *gen, const pc_ast *node);
static void compile_class_definition(generator *gen, const pc_ast *class_node);

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
        case AST_GLOBAL:
            return collect_capture_list(set, node->as.list.items);
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
            return collect_captures(set, node->as.member.base);
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
    if (node->as.closure.is_arrow) {
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

static void compile_assignment(generator *gen, const pc_ast *node) {
    uint8_t slot;
    pc_token_type operation = node->as.binary.op;
    if (node->as.binary.left->kind == AST_INDEX) {
        const pc_ast *index = node->as.binary.left;
        uint8_t array_slot;
        if (operation != T_EQUAL || index->as.index.base->kind != AST_VARIABLE ||
            !variable_slot(gen, index->as.index.base->as.literal.token, &array_slot)) {
            fail(gen, node->line, "complex array assignment is not supported");
            return;
        }
        emit_byte(gen, OP_LOAD_LOCAL, node->line);
        emit_byte(gen, array_slot, node->line);
        if (index->as.index.key != NULL) {
            compile_expression(gen, index->as.index.key);
        }
        compile_expression(gen, node->as.binary.right);
        emit_byte(gen, index->as.index.key == NULL ? OP_IDX_APPEND : OP_IDX_SET,
                  node->line);
        emit_byte(gen, OP_SWAP, node->line);
        emit_byte(gen, OP_STORE_LOCAL, node->line);
        emit_byte(gen, array_slot, node->line);
        return;
    }
    if (node->as.binary.left->kind == AST_MEMBER) {
        const pc_ast *member = node->as.binary.left;
        uint16_t name;
        if (operation != T_EQUAL || member->as.member.op != T_ARROW) {
            fail(gen, node->line, "complex property assignment is not supported");
            return;
        }
        compile_expression(gen, member->as.member.base);
        compile_expression(gen, node->as.binary.right);
        name = name_constant(gen, member->as.member.name);
        emit_byte(gen, OP_PROP_SET, node->line);
        emit_u16(gen, name, node->line);
        return;
    }
    if (node->as.binary.left->kind != AST_VARIABLE) {
        fail(gen, node->line, "assignment target is not executable");
        return;
    }
    if (!variable_slot(gen, node->as.binary.left->as.literal.token, &slot)) {
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
    if (node->as.unary.operand->kind != AST_VARIABLE ||
        !variable_slot(gen, node->as.unary.operand->as.literal.token, &slot)) {
        fail(gen, node->line, "only local variables can be incremented");
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
    compile_expression(gen, node->as.binary.left);
    if (node->as.binary.op == T_COALESCE) {
        opcode = OP_JMP_NOTNULL_KEEP;
    } else if (node->as.binary.op == T_BOOL_OR || node->as.binary.op == T_OR) {
        opcode = OP_JMP_IF_KEEP;
    } else {
        opcode = OP_JMP_UNLESS_KEEP;
    }
    jump = emit_jump(gen, opcode, node->line);
    compile_expression(gen, node->as.binary.right);
    patch_jump(gen, jump, gen->proto->code_length, node->line);
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
            pphp_int value = parse_integer_token(node->as.literal.token, &overflow);
            if (!overflow && value >= INT8_MIN && value <= INT8_MAX) {
                emit_byte(gen, OP_LOAD_I8, node->line);
                emit_byte(gen, (uint8_t)(int8_t)value, node->line);
            } else if (!overflow) {
                emit_byte(gen, OP_LOAD_I32, node->line);
                emit_i32(gen, (int32_t)value, node->line);
            } else {
                emit_constant(gen, pv_float((pphp_float)(uint32_t)value), node->line);
            }
            break;
        }
        case AST_FLOAT:
            emit_constant(gen, pv_float(parse_float_token(node->as.literal.token)), node->line);
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
                uint16_t class_name;
                if (node->as.binary.right->kind != AST_IDENTIFIER) {
                    fail(gen, node->line, "dynamic instanceof is not supported");
                    break;
                }
                compile_expression(gen, node->as.binary.left);
                class_name = name_constant(gen,
                    node->as.binary.right->as.literal.token);
                emit_byte(gen, OP_INSTANCEOF, node->line);
                emit_u16(gen, class_name, node->line);
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
            compile_expression(gen, node->as.unary.operand);
            if (node->as.unary.op == T_MINUS) emit_byte(gen, OP_NEG, node->line);
            else if (node->as.unary.op == T_BANG) emit_byte(gen, OP_NOT, node->line);
            else if (node->as.unary.op == T_TILDE) emit_byte(gen, OP_BNOT, node->line);
            else if (node->as.unary.op != T_PLUS) {
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
            if (node->as.call.callee->kind == AST_MEMBER &&
                node->as.call.callee->as.member.op == T_ARROW) {
                uint16_t method_name;
                compile_expression(gen, node->as.call.callee->as.member.base);
                while (argument != NULL) {
                    compile_expression(gen, argument);
                    argument = argument->next;
                }
                method_name = name_constant(gen, node->as.call.callee->as.member.name);
                emit_byte(gen, OP_MCALL, node->line);
                emit_u16(gen, method_name, node->line);
                emit_byte(gen, (uint8_t)node->as.call.count, node->line);
                break;
            }
            if (node->as.call.count > 255U) {
                fail(gen, node->line, "too many call arguments");
                break;
            }
            if (node->as.call.callee->kind != AST_IDENTIFIER) {
                compile_expression(gen, node->as.call.callee);
                while (argument != NULL) {
                    compile_expression(gen, argument);
                    argument = argument->next;
                }
                emit_byte(gen, OP_CALL_VALUE, node->line);
                emit_byte(gen, (uint8_t)node->as.call.count, node->line);
                break;
            }
            while (argument != NULL) {
                if (argument->kind == AST_UNARY && argument->as.unary.op == T_ELLIPSIS) {
                    fail(gen, argument->line, "argument unpacking requires array runtime");
                    break;
                }
                compile_expression(gen, argument);
                argument = argument->next;
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
            emit_byte(gen, OP_CALL, node->line);
            emit_u16(gen, name_index, node->line);
            emit_byte(gen, (uint8_t)node->as.call.count, node->line);
            break;
        }
        case AST_MEMBER: {
            uint16_t name;
            if (node->as.member.op != T_ARROW) {
                fail(gen, node->line, "static member access is not supported yet");
                break;
            }
            compile_expression(gen, node->as.member.base);
            name = name_constant(gen, node->as.member.name);
            emit_byte(gen, OP_PROP_GET, node->line);
            emit_u16(gen, name, node->line);
            break;
        }
        case AST_NEW: {
            const pc_ast *argument = node->as.new_expr.arguments;
            uint16_t class_name;
            if (node->as.new_expr.class_name == NULL ||
                node->as.new_expr.class_name->kind != AST_IDENTIFIER ||
                node->as.new_expr.count > 255U) {
                fail(gen, node->line, "invalid class construction");
                break;
            }
            while (argument != NULL) {
                compile_expression(gen, argument);
                argument = argument->next;
            }
            class_name = name_constant(gen, node->as.new_expr.class_name->as.literal.token);
            emit_byte(gen, OP_NEW_OBJ, node->line);
            emit_u16(gen, class_name, node->line);
            emit_byte(gen, (uint8_t)node->as.new_expr.count, node->line);
            break;
        }
        case AST_ARRAY: {
            const pc_ast *item = node->as.list.items;
            emit_byte(gen, OP_NEW_ARRAY, node->line);
            emit_u16(gen, (uint16_t)node->as.list.count, node->line);
            while (item != NULL) {
                if (item->as.array_item.spread) {
                    fail(gen, item->line, "array spread is not supported yet");
                    break;
                }
                if (item->as.array_item.key != NULL) {
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
            if (node->as.index.key == NULL) {
                fail(gen, node->line, "cannot read from an empty array index");
                break;
            }
            compile_expression(gen, node->as.index.base);
            compile_expression(gen, node->as.index.key);
            emit_byte(gen, OP_IDX_GET, node->line);
            break;
        case AST_ISSET:
            compile_expression(gen, node->as.unary.operand);
            emit_byte(gen, OP_LOAD_NULL, node->line);
            emit_byte(gen, OP_NIDENT, node->line);
            break;
        case AST_EMPTY:
            compile_expression(gen, node->as.unary.operand);
            emit_byte(gen, OP_NOT, node->line);
            break;
        case AST_UNSET:
            if (node->as.unary.operand->kind == AST_VARIABLE) {
                uint8_t slot;
                if (variable_slot(gen, node->as.unary.operand->as.literal.token, &slot)) {
                    emit_byte(gen, OP_LOAD_NULL, node->line);
                    emit_byte(gen, OP_DUP, node->line);
                    emit_byte(gen, OP_STORE_LOCAL, node->line);
                    emit_byte(gen, slot, node->line);
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
            if (gen->finally_count != 0U) {
                uint8_t slot;
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
        case AST_FUNCTION:
            break;
        case AST_CLASS:
            compile_class_definition(gen, node);
            break;
        case AST_STATIC:
            if (node->as.binding.value != NULL) {
                pc_ast variable;
                pc_ast assignment;
                memset(&variable, 0, sizeof(variable));
                memset(&assignment, 0, sizeof(assignment));
                variable.kind = AST_VARIABLE;
                variable.line = node->line;
                variable.as.literal.token = node->as.binding.name;
                assignment.kind = AST_ASSIGN;
                assignment.line = node->line;
                assignment.as.binary.op = T_EQUAL;
                assignment.as.binary.left = &variable;
                assignment.as.binary.right = node->as.binding.value;
                compile_expression(gen, &assignment);
                emit_byte(gen, OP_POP, node->line);
            }
            break;
        case AST_GLOBAL:
            break;
        case AST_CONST:
        case AST_INCLUDE:
            fail(gen, node->line, "%s requires a later runtime milestone",
                 pc_ast_kind_name(node->kind));
            break;
        default:
            fail(gen, node->line, "statement node %s is not executable",
                 pc_ast_kind_name(node->kind));
            break;
    }
}

static int compile_function(generator *gen, const pc_ast *function) {
    pproto *proto = pproto_new(function->as.function.name.start,
                               function->as.function.name.length);
    const pc_ast *parameter;
    generator child;
    int saw_default = 0;
    if (proto == NULL || !pmodule_add(gen->module, proto)) {
        pproto_destroy(proto);
        fail(gen, function->line, "out of memory while creating function");
        return 0;
    }
    parameter = function->as.function.parameters;
    while (parameter != NULL) {
        uint8_t slot;
        pc_token name = parameter->as.parameter.name;
        if (name.length != 0U && name.start[0] == '$') {
            name.start++;
            name.length--;
        }
        if (!pproto_add_local(proto, name.start, name.length, &slot)) {
            fail(gen, parameter->line, "too many function parameters");
            return 0;
        }
        proto->n_params++;
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

static int compile_method(generator *gen, pc_token class_name,
                          const pc_ast *method) {
    size_t qualified_length = class_name.length + 2U + method->as.function.name.length;
    char *qualified = pphp_alloc(qualified_length);
    pproto *proto;
    const pc_ast *parameter;
    generator child;
    int saw_default = 0;
    uint8_t ignored;
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
        if (name.length != 0U && name.start[0] == '$') {
            name.start++;
            name.length--;
        }
        if (!pproto_add_local(proto, name.start, name.length, &slot)) {
            fail(gen, parameter->line, "too many method parameters");
            return 0;
        }
        proto->n_params++;
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
            memcmp(name->data, class_name.start, class_name.length) == 0 &&
            memcmp(name->data + class_name.length, "::", 2U) == 0 &&
            memcmp(name->data + class_name.length + 2U, method_name.start,
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
    if (class_node->as.class_decl.parent.length != 0U) {
        parent_name = name_constant(gen, class_node->as.class_decl.parent);
    }
    emit_byte(gen, OP_DEF_CLASS, class_node->line);
    emit_u16(gen, class_name, class_node->line);
    emit_u16(gen, parent_name, class_node->line);
    emit_byte(gen, class_node->as.class_decl.flags, class_node->line);
    while (member != NULL && !gen->failed) {
        if (member->kind == AST_PROPERTY) {
            uint16_t name;
            pc_token property_name = member->as.property.name;
            if ((member->as.property.flags & PC_MOD_STATIC) != 0U) {
                fail(gen, member->line, "static properties are not supported yet");
                break;
            }
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
            emit_byte(gen, member->as.property.flags, member->line);
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
        }
        member = member->next;
    }
    emit_byte(gen, OP_DEF_END, class_node->line);
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
    statement = program->as.list.items;
    while (statement != NULL && !gen.failed) {
        if (statement->kind == AST_FUNCTION) {
            size_t i;
            for (i = 1U; i < module->count; i++) {
                if (token_equal(statement->as.function.name, module->protos[i]->name)) {
                    fail(&gen, statement->line, "function %.*s is already defined",
                         (int)statement->as.function.name.length,
                         statement->as.function.name.start);
                    break;
                }
            }
            if (!gen.failed) {
                (void)compile_function(&gen, statement);
            }
        } else if (statement->kind == AST_CLASS) {
            const pc_ast *member = statement->as.class_decl.members;
            while (member != NULL && !gen.failed) {
                if (member->kind == AST_FUNCTION) {
                    (void)compile_method(&gen, statement->as.class_decl.name, member);
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
