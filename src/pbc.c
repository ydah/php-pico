#include "pbc.h"

#include "opcode.h"
#include "pphp/fs.h"
#include "pphp/pphp.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int grow_array(void **array, size_t element_size, size_t *capacity,
                      size_t minimum) {
    size_t next = *capacity == 0U ? 8U : *capacity * 2U;
    void *resized;
    if (next < minimum) {
        next = minimum;
    }
    if (next > SIZE_MAX / element_size) {
        return 0;
    }
    resized = pphp_realloc(*array, next * element_size);
    if (resized == NULL) {
        return 0;
    }
    *array = resized;
    *capacity = next;
    return 1;
}

pproto *pproto_new(const char *name, size_t length) {
    pproto *proto = pphp_alloc(sizeof(*proto));
    if (proto == NULL) {
        return NULL;
    }
    memset(proto, 0, sizeof(*proto));
    proto->owns_code = 1U;
    proto->name = ps_new(name == NULL ? "" : name, name == NULL ? 0U : length);
    if (proto->name == NULL) {
        pphp_free(proto);
        return NULL;
    }
    return proto;
}

int pproto_emit_u8(pproto *proto, uint8_t value) {
    if (proto->code_length == proto->code_capacity &&
        !grow_array((void **)&proto->code, sizeof(*proto->code),
                    &proto->code_capacity, proto->code_length + 1U)) {
        return 0;
    }
    proto->code[proto->code_length++] = value;
    return 1;
}

int pproto_emit_u16(pproto *proto, uint16_t value) {
    return pproto_emit_u8(proto, (uint8_t)(value & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)(value >> 8U));
}

int pproto_emit_i32(pproto *proto, int32_t value) {
    uint32_t bits = (uint32_t)value;
    return pproto_emit_u8(proto, (uint8_t)(bits & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)((bits >> 8U) & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)((bits >> 16U) & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)((bits >> 24U) & 0xffU));
}

int pproto_patch_i16(pproto *proto, size_t operand_offset, size_t target) {
    ptrdiff_t relative;
    int16_t encoded;
    if (operand_offset + 2U > proto->code_length) {
        return 0;
    }
    relative = (ptrdiff_t)target - (ptrdiff_t)(operand_offset + 2U);
    if (relative < INT16_MIN || relative > INT16_MAX) {
        return 0;
    }
    encoded = (int16_t)relative;
    proto->code[operand_offset] = (uint8_t)((uint16_t)encoded & 0xffU);
    proto->code[operand_offset + 1U] = (uint8_t)((uint16_t)encoded >> 8U);
    return 1;
}

int pproto_add_constant(pproto *proto, pvalue value, uint16_t *index) {
    if (proto->constant_count >= UINT16_MAX) {
        return 0;
    }
    if (proto->constant_count == proto->constant_capacity &&
        !grow_array((void **)&proto->constants, sizeof(*proto->constants),
                    &proto->constant_capacity, proto->constant_count + 1U)) {
        return 0;
    }
    pv_retain(value);
    proto->constants[proto->constant_count] = value;
    *index = (uint16_t)proto->constant_count;
    proto->constant_count++;
    return 1;
}

int pproto_find_local(const pproto *proto, const char *name, size_t length,
                      uint8_t *slot) {
    size_t i;
    for (i = 0U; i < proto->n_locals; i++) {
        if (ps_equal_bytes(proto->locals[i], name, length)) {
            *slot = (uint8_t)i;
            return 1;
        }
    }
    return 0;
}

int pproto_add_local(pproto *proto, const char *name, size_t length, uint8_t *slot) {
    pstring **resized;
    pstring *string;
    if (pproto_find_local(proto, name, length, slot)) {
        return 1;
    }
    if (proto->n_locals == UINT8_MAX) {
        return 0;
    }
    resized = pphp_realloc(proto->locals,
                           ((size_t)proto->n_locals + 1U) * sizeof(*proto->locals));
    if (resized == NULL) {
        return 0;
    }
    proto->locals = resized;
    string = ps_new(name, length);
    if (string == NULL) {
        return 0;
    }
    *slot = proto->n_locals;
    proto->locals[proto->n_locals++] = string;
    return 1;
}

int pproto_add_catch(pproto *proto, pcatch entry) {
    if (proto->catch_count >= UINT16_MAX) return 0;
    if (proto->catch_count == proto->catch_capacity &&
        !grow_array((void **)&proto->catches, sizeof(*proto->catches),
                    &proto->catch_capacity, proto->catch_count + 1U)) return 0;
    proto->catches[proto->catch_count++] = entry;
    return 1;
}

int pmodule_add(pmodule *module, pproto *proto) {
    if (module->count == module->capacity &&
        !grow_array((void **)&module->protos, sizeof(*module->protos),
                    &module->capacity, module->count + 1U)) {
        return 0;
    }
    if (module->count == 0U && proto->role == PPROTO_FUNCTION) {
        proto->role = PPROTO_MAIN;
    }
    module->protos[module->count++] = proto;
    return 1;
}

const pproto *pmodule_find(const pmodule *module, const pstring *name) {
    size_t i;
    size_t j;
    for (i = 1U; i < module->count; i++) {
        const pstring *candidate = module->protos[i]->name;
        if (module->protos[i]->conditional ||
            module->protos[i]->role != PPROTO_FUNCTION) continue;
        int equal = candidate->length == name->length;
        for (j = 0U; equal && j < name->length; j++) {
            unsigned char left = (unsigned char)ps_data(candidate)[j];
            unsigned char right = (unsigned char)ps_data(name)[j];
            if (left >= 'A' && left <= 'Z') {
                left = (unsigned char)(left + ('a' - 'A'));
            }
            if (right >= 'A' && right <= 'Z') {
                right = (unsigned char)(right + ('a' - 'A'));
            }
            equal = left == right;
        }
        if (equal) {
            return module->protos[i];
        }
    }
    return NULL;
}

typedef struct string_list {
    pstring **items;
    size_t count;
    size_t capacity;
} string_list;

static size_t align4(size_t value) {
    return (value + 3U) & ~(size_t)3U;
}

static int size_add(size_t *value, size_t amount) {
    if (value == NULL || amount > SIZE_MAX - *value) return 0;
    *value += amount;
    return 1;
}

static int size_align4(size_t *value) {
    if (value == NULL || *value > SIZE_MAX - 3U) return 0;
    *value = align4(*value);
    return 1;
}

static void put_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)(value & 0xffU);
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)(value & 0xffU);
    bytes[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
    bytes[offset + 2U] = (uint8_t)((value >> 16U) & 0xffU);
    bytes[offset + 3U] = (uint8_t)((value >> 24U) & 0xffU);
}

static uint16_t get_u16(const uint8_t *bytes, size_t offset) {
    return (uint16_t)((uint16_t)bytes[offset] |
                      (uint16_t)((uint16_t)bytes[offset + 1U] << 8U));
}

static uint32_t get_u32(const uint8_t *bytes, size_t offset) {
    return (uint32_t)bytes[offset] |
           ((uint32_t)bytes[offset + 1U] << 8U) |
           ((uint32_t)bytes[offset + 2U] << 16U) |
           ((uint32_t)bytes[offset + 3U] << 24U);
}

static int pbc_range_available(size_t offset, size_t need, size_t length) {
    return offset <= length && need <= length - offset;
}

static int pbc_advance(size_t *offset, size_t need, size_t length) {
    if (!pbc_range_available(*offset, need, length)) return 0;
    *offset += need;
    return 1;
}

static int pbc_string_constant(const pproto *proto, uint16_t index) {
    return index < proto->constant_count &&
           proto->constants[index].type == PT_STRING;
}

static int pbc_flag_visibility(uint8_t flags) {
    uint8_t visibility = flags & (uint8_t)(1U | 2U | 4U);
    return visibility == 1U || visibility == 2U || visibility == 4U;
}

static int pbc_identifier_bytes(const char *bytes, size_t length) {
    size_t i;
    unsigned char byte;
    if (length == 0U) return 0;
    byte = (unsigned char)bytes[0];
    if (!((byte >= 'A' && byte <= 'Z') ||
          (byte >= 'a' && byte <= 'z') || byte == '_' || byte >= 0x80U)) {
        return 0;
    }
    for (i = 1U; i < length; i++) {
        byte = (unsigned char)bytes[i];
        if (!((byte >= 'A' && byte <= 'Z') ||
              (byte >= 'a' && byte <= 'z') ||
              (byte >= '0' && byte <= '9') || byte == '_' ||
              byte >= 0x80U)) {
            return 0;
        }
    }
    return 1;
}

