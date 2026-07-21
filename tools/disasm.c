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
        case OP_BIND_GLOBAL:
        case OP_ECHO:
        case OP_CALL_VALUE:
            return 1U;
        case OP_LOAD_CONST:
        case OP_LOAD_NAMED_CONST:
        case OP_DEF_CONST:
        case OP_CALL_ARRAY:
        case OP_MCALL_ARRAY:
        case OP_NEW_OBJ_ARRAY:
        case OP_JMP:
        case OP_JMP_IF:
        case OP_JMP_UNLESS:
        case OP_JMP_IF_KEEP:
        case OP_JMP_UNLESS_KEEP:
        case OP_JMP_NOTNULL_KEEP:
        case OP_LINE:
        case OP_NEW_ARRAY:
            return 2U;
        case OP_CALL:
        case OP_NEW_OBJ:
        case OP_MCALL:
        case OP_DEF_PROP:
        case OP_FE_NEXT:
        case OP_STATIC_INIT:
            return 3U;
        case OP_PROP_GET:
        case OP_PROP_SET:
        case OP_INSTANCEOF:
            return 2U;
        case OP_DEF_CLASS:
        case OP_DEF_METHOD:
            return 5U;
        case OP_LOAD_I32:
            return 4U;
        case OP_CLOSURE:
            return 3U;
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
            case OP_BIND_GLOBAL:
            case OP_ECHO:
            case OP_CALL_VALUE:
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
            case OP_LOAD_NAMED_CONST:
            case OP_DEF_CONST: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u", constant);
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
            case OP_NEW_OBJ:
            case OP_MCALL: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u argc=%u", constant, proto->code[pc + 2U]);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_PROP_GET:
            case OP_PROP_SET:
            case OP_INSTANCEOF: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u", constant);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_CALL_ARRAY:
            case OP_MCALL_ARRAY:
            case OP_NEW_OBJ_ARRAY: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u", constant);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_DEF_CLASS: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u parent=%u flags=0x%02x", constant,
                        code_u16(proto->code, pc + 2U), proto->code[pc + 4U]);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_DEF_METHOD: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u proto=%u flags=0x%02x", constant,
                        code_u16(proto->code, pc + 2U), proto->code[pc + 4U]);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_DEF_PROP: {
                uint16_t constant = code_u16(proto->code, pc);
                fprintf(stream, " name=%u flags=0x%02x", constant,
                        proto->code[pc + 2U]);
                print_constant(stream, proto, constant);
                break;
            }
            case OP_LINE:
            case OP_NEW_ARRAY:
                fprintf(stream, " %u", code_u16(proto->code, pc));
                break;
            case OP_FE_NEXT: {
                int16_t relative = (int16_t)code_u16(proto->code, pc);
                fprintf(stream, " %+d -> %td haskey=%u", relative,
                        (ptrdiff_t)(pc + 3U) + relative, proto->code[pc + 2U]);
                break;
            }
            case OP_STATIC_INIT: {
                int16_t relative = (int16_t)code_u16(proto->code, pc + 1U);
                fprintf(stream, " slot=%u %+d -> %td", proto->code[pc],
                        relative, (ptrdiff_t)(pc + 3U) + relative);
                break;
            }
            case OP_CLOSURE: {
                uint8_t count = proto->code[pc + 2U];
                size_t extra = (size_t)count * 2U;
                size_t capture;
                if (pc + 3U + extra > proto->code_length) {
                    fputs(" <truncated>\n", stream);
                    return 0;
                }
                fprintf(stream, " proto=%u captures=%u", code_u16(proto->code, pc),
                        count);
                for (capture = 0U; capture < count; capture++) {
                    fprintf(stream, " %u:%u", proto->code[pc + 3U + capture * 2U],
                            proto->code[pc + 4U + capture * 2U]);
                }
                operands += extra;
                break;
            }
            default:
                break;
        }
        fputc('\n', stream);
        pc += operands;
    }
    if (proto->catch_count != 0U) {
        size_t i;
        fputs("catch table:\n", stream);
        for (i = 0U; i < proto->catch_count; i++) {
            const pcatch *entry = &proto->catches[i];
            fprintf(stream, "  [%u, %u) -> %u ", entry->try_start,
                    entry->try_end, entry->handler_pc);
            if (entry->class_constant == UINT16_MAX) {
                fputs("finally", stream);
            } else if (entry->class_constant < proto->constant_count &&
                       proto->constants[entry->class_constant].type == PT_STRING) {
                const pstring *name = (const pstring *)
                    proto->constants[entry->class_constant].as.gc;
                fprintf(stream, "catch %.*s", (int)name->length, name->data);
            } else {
                fputs("catch <invalid>", stream);
            }
            if (entry->variable_slot != UINT8_MAX) {
                fprintf(stream, " slot=%u", entry->variable_slot);
            }
            fputc('\n', stream);
        }
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
