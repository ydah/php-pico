#include "disasm.h"

#include "opcode.h"

#include <stdint.h>

static uint16_t code_u16(const uint8_t *code, size_t offset) {
    return (uint16_t)((uint16_t)code[offset] |
                      (uint16_t)((uint16_t)code[offset + 1U] << 8U));
}

static int32_t code_i32(const uint8_t *code, size_t offset) {
    uint32_t value = code[offset];
    value |= (uint32_t)code[offset + 1U] << 8U;
    value |= (uint32_t)code[offset + 2U] << 16U;
    value |= (uint32_t)code[offset + 3U] << 24U;
    return (int32_t)value;
}

static size_t operand_size(uint8_t opcode) {
    switch ((pphp_opcode)opcode) {
        case OP_LOAD_I8:
        case OP_LOAD_LOCAL:
        case OP_STORE_LOCAL:
        case OP_ECHO:
            return 1U;
        case OP_LOAD_CONST:
        case OP_JMP:
        case OP_JMP_IF:
        case OP_JMP_UNLESS:
        case OP_JMP_IF_KEEP:
        case OP_JMP_UNLESS_KEEP:
        case OP_JMP_NOTNULL_KEEP:
        case OP_LINE:
            return 2U;
        case OP_CALL:
            return 3U;
        case OP_LOAD_I32:
            return 4U;
        default:
            return 0U;
    }
}

static void print_constant(FILE *stream, const pproto *proto, uint16_t index) {
    if (index >= proto->constant_count) {
        fputs(" <invalid>", stream);
        return;
    }
    switch ((pvalue_type)proto->constants[index].type) {
        case PT_INT:
            fprintf(stream, " ; %lld", (long long)proto->constants[index].as.i);
            break;
        case PT_FLOAT:
            fprintf(stream, " ; %.14g", (double)proto->constants[index].as.f);
            break;
        case PT_STRING: {
            const pstring *string = (const pstring *)proto->constants[index].as.gc;
            fprintf(stream, " ; \"%.*s\"", (int)string->length, string->data);
            break;
        }
        default:
            break;
    }
}

static int disassemble_proto(FILE *stream, const pproto *proto, size_t index) {
    size_t pc = 0U;
    fprintf(stream, "proto #%zu %.*s params=%u required=%u locals=%u max_stack=%u\n",
            index, (int)proto->name->length, proto->name->data, proto->n_params,
            proto->n_required, proto->n_locals, proto->max_stack);
    while (pc < proto->code_length) {
        size_t instruction = pc;
        uint8_t opcode = proto->code[pc++];
        size_t operands = operand_size(opcode);
        fprintf(stream, "%04zu  %-20s", instruction, pphp_opcode_name(opcode));
        if (pc + operands > proto->code_length) {
            fputs(" <truncated>\n", stream);
            return 0;
        }
        switch ((pphp_opcode)opcode) {
            case OP_LOAD_I8:
                fprintf(stream, " %d", (int)(int8_t)proto->code[pc]);
                break;
            case OP_LOAD_LOCAL:
            case OP_STORE_LOCAL:
            case OP_ECHO:
                fprintf(stream, " %u", proto->code[pc]);
                break;
            case OP_LOAD_I32:
                fprintf(stream, " %d", code_i32(proto->code, pc));
                break;
            case OP_LOAD_CONST: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " %u", constant);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_JMP:
            case OP_JMP_IF:
            case OP_JMP_UNLESS:
            case OP_JMP_IF_KEEP:
            case OP_JMP_UNLESS_KEEP:
            case OP_JMP_NOTNULL_KEEP: {
                int16_t relative = (int16_t)code_u16(proto->code, pc);
                fprintf(stream, " %+d -> %td", relative,
                        (ptrdiff_t)(pc + 2U) + relative);
                break;
            }
            case OP_CALL: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u argc=%u", constant, proto->code[pc + 2U]);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_LINE:
                fprintf(stream, " %u", code_u16(proto->code, pc));
                break;
            default:
                break;
        }
        fputc('\n', stream);
        pc += operands;
    }
    return 1;
}

int pphp_disassemble_module(FILE *stream, const pmodule *module) {
    size_t i;
    if (stream == NULL || module == NULL) {
        return 0;
    }
    for (i = 0U; i < module->count; i++) {
        if (!disassemble_proto(stream, module->protos[i], i)) {
            return 0;
        }
        if (i + 1U < module->count) {
            fputc('\n', stream);
        }
    }
    return 1;
}