#if PPHP_TYPECHECK
static int pbc_bytes_equal_ci(const char *left, size_t left_length,
                              const char *right, size_t right_length) {
    size_t i;
    if (left_length != right_length) return 0;
    for (i = 0U; i < left_length; i++) {
        unsigned char a = (unsigned char)left[i];
        unsigned char b = (unsigned char)right[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
        if (a != b) return 0;
    }
    return 1;
}

static int pbc_reserved_type_name(const char *bytes, size_t length) {
    static const char *const names[] = {
        "int", "float", "string", "bool", "array", "callable", "mixed",
        "void", "null", "self", "static", "parent"
    };
    size_t i;
    for (i = 0U; i < sizeof(names) / sizeof(names[0]); i++) {
        if (pbc_bytes_equal_ci(bytes, length, names[i], strlen(names[i]))) {
            return 1;
        }
    }
    return 0;
}

static int pbc_named_type_valid(const pstring *name) {
    return name != NULL &&
           pbc_identifier_bytes(ps_data(name), name->length) &&
           !pbc_reserved_type_name(ps_data(name), name->length);
}

static int pbc_property_type_valid(const pstring *type, int has_parent) {
    const char *bytes;
    size_t starts[16];
    size_t lengths[16];
    size_t start = 0U;
    size_t count = 0U;
    size_t i;
    size_t j;
    if (type == NULL) return 1;
    bytes = ps_data(type);
    for (i = 0U; i <= type->length; i++) {
        if (i != type->length && bytes[i] != '|') continue;
        if (i == start || count == 16U) return 0;
        starts[count] = start;
        lengths[count] = i - start;
        for (j = 0U; j < count; j++) {
            if (pbc_bytes_equal_ci(bytes + starts[j], lengths[j],
                                   bytes + start, i - start)) return 0;
        }
        count++;
        start = i + 1U;
    }
    for (i = 0U; i < count; i++) {
        const char *name = bytes + starts[i];
        size_t length = lengths[i];
        int reserved = pbc_reserved_type_name(name, length);
        if (pbc_bytes_equal_ci(name, length, "void", 4U) ||
            pbc_bytes_equal_ci(name, length, "static", 6U) ||
            pbc_bytes_equal_ci(name, length, "callable", 8U) ||
            (!has_parent && pbc_bytes_equal_ci(name, length, "parent", 6U)) ||
            (count > 1U && pbc_bytes_equal_ci(name, length, "mixed", 5U)) ||
            (!PPHP_ENABLE_FLOAT &&
             pbc_bytes_equal_ci(name, length, "float", 5U))) return 0;
        if (!reserved && !pbc_identifier_bytes(name, length)) return 0;
    }
    return count != 0U;
}
#endif

static size_t pbc_instruction_size(const pproto *proto, size_t pc) {
    uint8_t opcode;
    size_t operands = 0U;
    if (pc >= proto->code_length) return 0U;
    opcode = proto->code[pc++];
    switch ((pphp_opcode)opcode) {
        case OP_LOAD_I8: case OP_LOAD_LOCAL: case OP_LOAD_LOCAL_QUIET:
        case OP_STORE_LOCAL: case OP_UNSET_LOCAL: case OP_BIND_GLOBAL:
        case OP_ECHO: case OP_CALL_VALUE: case OP_INCLUDE: case OP_CAST:
        case OP_NEW_OBJ_DYNAMIC: case OP_MCALL_DYNAMIC:
            operands = 1U; break;
        case OP_LOAD_CONST: case OP_LOAD_NAMED_CONST: case OP_DEF_CONST:
        case OP_DEF_FUNC: case OP_CALL_ARRAY: case OP_MCALL_ARRAY:
        case OP_NEW_OBJ_ARRAY: case OP_DEF_CCONST: case OP_DEF_INTERFACE:
        case OP_JMP: case OP_JMP_IF: case OP_JMP_UNLESS:
        case OP_JMP_IF_KEEP: case OP_JMP_UNLESS_KEEP:
        case OP_JMP_NOTNULL_KEEP: case OP_JMP_IFNULL_KEEP: case OP_LINE:
        case OP_NEW_ARRAY: case OP_PROP_GET: case OP_PROP_GET_QUIET:
        case OP_PROP_SET: case OP_INSTANCEOF:
            operands = 2U; break;
        case OP_CALL: case OP_NEW_OBJ: case OP_MCALL: case OP_FE_NEXT:
        case OP_STATIC_INIT:
            operands = 3U; break;
        case OP_SPROP_GET: case OP_SPROP_SET: case OP_CLSCONST:
        case OP_SCALL_ARRAY: case OP_LOAD_I32:
            operands = 4U; break;
        case OP_SCALL: case OP_DEF_CLASS: case OP_DEF_METHOD:
            operands = 5U; break;
        case OP_DEF_PROP:
            operands = PPHP_TYPECHECK ? 6U : 3U; break;
        case OP_CLOSURE:
            if (!pbc_range_available(pc, 3U, proto->code_length)) return 0U;
            operands = 3U + (size_t)proto->code[pc + 2U] * 2U; break;
        case OP_NOP: case OP_HALT: case OP_POP: case OP_DUP: case OP_SWAP:
        case OP_LOAD_NULL: case OP_LOAD_TRUE: case OP_LOAD_FALSE:
        case OP_LOAD_ARGC: case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        case OP_MOD: case OP_POW: case OP_NEG: case OP_CONCAT: case OP_BAND:
        case OP_BOR: case OP_BXOR: case OP_BNOT: case OP_SHL: case OP_SHR:
        case OP_EQ: case OP_NE: case OP_IDENT: case OP_NIDENT: case OP_LT:
        case OP_LE: case OP_GT: case OP_GE: case OP_CMP: case OP_NOT:
        case OP_INSTANCEOF_DYNAMIC: case OP_RET: case OP_RET_NULL:
        case OP_CALL_VALUE_ARRAY:
#if PPHP_TYPECHECK
        case OP_TYPECHECK_ARGS:
#endif
        case OP_ARR_PUSH: case OP_ARR_SET: case OP_IDX_GET: case OP_IDX_SET:
        case OP_IDX_APPEND: case OP_FE_INIT: case OP_FE_FREE:
        case OP_ARR_EXTEND: case OP_ARR_SEPARATE: case OP_IDX_UNSET:
        case OP_IDX_GET_QUIET: case OP_NEW_OBJ_DYNAMIC_ARRAY: case OP_THROW:
        case OP_CLONE: case OP_DEF_END: case OP_PROP_GET_DYNAMIC:
        case OP_PROP_SET_DYNAMIC: case OP_PROP_GET_DYNAMIC_QUIET:
        case OP_MCALL_DYNAMIC_ARRAY:
            break;
        default: return 0U;
    }
    return pbc_range_available(pc, operands, proto->code_length)
               ? operands + 1U : 0U;
}

static int pbc_instruction_boundary(const pproto *proto, size_t target) {
    size_t pc = 0U;
    if (target >= proto->code_length) return 0;
    while (pc < target) {
        size_t size = pbc_instruction_size(proto, pc);
        if (size == 0U || size > target - pc) return 0;
        pc += size;
    }
    return pc == target;
}

static int pbc_instruction_boundary_or_end(const pproto *proto,
                                           size_t target) {
    return target == proto->code_length ||
           pbc_instruction_boundary(proto, target);
}

static int pbc_method_proto_name(const pproto *proto,
                                 const pstring *class_name,
                                 const pstring *method_name) {
    size_t class_length;
    size_t method_length;
    if (proto == NULL || class_name == NULL || method_name == NULL) return 0;
    class_length = class_name->length;
    method_length = method_name->length;
    return proto->name->length == class_length + 2U + method_length &&
           memcmp(ps_data(proto->name), ps_data(class_name), class_length) == 0 &&
           memcmp(ps_data(proto->name) + class_length, "::", 2U) == 0 &&
           memcmp(ps_data(proto->name) + class_length + 2U,
                  ps_data(method_name), method_length) == 0;
}

static int pbc_closure_proto_name(const pproto *proto, size_t index) {
    const char *bytes;
    size_t cursor = 9U;
    size_t value = 0U;
    if (proto == NULL || proto->name->length < 11U) return 0;
    bytes = ps_data(proto->name);
    if (memcmp(bytes, "{closure#", 9U) != 0 ||
        bytes[proto->name->length - 1U] != '}') return 0;
    if (bytes[cursor] == '0' && cursor + 2U < proto->name->length) return 0;
    while (cursor + 1U < proto->name->length) {
        unsigned digit;
        if (bytes[cursor] < '0' || bytes[cursor] > '9') return 0;
        digit = (unsigned)(bytes[cursor] - '0');
        if (value > (SIZE_MAX - digit) / 10U) return 0;
        value = value * 10U + digit;
        cursor++;
    }
    return value == index;
}

static int pbc_validate_code(const pmodule *module, size_t proto_index) {
    const pproto *proto = module->protos[proto_index];
    size_t pc = 0U;
    int building_class = 0;
    int class_readonly = 0;
    const pstring *building_class_name = NULL;
#if PPHP_TYPECHECK
    int class_has_parent = 0;
    size_t typecheck_pc = SIZE_MAX;
#else
    (void)proto_index;
#endif
    while (pc < proto->code_length) {
        size_t size = pbc_instruction_size(proto, pc);
        if (size == 0U) goto invalid;
        pc += size;
    }
    pc = 0U;
    while (pc < proto->code_length) {
        size_t instruction = pc;
        size_t size = pbc_instruction_size(proto, pc);
        uint8_t opcode = proto->code[pc++];
        uint16_t first = size > 2U ? get_u16(proto->code, pc) : 0U;
        size_t next = instruction + size;
        switch ((pphp_opcode)opcode) {
            case OP_LOAD_LOCAL: case OP_LOAD_LOCAL_QUIET: case OP_STORE_LOCAL:
            case OP_UNSET_LOCAL: case OP_BIND_GLOBAL:
                if (proto->code[pc] >= proto->n_locals) goto invalid;
                break;
            case OP_STATIC_INIT: {
                int16_t relative = (int16_t)get_u16(proto->code, pc + 1U);
                size_t target = (size_t)((ptrdiff_t)next + relative);
                if (proto->code[pc] >= proto->n_locals ||
                    !pbc_instruction_boundary(proto, target)) {
                    goto invalid;
                }
                break;
            }
            case OP_LOAD_CONST:
                if (first >= proto->constant_count) goto invalid;
                break;
            case OP_LOAD_NAMED_CONST: case OP_DEF_CONST: case OP_CALL_ARRAY:
            case OP_MCALL_ARRAY: case OP_NEW_OBJ_ARRAY: case OP_DEF_CCONST:
            case OP_DEF_INTERFACE: case OP_PROP_GET: case OP_PROP_GET_QUIET:
            case OP_PROP_SET: case OP_INSTANCEOF:
                if (!pbc_string_constant(proto, first)) goto invalid;
                if ((opcode == OP_DEF_CCONST || opcode == OP_DEF_INTERFACE) &&
                    !building_class) goto invalid;
                break;
            case OP_DEF_FUNC:
                if (first >= module->count ||
                    module->protos[first]->is_method ||
                    !module->protos[first]->conditional) goto invalid;
                break;
            case OP_CALL: case OP_NEW_OBJ: case OP_MCALL:
                if (!pbc_string_constant(proto, first) ||
                    proto->code[pc + 2U] > 31U) goto invalid;
                break;
            case OP_CALL_VALUE: case OP_NEW_OBJ_DYNAMIC:
            case OP_MCALL_DYNAMIC:
                if (proto->code[pc] > 31U) goto invalid;
                break;
            case OP_SCALL: case OP_SCALL_ARRAY: case OP_SPROP_GET:
            case OP_SPROP_SET: case OP_CLSCONST:
                if (!pbc_string_constant(proto, first) ||
                    !pbc_string_constant(proto, get_u16(proto->code, pc + 2U)) ||
                    (opcode == OP_SCALL && proto->code[pc + 4U] > 31U)) {
                    goto invalid;
                }
                break;
            case OP_JMP: case OP_JMP_IF: case OP_JMP_UNLESS:
            case OP_JMP_IF_KEEP: case OP_JMP_UNLESS_KEEP:
            case OP_JMP_NOTNULL_KEEP: case OP_JMP_IFNULL_KEEP: {
                int16_t relative = (int16_t)first;
                size_t target = (size_t)((ptrdiff_t)next + relative);
                if (!pbc_instruction_boundary(proto, target)) {
                    goto invalid;
                }
                break;
            }
            case OP_FE_NEXT: {
                int16_t relative = (int16_t)first;
                size_t target = (size_t)((ptrdiff_t)next + relative);
                if (proto->code[pc + 2U] > 1U ||
                    !pbc_instruction_boundary(proto, target)) {
                    goto invalid;
                }
                break;
            }
            case OP_INCLUDE:
                if (proto->code[pc] != 113U && proto->code[pc] != 114U &&
                    proto->code[pc] != 129U && proto->code[pc] != 130U) {
                    goto invalid;
                }
                break;
            case OP_CAST:
                if (proto->code[pc] != PT_INT && proto->code[pc] != PT_STRING &&
                    proto->code[pc] != PT_TRUE && proto->code[pc] != PT_ARRAY
#if PPHP_ENABLE_FLOAT
                    && proto->code[pc] != PT_FLOAT
#endif
                ) goto invalid;
                break;
            case OP_DEF_CLASS: {
                uint16_t parent = get_u16(proto->code, pc + 2U);
                uint8_t flags = proto->code[pc + 4U];
                if (building_class || !pbc_string_constant(proto, first) ||
                    (parent != UINT16_MAX &&
                     !pbc_string_constant(proto, parent)) ||
                    (flags & (uint8_t)~(16U | 32U | 64U | 128U)) != 0U ||
                    (flags & (16U | 32U)) == (16U | 32U) ||
                    ((flags & 128U) != 0U &&
                     ((flags & 16U) == 0U || (flags & (32U | 64U)) != 0U))) {
                    goto invalid;
                }
                building_class = 1;
                building_class_name = (const pstring *)
                    proto->constants[first].as.gc;
                class_readonly = (flags & 64U) != 0U;
#if PPHP_TYPECHECK
                class_has_parent = parent != UINT16_MAX;
#endif
                break;
            }
            case OP_DEF_METHOD: {
                uint16_t target = get_u16(proto->code, pc + 2U);
                uint8_t flags = proto->code[pc + 4U];
                const pstring *method_name = pbc_string_constant(proto, first)
                    ? (const pstring *)proto->constants[first].as.gc : NULL;
                if (!building_class || !pbc_string_constant(proto, first) ||
                    target == 0U || target >= module->count ||
                    module->protos[target]->conditional ||
                    !pbc_method_proto_name(module->protos[target],
                                           building_class_name, method_name) ||
                    !pbc_flag_visibility(flags) ||
                    (flags & (uint8_t)~(1U | 2U | 4U | 8U | 16U | 32U)) != 0U ||
                    (flags & (16U | 32U)) == (16U | 32U) ||
                    (((flags & 8U) == 0U) !=
                     (module->protos[target]->is_method != 0U))) goto invalid;
                break;
            }
            case OP_DEF_PROP: {
                uint8_t flags = proto->code[pc + 2U];
                if (!building_class || !pbc_string_constant(proto, first) ||
                    !pbc_flag_visibility(flags) ||
                    (flags & (uint8_t)~(1U | 2U | 4U | 8U | 64U)) != 0U ||
                    (flags & (8U | 64U)) == (8U | 64U) ||
                    (class_readonly && (flags & 64U) == 0U)) goto invalid;
#if PPHP_TYPECHECK
                {
                    uint16_t type_index = get_u16(proto->code, pc + 3U);
                    uint8_t has_default = proto->code[pc + 5U];
                    const pstring *type = NULL;
                    if (has_default > 1U ||
                        ((flags & 64U) != 0U && has_default != 0U) ||
                        ((flags & 64U) != 0U && type_index == UINT16_MAX) ||
                        (type_index != UINT16_MAX &&
                         !pbc_string_constant(proto, type_index)) ||
                        ((type_index != UINT16_MAX) &&
                         (type = (const pstring *)
                              proto->constants[type_index].as.gc) == NULL) ||
                        !pbc_property_type_valid(type, class_has_parent)) {
                        goto invalid;
                    }
                }
#endif
                break;
            }
            case OP_DEF_END:
                if (!building_class) goto invalid;
                building_class = 0;
                class_readonly = 0;
                building_class_name = NULL;
#if PPHP_TYPECHECK
                class_has_parent = 0;
#endif
                break;
            case OP_CLOSURE: {
                uint8_t count = proto->code[pc + 2U];
                size_t capture;
                if (first == 0U || first >= module->count ||
                    module->protos[first]->conditional ||
                    !pbc_closure_proto_name(module->protos[first], first) ||
                    (size_t)module->protos[first]->n_params + count >
                        module->protos[first]->n_locals) goto invalid;
                for (capture = 0U; capture < count; capture++) {
                    if (proto->code[pc + 3U + capture * 2U] != 0U ||
                        proto->code[pc + 4U + capture * 2U] >=
                            proto->n_locals) goto invalid;
                }
                break;
            }
#if PPHP_TYPECHECK
            case OP_TYPECHECK_ARGS:
                if (typecheck_pc != SIZE_MAX) goto invalid;
                typecheck_pc = instruction;
                break;
#endif
            default: break;
        }
        pc = next;
    }
    if (building_class) goto invalid;
    for (pc = 0U; pc < proto->catch_count; pc++) {
        const pcatch *entry = &proto->catches[pc];
        if (!pbc_instruction_boundary(proto, entry->try_start) ||
            !pbc_instruction_boundary_or_end(proto, entry->try_end) ||
            !pbc_instruction_boundary(proto, entry->handler_pc)) {
            goto invalid;
        }
    }
#if PPHP_TYPECHECK
    if (proto_index == 0U) {
        if (typecheck_pc != SIZE_MAX) goto invalid;
    } else {
        size_t cursor = 0U;
        size_t fixed = proto->variadic ? (size_t)proto->n_params - 1U
                                       : proto->n_params;
        size_t parameter;
        for (parameter = proto->n_required; parameter < fixed; parameter++) {
            size_t target;
            int16_t relative;
            if (!pbc_range_available(cursor, 7U, proto->code_length) ||
                proto->code[cursor] != OP_LOAD_ARGC ||
                proto->code[cursor + 1U] != OP_LOAD_I8 ||
                proto->code[cursor + 2U] != parameter ||
                proto->code[cursor + 3U] != OP_GT ||
                proto->code[cursor + 4U] != OP_JMP_IF) goto invalid;
            relative = (int16_t)get_u16(proto->code, cursor + 5U);
            target = (size_t)((ptrdiff_t)(cursor + 7U) + relative);
            if (target < cursor + 9U || target > proto->code_length ||
                proto->code[target - 2U] != OP_STORE_LOCAL ||
                proto->code[target - 1U] !=
                    parameter + (proto->is_method ? 1U : 0U)) goto invalid;
            cursor = target;
        }
        if (typecheck_pc != cursor) goto invalid;
    }
#endif
    return 1;
invalid:
    return 0;
}

static int pbc_assign_proto_roles(pmodule *module) {
    enum {
        PBC_REF_FUNCTION = 1,
        PBC_REF_METHOD = 2,
        PBC_REF_CLOSURE = 4
    };
    size_t source;
    size_t target;
    for (target = 0U; target < module->count; target++) {
        module->protos[target]->role = 0U;
    }
    for (source = 0U; source < module->count; source++) {
        const pproto *proto = module->protos[source];
        size_t pc = 0U;
        while (pc < proto->code_length) {
            uint8_t opcode = proto->code[pc];
            size_t size = pbc_instruction_size(proto, pc);
            uint16_t referenced = UINT16_MAX;
            uint8_t reference_role = 0U;
            if (opcode == OP_DEF_FUNC || opcode == OP_CLOSURE) {
                referenced = get_u16(proto->code, pc + 1U);
                reference_role = opcode == OP_DEF_FUNC
                    ? PBC_REF_FUNCTION : PBC_REF_CLOSURE;
            } else if (opcode == OP_DEF_METHOD) {
                referenced = get_u16(proto->code, pc + 3U);
                reference_role = PBC_REF_METHOD;
            }
            if (reference_role != 0U) {
                if ((module->protos[referenced]->role & reference_role) != 0U) {
                    return 0;
                }
                module->protos[referenced]->role |= reference_role;
            }
            pc += size;
        }
    }
    for (target = 0U; target < module->count; target++) {
        uint8_t references = module->protos[target]->role;
        if (target == 0U) {
            if (references != 0U) return 0;
            module->protos[target]->role = PPROTO_MAIN;
        } else if (references == PBC_REF_METHOD) {
            module->protos[target]->role = PPROTO_METHOD;
        } else if (references == PBC_REF_CLOSURE) {
            if (module->protos[target]->is_method) return 0;
            module->protos[target]->role = PPROTO_CLOSURE;
        } else if (references == PBC_REF_FUNCTION || references == 0U) {
            if ((references == PBC_REF_FUNCTION) !=
                    (module->protos[target]->conditional != 0U) ||
                module->protos[target]->is_method ||
                !pbc_identifier_bytes(ps_data(module->protos[target]->name),
                                      module->protos[target]->name->length)) {
                return 0;
            }
            module->protos[target]->role = PPROTO_FUNCTION;
        } else {
            return 0;
        }
    }
    return 1;
}

#if PPHP_TYPECHECK
static int pbc_validate_type_context(const pmodule *module) {
    enum {
        PBC_CONTEXT_SCOPE_MASK = 3,
        PBC_CONTEXT_METHOD = 4,
        PBC_CONTEXT_CONSTRUCTOR = 8,
        PBC_CONTEXT_DESTRUCTOR = 16,
        PBC_CONTEXT_CLOSURE = 32
    };
    uint8_t *contexts = pphp_alloc(module->count);
    size_t i;
    int changed = 1;
    if (contexts == NULL) return PPHP_E_NOMEM;
    memset(contexts, 0, module->count);
    for (i = 0U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        size_t pc = 0U;
        uint8_t scope = 0U;
        while (pc < proto->code_length) {
            uint8_t opcode = proto->code[pc];
            size_t size = pbc_instruction_size(proto, pc);
            if (opcode == OP_DEF_CLASS) {
                scope = get_u16(proto->code, pc + 3U) == UINT16_MAX ? 1U : 2U;
            } else if (opcode == OP_DEF_METHOD) {
                uint16_t target = get_u16(proto->code, pc + 3U);
                uint16_t name_index = get_u16(proto->code, pc + 1U);
                const pstring *name = (const pstring *)
                    proto->constants[name_index].as.gc;
                uint8_t role = pbc_bytes_equal_ci(
                                   ps_data(name), name->length,
                                   "__construct", 11U)
                                   ? PBC_CONTEXT_CONSTRUCTOR :
                               pbc_bytes_equal_ci(
                                   ps_data(name), name->length,
                                   "__destruct", 10U)
                                   ? PBC_CONTEXT_DESTRUCTOR
                                   : PBC_CONTEXT_METHOD;
                uint8_t target_scope = contexts[target] &
                    PBC_CONTEXT_SCOPE_MASK;
                if ((contexts[target] & (uint8_t)~PBC_CONTEXT_SCOPE_MASK) != 0U ||
                    (target_scope != 0U && target_scope != scope)) goto invalid;
                contexts[target] = (uint8_t)(scope | role);
            } else if (opcode == OP_CLOSURE) {
                uint16_t target = get_u16(proto->code, pc + 1U);
                contexts[target] |= PBC_CONTEXT_CLOSURE;
            } else if (opcode == OP_DEF_END) {
                scope = 0U;
            }
            pc += size;
        }
    }
    while (changed) {
        changed = 0;
        for (i = 0U; i < module->count; i++) {
            const pproto *proto;
            size_t pc;
            uint8_t scope = contexts[i] & PBC_CONTEXT_SCOPE_MASK;
            if (scope == 0U) continue;
            proto = module->protos[i];
            pc = 0U;
            while (pc < proto->code_length) {
                size_t size = pbc_instruction_size(proto, pc);
                if (proto->code[pc] == OP_CLOSURE) {
                    uint16_t target = get_u16(proto->code, pc + 1U);
                    uint8_t target_scope = contexts[target] &
                        PBC_CONTEXT_SCOPE_MASK;
                    if ((contexts[target] & PBC_CONTEXT_CLOSURE) == 0U ||
                        (target_scope != 0U && target_scope != scope)) {
                        goto invalid;
                    }
                    if (target_scope == 0U) {
                        contexts[target] = (uint8_t)(
                            contexts[target] | scope);
                        changed = 1;
                    }
                }
                pc += size;
            }
        }
    }
    for (i = 0U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        size_t spec_index;
        uint8_t scope = contexts[i] & PBC_CONTEXT_SCOPE_MASK;
        uint8_t role = contexts[i] & (uint8_t)~PBC_CONTEXT_SCOPE_MASK;
        int no_value_return = (role == PBC_CONTEXT_CONSTRUCTOR ||
            role == PBC_CONTEXT_DESTRUCTOR ||
            (proto->return_type.count == 1U &&
             proto->return_type.members[0].kind == PTYPE_VOID));
        if ((role == PBC_CONTEXT_CONSTRUCTOR ||
             role == PBC_CONTEXT_DESTRUCTOR) &&
            !(proto->return_type.count == 0U ||
              (proto->return_type.count == 1U &&
               proto->return_type.members[0].kind == PTYPE_VOID))) goto invalid;
        for (spec_index = 0U; spec_index < (size_t)proto->n_params + 1U;
             spec_index++) {
            const ptype_spec *spec = spec_index < proto->n_params
                                         ? &proto->parameter_types[spec_index]
                                         : &proto->return_type;
            size_t member;
            for (member = 0U; member < spec->count; member++) {
                uint8_t kind = spec->members[member].kind;
                if ((kind == PTYPE_SELF || kind == PTYPE_PARENT) &&
                    scope == 0U) goto invalid;
                if (kind == PTYPE_PARENT && scope != 2U) goto invalid;
                if (kind == PTYPE_STATIC &&
                    (spec_index < proto->n_params ||
                     role == 0U || role == PBC_CONTEXT_CLOSURE)) goto invalid;
            }
        }
        if (no_value_return) {
            size_t pc = 0U;
            while (pc < proto->code_length) {
                if (proto->code[pc] == OP_RET) goto invalid;
                pc += pbc_instruction_size(proto, pc);
            }
        }
    }
    pphp_free(contexts);
    return PPHP_OK;
invalid:
    pphp_free(contexts);
    return PPHP_E_PARSE;
}
#endif

static int strings_add(string_list *list, pstring *string, uint16_t *index) {
    size_t i;
    for (i = 0U; i < list->count; i++) {
        if (ps_equal(list->items[i], string)) {
            *index = (uint16_t)i;
            return 1;
        }
    }
    if (list->count >= UINT16_MAX) {
        return 0;
    }
    if (list->count == list->capacity &&
        !grow_array((void **)&list->items, sizeof(*list->items), &list->capacity,
                    list->count + 1U)) {
        return 0;
    }
    list->items[list->count] = string;
    *index = (uint16_t)list->count;
    list->count++;
    return 1;
}

static int collect_strings(const pmodule *module, string_list *strings) {
    size_t i;
    memset(strings, 0, sizeof(*strings));
    for (i = 0U; i < module->count; i++) {
        size_t j;
        uint16_t ignored;
        if (!strings_add(strings, module->protos[i]->name, &ignored)) {
            return 0;
        }
        for (j = 0U; j < module->protos[i]->constant_count; j++) {
            pvalue value = module->protos[i]->constants[j];
            if (value.type == PT_STRING &&
                !strings_add(strings, (pstring *)value.as.gc, &ignored)) {
                return 0;
            }
        }
        for (j = 0U; j < module->protos[i]->n_locals; j++) {
            if (!strings_add(strings, module->protos[i]->locals[j], &ignored)) {
                return 0;
            }
        }
#if PPHP_TYPECHECK
        for (j = 0U; j < module->protos[i]->n_params + 1U; j++) {
            const ptype_spec *spec;
            size_t k;
            if (j < module->protos[i]->n_params &&
                module->protos[i]->parameter_types == NULL) continue;
            spec = j < module->protos[i]->n_params
                       ? &module->protos[i]->parameter_types[j]
                       : &module->protos[i]->return_type;
            for (k = 0U; k < spec->count; k++) {
                if (spec->members[k].kind == PTYPE_NAMED &&
                    !strings_add(strings, spec->members[k].name, &ignored)) {
                    return 0;
                }
            }
        }
#endif
    }
    return 1;
}

static size_t serialized_constant_size(pvalue value) {
    if (value.type == PT_INT && PPHP_INT64 &&
        (value.as.i < INT32_MIN || value.as.i > INT32_MAX)) return 16U;
#if PPHP_ENABLE_FLOAT
    return value.type == PT_FLOAT && sizeof(pphp_float) == 8U ? 16U : 8U;
#else
    (void)value;
    return 8U;
#endif
}

static int string_index(const string_list *strings, const pstring *string,
                        uint16_t *index) {
    size_t i;
    for (i = 0U; i < strings->count; i++) {
        if (ps_equal(strings->items[i], string)) {
            *index = (uint16_t)i;
            return 1;
        }
    }
    return 0;
}

int pphp_pbc_write_file(const pmodule *module, const char *path) {
    string_list strings;
    uint32_t *string_offsets = NULL;
    uint32_t *proto_offsets = NULL;
    size_t total;
    size_t offset;
    size_t i;
    uint8_t *bytes = NULL;
    pphp_file *file = NULL;
    int result = PPHP_E_NOMEM;
    if (module == NULL || module->count == 0U || module->count > UINT16_MAX ||
        path == NULL || !collect_strings(module, &strings)) {
        return PPHP_E_NOMEM;
    }
    string_offsets = pphp_alloc(strings.count * sizeof(*string_offsets));
    proto_offsets = pphp_alloc(module->count * sizeof(*proto_offsets));
    if ((strings.count != 0U && string_offsets == NULL) || proto_offsets == NULL) {
        goto done;
    }
    total = 16U;
    if (!size_add(&total, strings.count * 4U) ||
        !size_add(&total, module->count * 4U) || !size_align4(&total)) {
        goto done;
    }
    for (i = 0U; i < strings.count; i++) {
        if (total > UINT32_MAX) goto done;
        string_offsets[i] = (uint32_t)total;
        if (!size_add(&total, 3U + strings.items[i]->length) ||
            !size_align4(&total)) goto done;
    }
    for (i = 0U; i < module->count; i++) {
        size_t j;
        const pproto *proto = module->protos[i];
        if (proto->code_length > UINT16_MAX ||
            proto->constant_count > UINT16_MAX ||
            proto->catch_count > UINT16_MAX ||
            total > UINT32_MAX) goto done;
        proto_offsets[i] = (uint32_t)total;
        if (!size_add(&total, 16U) ||
            !size_add(&total, align4(proto->code_length))) goto done;
        for (j = 0U; j < proto->constant_count; j++) {
            if (!size_add(&total,
                          serialized_constant_size(proto->constants[j]))) {
                goto done;
            }
        }
        if (!size_add(&total, (size_t)proto->n_locals * 2U) ||
            !size_add(&total, proto->catch_count * 10U) ||
#if PPHP_TYPECHECK
            !size_add(&total, (size_t)proto->n_params + 1U)) goto done;
        {
            size_t type_index;
            for (type_index = 0U; type_index < (size_t)proto->n_params + 1U;
                 type_index++) {
                const ptype_spec *spec = type_index < proto->n_params &&
                                                 proto->parameter_types != NULL
                                             ? &proto->parameter_types[type_index]
                                             : type_index == proto->n_params
                                                   ? &proto->return_type : NULL;
                if (spec != NULL && !size_add(
                        &total, (size_t)spec->count * 3U)) goto done;
            }
        }
#else
            0) goto done;
#endif
        if (!size_align4(&total)) goto done;
    }
    if (total > UINT32_MAX) goto done;
    bytes = pphp_alloc(total);
    if (bytes == NULL) goto done;
    memset(bytes, 0, total);
    memcpy(bytes, "PPBC", 4U);
    put_u16(bytes, 4U, (uint16_t)PPHP_PBC_FORMAT_VERSION);
    put_u16(bytes, 6U, (uint16_t)((PPHP_INT64 ? 1U : 0U) |
                                  (PPHP_USE_DOUBLE ? 2U : 0U) |
                                  (PPHP_LINE_INFO ? 4U : 0U) |
                                  (PPHP_TYPECHECK ? 8U : 0U) |
                                  (PPHP_ENABLE_FLOAT ? 16U : 0U)));
    put_u32(bytes, 8U, (uint32_t)total);
    put_u16(bytes, 12U, (uint16_t)module->count);
    put_u16(bytes, 14U, (uint16_t)strings.count);
    offset = 16U;
    for (i = 0U; i < strings.count; i++, offset += 4U) {
        put_u32(bytes, offset, string_offsets[i]);
    }
    for (i = 0U; i < module->count; i++, offset += 4U) {
        put_u32(bytes, offset, proto_offsets[i]);
    }
    for (i = 0U; i < strings.count; i++) {
        offset = string_offsets[i];
        put_u16(bytes, offset, strings.items[i]->length);
        memcpy(bytes + offset + 2U, ps_data(strings.items[i]),
               strings.items[i]->length);
    }
    for (i = 0U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        uint16_t name_sid;
        size_t j;
        offset = proto_offsets[i];
        if (!string_index(&strings, proto->name, &name_sid)) goto done;
        bytes[offset] = proto->n_params;
        bytes[offset + 1U] = proto->n_required;
        bytes[offset + 2U] = (uint8_t)((proto->variadic ? 1U : 0U) |
                                        (proto->is_method ? 2U : 0U) |
                                        (proto->conditional ? 8U : 0U));
        bytes[offset + 3U] = proto->n_locals;
        put_u16(bytes, offset + 6U, proto->max_stack);
        put_u16(bytes, offset + 8U, (uint16_t)proto->code_length);
        put_u16(bytes, offset + 10U, (uint16_t)proto->constant_count);
        put_u16(bytes, offset + 12U, (uint16_t)proto->catch_count);
        put_u16(bytes, offset + 14U, name_sid);
        offset += 16U;
        memcpy(bytes + offset, proto->code, proto->code_length);
        offset += align4(proto->code_length);
        for (j = 0U; j < proto->constant_count; j++) {
            pvalue value = proto->constants[j];
            if (value.type == PT_INT) {
                if (PPHP_INT64 &&
                    (value.as.i < INT32_MIN || value.as.i > INT32_MAX)) {
                    uint64_t bits = (uint64_t)value.as.i;
                    bytes[offset] = 4U;
                    put_u32(bytes, offset + 8U,
                            (uint32_t)(bits & UINT32_MAX));
                    put_u32(bytes, offset + 12U,
                            (uint32_t)(bits >> 32U));
                    offset += 16U;
                } else {
                    bytes[offset] = 0U;
                    put_u32(bytes, offset + 4U,
                            (uint32_t)(int32_t)value.as.i);
                    offset += 8U;
                }
#if PPHP_ENABLE_FLOAT
            } else if (value.type == PT_FLOAT && sizeof(pphp_float) == 4U) {
                uint32_t bits;
                float number = (float)value.as.f;
                memcpy(&bits, &number, sizeof(bits));
                bytes[offset] = 1U;
                put_u32(bytes, offset + 4U, bits);
                offset += 8U;
#endif
            } else if (value.type == PT_STRING) {
                uint16_t sid;
                if (!string_index(&strings, (pstring *)value.as.gc, &sid)) goto done;
                bytes[offset] = 2U;
                put_u32(bytes, offset + 4U, sid);
                offset += 8U;
#if PPHP_ENABLE_FLOAT
            } else if (value.type == PT_FLOAT) {
                uint64_t bits;
                double number = (double)value.as.f;
                memcpy(&bits, &number, sizeof(bits));
                bytes[offset] = 3U;
                put_u32(bytes, offset + 8U, (uint32_t)(bits & UINT32_MAX));
                put_u32(bytes, offset + 12U, (uint32_t)(bits >> 32U));
                offset += 16U;
#endif
            } else {
                goto done;
            }
        }
        for (j = 0U; j < proto->n_locals; j++) {
            uint16_t local_sid;
            if (!string_index(&strings, proto->locals[j], &local_sid)) goto done;
            put_u16(bytes, offset, local_sid);
            offset += 2U;
        }
        for (j = 0U; j < proto->catch_count; j++) {
            const pcatch *entry = &proto->catches[j];
            uint16_t class_sid = UINT16_MAX;
            if (entry->class_constant != UINT16_MAX) {
                if (entry->class_constant >= proto->constant_count ||
                    proto->constants[entry->class_constant].type != PT_STRING ||
                    !string_index(&strings,
                                  (pstring *)proto->constants[entry->class_constant].as.gc,
                                  &class_sid)) {
                    goto done;
                }
            }
            put_u16(bytes, offset, entry->try_start);
            put_u16(bytes, offset + 2U, entry->try_end);
            put_u16(bytes, offset + 4U, entry->handler_pc);
            put_u16(bytes, offset + 6U, class_sid);
            bytes[offset + 8U] = entry->variable_slot;
            bytes[offset + 9U] = 0U;
            offset += 10U;
        }
#if PPHP_TYPECHECK
        for (j = 0U; j < (size_t)proto->n_params + 1U; j++) {
            const ptype_spec *spec = j < proto->n_params &&
                                             proto->parameter_types != NULL
                                         ? &proto->parameter_types[j]
                                         : j == proto->n_params
                                               ? &proto->return_type : NULL;
            size_t k;
            bytes[offset++] = spec == NULL ? 0U : spec->count;
            for (k = 0U; spec != NULL && k < spec->count; k++) {
                uint16_t sid = UINT16_MAX;
                bytes[offset] = spec->members[k].kind;
                if (spec->members[k].kind == PTYPE_NAMED &&
                    !string_index(&strings, spec->members[k].name, &sid)) {
                    goto done;
                }
                put_u16(bytes, offset + 1U, sid);
                offset += 3U;
            }
        }
#endif
    }
    file = pphp_fs_open(path, "wb");
    if (file == NULL) {
        result = PPHP_E_IO;
        goto done;
    }
    result = pphp_fs_write(file, bytes, total) == (int64_t)total
                 ? PPHP_OK : PPHP_E_IO;
done:
    if (file != NULL && !pphp_fs_close(file) && result == PPHP_OK) {
        result = PPHP_E_IO;
    }
    pphp_free(bytes);
    pphp_free(string_offsets);
    pphp_free(proto_offsets);
    pphp_free(strings.items);
    return result;
}

