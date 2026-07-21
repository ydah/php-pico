#include "pbc.h"

#include "opcode.h"
#include "pphp/pphp.h"

#include <limits.h>
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
    proto->name = ps_new(name == NULL ? "" : name, name == NULL ? 0U : length);
    if (proto->name == NULL) {
        pphp_free(proto);
        return NULL;
    }
    return proto;
}

void pproto_destroy(pproto *proto) {
    size_t i;
    if (proto == NULL) {
        return;
    }
    for (i = 0U; i < proto->constant_count; i++) {
        pv_release(proto->constants[i]);
    }
    for (i = 0U; i < proto->n_locals; i++) {
        ps_destroy(proto->locals[i]);
    }
    ps_destroy(proto->name);
    pphp_free(proto->locals);
    pphp_free(proto->constants);
    pphp_free(proto->code);
    pphp_free(proto);
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

int pmodule_init(pmodule *module) {
    memset(module, 0, sizeof(*module));
    return 1;
}

void pmodule_destroy(pmodule *module) {
    size_t i;
    for (i = 0U; i < module->count; i++) {
        pproto_destroy(module->protos[i]);
    }
    pphp_free(module->protos);
    memset(module, 0, sizeof(*module));
}

int pmodule_add(pmodule *module, pproto *proto) {
    if (module->count == module->capacity &&
        !grow_array((void **)&module->protos, sizeof(*module->protos),
                    &module->capacity, module->count + 1U)) {
        return 0;
    }
    module->protos[module->count++] = proto;
    return 1;
}

const pproto *pmodule_find(const pmodule *module, const pstring *name) {
    size_t i;
    for (i = 1U; i < module->count; i++) {
        if (ps_equal(module->protos[i]->name, name)) {
            return module->protos[i];
        }
    }
    return NULL;
}

int pphp_pbc_write_file(const pmodule *module, const char *path) {
    (void)module;
    (void)path;
    return PPHP_E_IO;
}

int pphp_pbc_read_file(const char *path, pmodule *module) {
    (void)path;
    (void)module;
    return PPHP_E_IO;
}

const char *pphp_opcode_name(uint8_t opcode) {
    switch ((pphp_opcode)opcode) {
        case OP_NOP: return "NOP";
        case OP_HALT: return "HALT";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        case OP_LOAD_NULL: return "LOAD_NULL";
        case OP_LOAD_TRUE: return "LOAD_TRUE";
        case OP_LOAD_FALSE: return "LOAD_FALSE";
        case OP_LOAD_I8: return "LOAD_I8";
        case OP_LOAD_I32: return "LOAD_I32";
        case OP_LOAD_CONST: return "LOAD_CONST";
        case OP_LOAD_LOCAL: return "LOAD_LOCAL";
        case OP_STORE_LOCAL: return "STORE_LOCAL";
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
        case OP_JMP: return "JMP";
        case OP_JMP_IF: return "JMP_IF";
        case OP_JMP_UNLESS: return "JMP_UNLESS";
        case OP_JMP_IF_KEEP: return "JMP_IF_KEEP";
        case OP_JMP_UNLESS_KEEP: return "JMP_UNLESS_KEEP";
        case OP_JMP_NOTNULL_KEEP: return "JMP_NOTNULL_KEEP";
        case OP_CALL: return "CALL";
        case OP_RET: return "RET";
        case OP_RET_NULL: return "RET_NULL";
        case OP_ECHO: return "ECHO";
        case OP_LINE: return "LINE";
        default: return "UNKNOWN";
    }
}