int pphp_pbc_read_file(const char *path, pmodule *module) {
    pphp_file *file = pphp_fs_open(path, "rb");
    int64_t file_size;
    uint8_t *bytes;
    int64_t read_count;
    int result;
    if (file == NULL) return PPHP_E_IO;
    file_size = pphp_fs_seek(file, 0, PPHP_FS_SEEK_END);
    if (file_size < 0 || (uint64_t)file_size > (uint64_t)SIZE_MAX ||
        pphp_fs_seek(file, 0, PPHP_FS_SEEK_SET) < 0) {
        (void)pphp_fs_close(file);
        return PPHP_E_IO;
    }
    bytes = pphp_alloc((size_t)file_size);
    if (bytes == NULL) {
        (void)pphp_fs_close(file);
        return PPHP_E_NOMEM;
    }
    read_count = pphp_fs_read(file, bytes, (size_t)file_size);
    (void)pphp_fs_close(file);
    if (read_count != file_size) {
        pphp_free(bytes);
        return PPHP_E_IO;
    }
    result = pphp_pbc_load(bytes, (size_t)read_count, module);
    if (result == PPHP_OK) {
        module->backing = bytes;
        module->owns_backing = 1U;
    } else {
        pphp_free(bytes);
    }
    return result;
}

int pphp_pbc_load(const void *data, size_t length, pmodule *module) {
    const uint8_t *bytes = data;
    const uint16_t float_flag = 16U;
    const uint16_t expected_flags =
        (uint16_t)((PPHP_INT64 ? 1U : 0U) |
                   (PPHP_USE_DOUBLE ? 2U : 0U) |
                   (PPHP_LINE_INFO ? 4U : 0U) |
                   (PPHP_TYPECHECK ? 8U : 0U) |
                   (PPHP_ENABLE_FLOAT ? 16U : 0U));
    uint16_t n_protos;
    uint16_t n_strings;
    size_t table_entries;
    size_t table_size;
    size_t proto_table;
    size_t records_start;
    pro_string *strings = NULL;
    uint16_t image_flags;
    size_t i;
    int result = PPHP_E_PARSE;
    if (bytes == NULL || module == NULL ||
        !pbc_range_available(0U, 16U, length) ||
        memcmp(bytes, "PPBC", 4U) != 0 ||
        get_u16(bytes, 4U) != (uint16_t)PPHP_PBC_FORMAT_VERSION ||
        get_u32(bytes, 8U) != length) {
        return PPHP_E_PARSE;
    }
    image_flags = get_u16(bytes, 6U);
    if ((image_flags & (uint16_t)~float_flag) !=
        (expected_flags & (uint16_t)~float_flag)) {
        return PPHP_E_PARSE;
    }
#if !PPHP_ENABLE_FLOAT
    if ((image_flags & float_flag) != 0U) return PPHP_E_UNSUPPORTED;
#endif
    if (image_flags != expected_flags) return PPHP_E_PARSE;
    n_protos = get_u16(bytes, 12U);
    n_strings = get_u16(bytes, 14U);
    table_entries = (size_t)n_strings + (size_t)n_protos;
    if (n_protos == 0U || table_entries > (SIZE_MAX - 16U) / 4U) {
        return PPHP_E_PARSE;
    }
    table_size = table_entries * 4U;
    if (!pbc_range_available(16U, table_size, length)) return PPHP_E_PARSE;
    proto_table = 16U + (size_t)n_strings * 4U;
    records_start = align4(16U + table_size);
    if (records_start > length) return PPHP_E_PARSE;
    for (i = 0U; i < table_entries; i++) {
        size_t record_offset = get_u32(bytes, 16U + i * 4U);
        size_t previous = i == 0U
                              ? records_start
                              : get_u32(bytes, 16U + (i - 1U) * 4U);
        if ((record_offset & 3U) != 0U || record_offset < records_start ||
            record_offset < previous || (i != 0U && record_offset == previous) ||
            record_offset > length) return PPHP_E_PARSE;
    }
    if (!pmodule_init(module)) {
        return PPHP_E_NOMEM;
    }
    module->image = bytes;
    module->image_length = length;
    module->ro_string_count = n_strings;
    if (n_strings != 0U) {
        module->ro_strings = pphp_alloc((size_t)n_strings *
                                         sizeof(*module->ro_strings));
        if (module->ro_strings == NULL) {
            result = PPHP_E_NOMEM;
            goto failed_module;
        }
        memset(module->ro_strings, 0,
               (size_t)n_strings * sizeof(*module->ro_strings));
    }
    strings = module->ro_strings;
    for (i = 0U; i < n_strings; i++) {
        size_t offset = get_u32(bytes, 16U + i * 4U);
        size_t limit = get_u32(bytes, 16U + (i + 1U) * 4U);
        uint16_t string_length;
        size_t record_size;
        size_t record_end;
        if ((offset & 3U) != 0U ||
            !pbc_range_available(offset, 2U, length)) goto failed_module;
        string_length = get_u16(bytes, offset);
        record_size = 2U + (size_t)string_length + 1U;
        record_end = offset;
        if (!pbc_range_available(offset, record_size, limit) ||
            !size_add(&record_end, record_size) ||
            !size_align4(&record_end) || record_end != limit ||
            bytes[offset + 2U + string_length] != 0U) {
            goto failed_module;
        }
        strings[i].base.header.refcnt = UINT16_MAX;
        strings[i].base.header.type = PT_ROSTRING;
        strings[i].base.header.flags = 0U;
        strings[i].base.length = string_length;
        strings[i].base.reserved = 0U;
        strings[i].data = (const char *)bytes + offset + 2U;
        strings[i].owner = NULL;
        strings[i].base.hash = ps_hash_bytes(strings[i].data, string_length);
    }
    for (i = 0U; i < n_protos; i++) {
        size_t offset = get_u32(bytes, proto_table + i * 4U);
        size_t limit = i + 1U < n_protos
                           ? get_u32(bytes, proto_table + (i + 1U) * 4U)
                           : length;
        uint16_t code_length;
        uint16_t constant_count;
        uint16_t catch_count;
        uint16_t name_sid;
        uint8_t local_count;
        pproto *proto;
        size_t j;
        if (!pbc_range_available(offset, 16U, limit)) goto failed_module;
        code_length = get_u16(bytes, offset + 8U);
        constant_count = get_u16(bytes, offset + 10U);
        catch_count = get_u16(bytes, offset + 12U);
        name_sid = get_u16(bytes, offset + 14U);
        if (name_sid >= n_strings ||
            !pbc_range_available(offset, 16U + align4(code_length), limit)) {
            goto failed_module;
        }
        proto = pproto_new(strings[name_sid].data,
                           strings[name_sid].base.length);
        if (proto == NULL || !pmodule_add(module, proto)) {
            pproto_destroy(proto);
            result = PPHP_E_NOMEM;
            goto failed_module;
        }
        proto->n_params = bytes[offset];
        proto->n_required = bytes[offset + 1U];
        proto->variadic = (uint8_t)(bytes[offset + 2U] & 1U);
        proto->is_method = (uint8_t)((bytes[offset + 2U] & 2U) != 0U);
        proto->conditional = (uint8_t)((bytes[offset + 2U] & 8U) != 0U);
        local_count = bytes[offset + 3U];
        if ((bytes[offset + 2U] & (uint8_t)~11U) != 0U ||
            bytes[offset + 4U] != 0U || bytes[offset + 5U] != 0U ||
            (proto->is_method && proto->conditional) ||
            (i == 0U && (proto->n_params != 0U || proto->n_required != 0U ||
                         proto->variadic || proto->is_method ||
                         proto->conditional)) ||
            proto->n_params > 31U || proto->n_required > proto->n_params ||
            (proto->variadic && proto->n_params == 0U) ||
            (size_t)proto->n_params + (proto->is_method ? 1U : 0U) >
                local_count) goto failed_module;
        proto->max_stack = get_u16(bytes, offset + 6U);
        proto->code = (uint8_t *)(uintptr_t)(bytes + offset + 16U);
        proto->owns_code = 0U;
        proto->code_length = code_length;
        proto->code_capacity = 0U;
        if (!pbc_advance(&offset, 16U + align4(code_length), limit)) {
            goto failed_module;
        }
        for (j = 0U; j < constant_count; j++) {
            pvalue value;
            uint16_t ignored;
            uint8_t tag;
            size_t constant_size;
            if (!pbc_range_available(offset, 8U, limit)) goto failed_module;
            tag = bytes[offset];
            constant_size = tag == 3U || tag == 4U ? 16U : 8U;
            if (!pbc_range_available(offset, constant_size, limit)) {
                goto failed_module;
            }
            if (tag == 0U) {
                value = pv_int((pphp_int)(int32_t)get_u32(bytes, offset + 4U));
            } else if (tag == 1U) {
#if PPHP_ENABLE_FLOAT
                uint32_t bits = get_u32(bytes, offset + 4U);
                float number;
                memcpy(&number, &bits, sizeof(number));
                value = pv_float((pphp_float)number);
#else
                result = PPHP_E_UNSUPPORTED;
                goto failed_module;
#endif
            } else if (tag == 2U) {
                uint32_t sid = get_u32(bytes, offset + 4U);
                if (sid >= n_strings) goto failed_module;
                value = pv_heap(PT_STRING, &strings[sid].base.header);
            } else if (tag == 3U) {
#if PPHP_ENABLE_FLOAT
                uint64_t bits;
                double number;
                bits = get_u32(bytes, offset + 8U);
                bits |= (uint64_t)get_u32(bytes, offset + 12U) << 32U;
                memcpy(&number, &bits, sizeof(number));
                value = pv_float((pphp_float)number);
#else
                result = PPHP_E_UNSUPPORTED;
                goto failed_module;
#endif
            } else if (tag == 4U) {
#if PPHP_INT64
                uint64_t bits;
                int64_t integer;
                bits = get_u32(bytes, offset + 8U);
                bits |= (uint64_t)get_u32(bytes, offset + 12U) << 32U;
                memcpy(&integer, &bits, sizeof(integer));
                value = pv_int((pphp_int)integer);
#else
                goto failed_module;
#endif
            } else {
                goto failed_module;
            }
            if (!pproto_add_constant(proto, value, &ignored)) {
                result = PPHP_E_NOMEM;
                goto failed_module;
            }
            if (!pbc_advance(&offset, constant_size, limit)) {
                goto failed_module;
            }
        }
        for (j = 0U; j < local_count; j++) {
            uint16_t local_sid;
            uint8_t slot;
            if (!pbc_range_available(offset, 2U, limit)) goto failed_module;
            local_sid = get_u16(bytes, offset);
            if (local_sid >= n_strings ||
                !pproto_add_local(proto, strings[local_sid].data,
                                  strings[local_sid].base.length, &slot) ||
                slot != j) {
                goto failed_module;
            }
            if (!pbc_advance(&offset, 2U, limit)) goto failed_module;
        }
        for (j = 0U; j < catch_count; j++) {
            pcatch entry;
            uint16_t class_sid;
            if (!pbc_range_available(offset, 10U, limit)) goto failed_module;
            entry.try_start = get_u16(bytes, offset);
            entry.try_end = get_u16(bytes, offset + 2U);
            entry.handler_pc = get_u16(bytes, offset + 4U);
            class_sid = get_u16(bytes, offset + 6U);
            entry.class_constant = UINT16_MAX;
            entry.variable_slot = bytes[offset + 8U];
            entry.reserved = 0U;
            if (entry.try_start > entry.try_end || entry.try_end > code_length ||
                entry.handler_pc >= code_length ||
                bytes[offset + 9U] != 0U ||
                (entry.variable_slot != UINT8_MAX &&
                 entry.variable_slot >= proto->n_locals)) goto failed_module;
            if (class_sid != UINT16_MAX) {
                pvalue class_name;
                uint16_t class_constant;
                if (class_sid >= n_strings) goto failed_module;
                class_name = pv_heap(PT_STRING,
                                     &strings[class_sid].base.header);
                if (!pproto_add_constant(proto, class_name, &class_constant)) {
                    result = PPHP_E_NOMEM;
                    goto failed_module;
                }
                entry.class_constant = class_constant;
            }
            if (!pproto_add_catch(proto, entry)) goto failed_module;
            if (!pbc_advance(&offset, 10U, limit)) goto failed_module;
        }
#if PPHP_TYPECHECK
        if (proto->n_params != 0U) {
            proto->parameter_types = pphp_alloc(
                (size_t)proto->n_params * sizeof(*proto->parameter_types));
            if (proto->parameter_types == NULL) {
                result = PPHP_E_NOMEM;
                goto failed_module;
            }
            memset(proto->parameter_types, 0,
                   (size_t)proto->n_params * sizeof(*proto->parameter_types));
        }
        for (j = 0U; j < (size_t)proto->n_params + 1U; j++) {
            ptype_spec *spec = j < proto->n_params
                                   ? &proto->parameter_types[j]
                                   : &proto->return_type;
            uint8_t member_count;
            size_t k;
            if (!pbc_range_available(offset, 1U, limit)) goto failed_module;
            member_count = bytes[offset++];
            if (member_count > 16U ||
                !pbc_range_available(offset, (size_t)member_count * 3U,
                                     limit)) goto failed_module;
            for (k = 0U; k < member_count; k++) {
                uint8_t kind = bytes[offset];
                uint16_t sid = get_u16(bytes, offset + 1U);
                pstring *type_name = NULL;
                size_t previous;
                if (kind < PTYPE_INT || kind > PTYPE_NAMED ||
                    (kind == PTYPE_NAMED && sid >= n_strings) ||
                    (kind != PTYPE_NAMED && sid != UINT16_MAX) ||
                    (j < proto->n_params &&
                     (kind == PTYPE_VOID || kind == PTYPE_STATIC)) ||
                    (member_count > 1U &&
                     (kind == PTYPE_VOID || kind == PTYPE_MIXED)) ||
                    (!PPHP_ENABLE_FLOAT && kind == PTYPE_FLOAT)) {
                    goto failed_module;
                }
                if (kind == PTYPE_NAMED) {
                    type_name = &strings[sid].base;
                    if (!pbc_named_type_valid(type_name)) goto failed_module;
                }
                for (previous = 0U; previous < spec->count; previous++) {
                    if (spec->members[previous].kind == kind &&
                        (kind != PTYPE_NAMED ||
                         pbc_bytes_equal_ci(
                             ps_data(spec->members[previous].name),
                             spec->members[previous].name->length,
                             ps_data(type_name), type_name->length))) {
                        goto failed_module;
                    }
                }
                if (!ptype_spec_add_string(spec, kind, type_name)) {
                    result = PPHP_E_NOMEM;
                    goto failed_module;
                }
                offset += 3U;
            }
        }
#endif
        if (!size_align4(&offset) || offset != limit) goto failed_module;
    }
    for (i = 0U; i < module->count; i++) {
        if (!pbc_validate_code(module, i)) goto failed_module;
    }
    if (!pbc_assign_proto_roles(module)) goto failed_module;
#if PPHP_TYPECHECK
    {
        int type_context_status = pbc_validate_type_context(module);
        if (type_context_status != PPHP_OK) {
            result = type_context_status;
            goto failed_module;
        }
    }
#endif
    for (i = 0U; i < n_strings; i++) strings[i].owner = module;
    result = PPHP_OK;
    goto done;
failed_module:
    pmodule_destroy(module);
done:
    return result;
}

const char *pphp_opcode_name(uint8_t opcode) {
    switch ((pphp_opcode)opcode) {
        case OP_NOP: return "NOP";
        case OP_HALT: return "HALT";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        case OP_SWAP: return "SWAP";
        case OP_LOAD_NULL: return "LOAD_NULL";
        case OP_LOAD_TRUE: return "LOAD_TRUE";
        case OP_LOAD_FALSE: return "LOAD_FALSE";
        case OP_LOAD_I8: return "LOAD_I8";
        case OP_LOAD_I32: return "LOAD_I32";
        case OP_LOAD_CONST: return "LOAD_CONST";
        case OP_LOAD_LOCAL: return "LOAD_LOCAL";
        case OP_STORE_LOCAL: return "STORE_LOCAL";
        case OP_LOAD_ARGC: return "LOAD_ARGC";
        case OP_BIND_GLOBAL: return "BIND_GLOBAL";
        case OP_STATIC_INIT: return "STATIC_INIT";
        case OP_LOAD_NAMED_CONST: return "LOAD_NAMED_CONST";
        case OP_LOAD_LOCAL_QUIET: return "LOAD_LOCAL_QUIET";
        case OP_UNSET_LOCAL: return "UNSET_LOCAL";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_POW: return "POW";
        case OP_NEG: return "NEG";
        case OP_CONCAT: return "CONCAT";
        case OP_BAND: return "BAND";
        case OP_BOR: return "BOR";
        case OP_BXOR: return "BXOR";
        case OP_BNOT: return "BNOT";
        case OP_SHL: return "SHL";
        case OP_SHR: return "SHR";
        case OP_EQ: return "EQ";
        case OP_NE: return "NE";
        case OP_IDENT: return "IDENT";
        case OP_NIDENT: return "NIDENT";
        case OP_LT: return "LT";
        case OP_LE: return "LE";
        case OP_GT: return "GT";
        case OP_GE: return "GE";
        case OP_CMP: return "CMP";
        case OP_NOT: return "NOT";
        case OP_CAST: return "CAST";
        case OP_INSTANCEOF_DYNAMIC: return "INSTANCEOF_DYNAMIC";
        case OP_JMP: return "JMP";
        case OP_JMP_IF: return "JMP_IF";
        case OP_JMP_UNLESS: return "JMP_UNLESS";
        case OP_JMP_IF_KEEP: return "JMP_IF_KEEP";
        case OP_JMP_UNLESS_KEEP: return "JMP_UNLESS_KEEP";
        case OP_JMP_NOTNULL_KEEP: return "JMP_NOTNULL_KEEP";
        case OP_JMP_IFNULL_KEEP: return "JMP_IFNULL_KEEP";
        case OP_CALL: return "CALL";
        case OP_CALL_VALUE: return "CALL_VALUE";
        case OP_RET: return "RET";
        case OP_RET_NULL: return "RET_NULL";
        case OP_ECHO: return "ECHO";
        case OP_CALL_ARRAY: return "CALL_ARRAY";
        case OP_CALL_VALUE_ARRAY: return "CALL_VALUE_ARRAY";
        case OP_INCLUDE: return "INCLUDE";
        case OP_TYPECHECK_ARGS: return "TYPECHECK_ARGS";
        case OP_NEW_ARRAY: return "NEW_ARRAY";
        case OP_ARR_PUSH: return "ARR_PUSH";
        case OP_ARR_SET: return "ARR_SET";
        case OP_IDX_GET: return "IDX_GET";
        case OP_IDX_SET: return "IDX_SET";
        case OP_IDX_APPEND: return "IDX_APPEND";
        case OP_FE_INIT: return "FE_INIT";
        case OP_FE_NEXT: return "FE_NEXT";
        case OP_FE_FREE: return "FE_FREE";
        case OP_ARR_EXTEND: return "ARR_EXTEND";
        case OP_ARR_SEPARATE: return "ARR_SEPARATE";
        case OP_IDX_UNSET: return "IDX_UNSET";
        case OP_IDX_GET_QUIET: return "IDX_GET_QUIET";
        case OP_NEW_OBJ: return "NEW_OBJ";
        case OP_PROP_GET: return "PROP_GET";
        case OP_PROP_SET: return "PROP_SET";
        case OP_MCALL: return "MCALL";
        case OP_MCALL_ARRAY: return "MCALL_ARRAY";
        case OP_NEW_OBJ_ARRAY: return "NEW_OBJ_ARRAY";
        case OP_SCALL: return "SCALL";
        case OP_SCALL_ARRAY: return "SCALL_ARRAY";
        case OP_INSTANCEOF: return "INSTANCEOF";
        case OP_SPROP_GET: return "SPROP_GET";
        case OP_SPROP_SET: return "SPROP_SET";
        case OP_CLSCONST: return "CLSCONST";
        case OP_PROP_GET_QUIET: return "PROP_GET_QUIET";
        case OP_NEW_OBJ_DYNAMIC: return "NEW_OBJ_DYNAMIC";
        case OP_NEW_OBJ_DYNAMIC_ARRAY: return "NEW_OBJ_DYNAMIC_ARRAY";
        case OP_CLOSURE: return "CLOSURE";
        case OP_THROW: return "THROW";
        case OP_CLONE: return "CLONE";
        case OP_DEF_FUNC: return "DEF_FUNC";
        case OP_DEF_CCONST: return "DEF_CCONST";
        case OP_DEF_CLASS: return "DEF_CLASS";
        case OP_DEF_METHOD: return "DEF_METHOD";
        case OP_DEF_PROP: return "DEF_PROP";
        case OP_DEF_CONST: return "DEF_CONST";
        case OP_DEF_END: return "DEF_END";
        case OP_DEF_INTERFACE: return "DEF_INTERFACE";
        case OP_LINE: return "LINE";
        case OP_PROP_GET_DYNAMIC: return "PROP_GET_DYNAMIC";
        case OP_PROP_SET_DYNAMIC: return "PROP_SET_DYNAMIC";
        case OP_PROP_GET_DYNAMIC_QUIET: return "PROP_GET_DYNAMIC_QUIET";
        case OP_MCALL_DYNAMIC: return "MCALL_DYNAMIC";
        case OP_MCALL_DYNAMIC_ARRAY: return "MCALL_DYNAMIC_ARRAY";
        default: return "UNKNOWN";
    }
}
