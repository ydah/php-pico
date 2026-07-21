#include "vm.h"

#include "builtins.h"
#include "opcode.h"
#include "parray.h"
#include "resource.h"
#include "pclass.h"
#include "value_ops.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int push(pphp_state *state, pvalue value) {
    if (state->stack_count >= PPHP_STACK_SLOTS) {
        pphp_runtime_error(state, 0U, "value stack overflow");
        pv_release(value);
        return 0;
    }
    state->stack[state->stack_count++] = value;
    return 1;
}

static pvalue pop(pphp_state *state) {
    if (state->stack_count == 0U) {
        pphp_runtime_error(state, 0U, "bytecode stack underflow");
        return pv_null();
    }
    return state->stack[--state->stack_count];
}

static uint8_t read_u8(pphp_state *state, pframe *frame) {
    if (frame->pc >= frame->proto->code_length) {
        pphp_runtime_error(state, frame->line, "truncated bytecode");
        return 0U;
    }
    return frame->proto->code[frame->pc++];
}

static uint16_t read_u16(pphp_state *state, pframe *frame) {
    uint16_t low = read_u8(state, frame);
    uint16_t high = read_u8(state, frame);
    return (uint16_t)(low | (uint16_t)(high << 8U));
}

static int16_t read_i16(pphp_state *state, pframe *frame) {
    return (int16_t)read_u16(state, frame);
}

static int32_t read_i32(pphp_state *state, pframe *frame) {
    uint32_t value = read_u8(state, frame);
    value |= (uint32_t)read_u8(state, frame) << 8U;
    value |= (uint32_t)read_u8(state, frame) << 16U;
    value |= (uint32_t)read_u8(state, frame) << 24U;
    return (int32_t)value;
}

static int jump_relative(pphp_state *state, pframe *frame, int16_t relative) {
    ptrdiff_t target = (ptrdiff_t)frame->pc + relative;
    if (target < 0 || (size_t)target > frame->proto->code_length) {
        pphp_runtime_error(state, frame->line, "bytecode jump is out of range");
        return 0;
    }
    frame->pc = (size_t)target;
    return 1;
}

static int output_echo(pphp_state *state, pvalue value) {
    pstring *string = pv_to_string(value);
    if (string == NULL) {
        pphp_runtime_error(state, 0U, "value cannot be converted to string");
        return 0;
    }
    pphp_output(state, string->data, string->length);
    ps_destroy(string);
    return 1;
}

static pv_operation operation_for(uint8_t opcode) {
    switch ((pphp_opcode)opcode) {
        case OP_ADD: return PV_ADD;
        case OP_SUB: return PV_SUB;
        case OP_MUL: return PV_MUL;
        case OP_DIV: return PV_DIV;
        case OP_MOD: return PV_MOD;
        case OP_POW: return PV_POW;
        case OP_CONCAT: return PV_CONCAT;
        case OP_BAND: return PV_BAND;
        case OP_BOR: return PV_BOR;
        case OP_BXOR: return PV_BXOR;
        case OP_SHL: return PV_SHL;
        case OP_SHR: return PV_SHR;
        default: return PV_ADD;
    }
}

static int execute_binary(pphp_state *state, uint8_t opcode) {
    pvalue right = pop(state);
    pvalue left = pop(state);
    pvalue result = pv_null();
    const char *error = NULL;
    int ok = pv_binary_operation(operation_for(opcode), left, right, &result, &error);
    pv_release(left);
    pv_release(right);
    if (!ok) {
        pphp_runtime_error(state, 0U, "%s", error == NULL ? "binary operation failed" : error);
        return 0;
    }
    return push(state, result);
}

static int execute_compare(pphp_state *state, uint8_t opcode) {
    pvalue right = pop(state);
    pvalue left = pop(state);
    const char *error = NULL;
    int compared = 0;
    int strict = opcode == OP_IDENT || opcode == OP_NIDENT;
    int truth;
    int ok = pv_compare(left, right, strict, &compared, &error);
    pv_release(left);
    pv_release(right);
    if (!ok) {
        pphp_runtime_error(state, 0U, "%s", error == NULL ? "comparison failed" : error);
        return 0;
    }
    switch ((pphp_opcode)opcode) {
        case OP_EQ:
        case OP_IDENT: truth = compared == 0; break;
        case OP_NE:
        case OP_NIDENT: truth = compared != 0; break;
        case OP_LT: truth = compared < 0; break;
        case OP_LE: truth = compared <= 0; break;
        case OP_GT: truth = compared > 0; break;
        case OP_GE: truth = compared >= 0; break;
        case OP_CMP: return push(state, pv_int((pphp_int)compared));
        default: truth = 0; break;
    }
    return push(state, pv_bool(truth));
}

static parray *array_for_write(pvalue array_value) {
    parray *array = (parray *)array_value.as.gc;
    if (array->header.refcnt <= 2U) {
        return array;
    }
    return pa_clone(array);
}

static int execute_array_set(pphp_state *state, int append) {
    pvalue value = pop(state);
    pvalue key = pv_null();
    pvalue array_value;
    parray *array;
    parray *writable;
    int ok;
    if (!append) key = pop(state);
    array_value = pop(state);
    if (array_value.type != PT_ARRAY) {
        pv_release(array_value);
        pv_release(key);
        pv_release(value);
        pphp_runtime_error(state, 0U, "Cannot use a non-array value as an array");
        return 0;
    }
    array = (parray *)array_value.as.gc;
    writable = array_for_write(array_value);
    if (writable == NULL) {
        pv_release(array_value);
        pv_release(key);
        pv_release(value);
        pphp_runtime_error(state, 0U, "out of memory separating array");
        return 0;
    }
    if (writable != array) {
        pv_release(array_value);
        array_value = pv_heap(PT_ARRAY, &writable->header);
    }
    ok = append ? pa_push(writable, value) : pa_set(writable, key, value);
    pv_release(key);
    if (!ok) {
        pv_release(array_value);
        pv_release(value);
        pphp_runtime_error(state, 0U, "array update failed");
        return 0;
    }
    if (!push(state, array_value)) {
        pv_release(value);
        return 0;
    }
    return push(state, value);
}

static void release_range(pphp_state *state, size_t begin, size_t end) {
    size_t i;
    for (i = begin; i < end; i++) {
        pv_release(state->stack[i]);
    }
}

static int call_function(pphp_state *state, pframe *caller, uint16_t name_index,
                         uint8_t argument_count) {
    const pvalue *name_value;
    const pstring *name;
    size_t argument_base;
    pvalue result = pv_null();
    int builtin;
    const pproto *callee;
    size_t i;
    (void)caller;
    if (name_index >= caller->proto->constant_count ||
        caller->proto->constants[name_index].type != PT_STRING ||
        state->stack_count < argument_count) {
        pphp_runtime_error(state, caller->line, "invalid CALL instruction");
        return 0;
    }
    name_value = &caller->proto->constants[name_index];
    name = (const pstring *)name_value->as.gc;
    argument_base = state->stack_count - argument_count;
    builtin = pphp_call_builtin(state, name, state->stack + argument_base,
                                argument_count, &result);
    if (builtin != 0) {
        release_range(state, argument_base, state->stack_count);
        state->stack_count = argument_base;
        return builtin > 0 && push(state, result);
    }
    callee = pmodule_find(state->module, name);
    if (callee == NULL) {
        pphp_runtime_error(state, caller->line, "Call to undefined function %.*s()",
                           (int)name->length, name->data);
        return 0;
    }
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to function %.*s(), %u required, %u given",
                           (int)name->length, name->data, callee->n_required,
                           argument_count);
        return 0;
    }
    if (callee->variadic) {
        pphp_runtime_error(state, caller->line,
                           "variadic functions require the array runtime");
        return 0;
    }
    if (argument_count > callee->n_params) {
        release_range(state, argument_base + callee->n_params, state->stack_count);
        state->stack_count = argument_base + callee->n_params;
    }
    if (argument_base + callee->n_locals >= PPHP_STACK_SLOTS) {
        pphp_runtime_error(state, caller->line, "value stack overflow entering function");
        return 0;
    }
    for (i = argument_count < callee->n_params ? argument_count : callee->n_params;
         i < callee->n_locals; i++) {
        state->stack[argument_base + i] = pv_null();
    }
    state->stack_count = argument_base + callee->n_locals;
    if (state->frame_count >= PPHP_FRAME_MAX) {
        pphp_runtime_error(state, caller->line, "call stack overflow");
        return 0;
    }
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].pc = 0U;
    state->frames[state->frame_count].base = argument_base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].has_return_override = 0;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = NULL;
    state->frame_count++;
    return 1;
}

static const pstring *constant_string(pphp_state *state, pframe *frame,
                                      uint16_t index) {
    if (index >= frame->proto->constant_count ||
        frame->proto->constants[index].type != PT_STRING) {
        pphp_runtime_error(state, frame->line, "name constant is invalid");
        return NULL;
    }
    return (const pstring *)frame->proto->constants[index].as.gc;
}

static int enter_method(pphp_state *state, pframe *caller, const pmethod *method,
                        uint8_t argument_count, int constructor) {
    const pproto *callee = method->proto;
    size_t base;
    size_t supplied;
    size_t i;
    pvalue object_value;
    if (method == NULL || callee == NULL || (method->flags & PC_STATIC) != 0U ||
        state->stack_count < (size_t)argument_count + 1U) {
        pphp_runtime_error(state, caller->line, "invalid method call");
        return 0;
    }
    base = state->stack_count - argument_count - 1U;
    object_value = state->stack[base];
    if (object_value.type != PT_OBJECT) {
        pphp_runtime_error(state, caller->line, "method call target is not an object");
        return 0;
    }
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to method %.*s(), %u required, %u given",
                           (int)method->name->length, method->name->data,
                           callee->n_required, argument_count);
        return 0;
    }
    if (argument_count > callee->n_params) {
        release_range(state, base + 1U + callee->n_params, state->stack_count);
        state->stack_count = base + 1U + callee->n_params;
    }
    supplied = argument_count < callee->n_params ? argument_count : callee->n_params;
    if (base + callee->n_locals >= PPHP_STACK_SLOTS ||
        state->frame_count >= PPHP_FRAME_MAX) {
        pphp_runtime_error(state, caller->line, "stack overflow entering method");
        return 0;
    }
    for (i = 1U + supplied; i < callee->n_locals; i++) {
        state->stack[base + i] = pv_null();
    }
    state->stack_count = base + callee->n_locals;
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].pc = 0U;
    state->frames[state->frame_count].base = base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].has_return_override = constructor;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = method->owner;
    if (constructor) {
        state->frames[state->frame_count].return_override = object_value;
        pv_retain(object_value);
    }
    state->frame_count++;
    return 1;
}

static int return_from_function(pphp_state *state, pvalue result) {
    pframe *frame = &state->frames[state->frame_count - 1U];
    size_t base = frame->base;
    if (frame->has_return_override) {
        pv_release(result);
        result = frame->return_override;
        frame->return_override = pv_null();
        frame->has_return_override = 0;
    }
    release_range(state, base, state->stack_count);
    state->stack_count = base;
    state->frame_count--;
    if (state->frame_count == 0U) {
        pv_release(result);
        return 1;
    }
    return push(state, result);
}

int pphp_vm_execute(pphp_state *state, const pmodule *module) {
    size_t i;
    if (module == NULL || module->count == 0U) {
        pphp_runtime_error(state, 0U, "module has no entry point");
        return PPHP_E_RUNTIME;
    }
    state->module = module;
    state->stack_count = module->protos[0]->n_locals;
    state->frame_count = 1U;
    state->frames[0].proto = module->protos[0];
    state->frames[0].pc = 0U;
    state->frames[0].base = 0U;
    state->frames[0].line = 1U;
    state->frames[0].has_return_override = 0;
    state->frames[0].return_override = pv_null();
    state->frames[0].called_scope = NULL;
    for (i = 0U; i < state->stack_count; i++) {
        state->stack[i] = pv_null();
    }
    while (state->frame_count != 0U && state->error[0] == '\0') {
        pframe *frame = &state->frames[state->frame_count - 1U];
        uint8_t opcode = read_u8(state, frame);
        switch ((pphp_opcode)opcode) {
            case OP_NOP: break;
            case OP_HALT:
                if (state->frame_count != 1U) {
                    pphp_runtime_error(state, frame->line, "HALT inside function");
                    break;
                }
                release_range(state, 0U, state->stack_count);
                state->stack_count = 0U;
                state->frame_count = 0U;
                break;
            case OP_POP: {
                pvalue value = pop(state);
                pv_release(value);
                break;
            }
            case OP_DUP:
                if (state->stack_count == 0U) {
                    pphp_runtime_error(state, frame->line, "DUP on empty stack");
                } else {
                    pvalue value = state->stack[state->stack_count - 1U];
                    pv_retain(value);
                    (void)push(state, value);
                }
                break;
            case OP_SWAP:
                if (state->stack_count < 2U) {
                    pphp_runtime_error(state, frame->line, "SWAP requires two stack values");
                } else {
                    pvalue top = state->stack[state->stack_count - 1U];
                    state->stack[state->stack_count - 1U] =
                        state->stack[state->stack_count - 2U];
                    state->stack[state->stack_count - 2U] = top;
                }
                break;
            case OP_LOAD_NULL: (void)push(state, pv_null()); break;
            case OP_LOAD_TRUE: (void)push(state, pv_bool(1)); break;
            case OP_LOAD_FALSE: (void)push(state, pv_bool(0)); break;
            case OP_LOAD_I8: (void)push(state, pv_int((pphp_int)(int8_t)read_u8(state, frame))); break;
            case OP_LOAD_I32: (void)push(state, pv_int((pphp_int)read_i32(state, frame))); break;
            case OP_LOAD_CONST: {
                uint16_t index = read_u16(state, frame);
                if (index >= frame->proto->constant_count) {
                    pphp_runtime_error(state, frame->line, "constant index out of range");
                } else {
                    pvalue value = frame->proto->constants[index];
                    pv_retain(value);
                    (void)push(state, value);
                }
                break;
            }
            case OP_LOAD_LOCAL: {
                uint8_t slot = read_u8(state, frame);
                if (slot >= frame->proto->n_locals) {
                    pphp_runtime_error(state, frame->line, "local index out of range");
                } else {
                    pvalue value = state->stack[frame->base + slot];
                    pv_retain(value);
                    (void)push(state, value);
                }
                break;
            }
            case OP_STORE_LOCAL: {
                uint8_t slot = read_u8(state, frame);
                pvalue value = pop(state);
                if (slot >= frame->proto->n_locals) {
                    pv_release(value);
                    pphp_runtime_error(state, frame->line, "local index out of range");
                } else {
                    pv_release(state->stack[frame->base + slot]);
                    state->stack[frame->base + slot] = value;
                }
                break;
            }
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_POW: case OP_CONCAT: case OP_BAND: case OP_BOR: case OP_BXOR:
            case OP_SHL: case OP_SHR:
                (void)execute_binary(state, opcode);
                break;
            case OP_NEG: {
                pvalue value = pop(state);
                pphp_float number;
                int integer;
                if (!pv_to_number(value, &number, &integer)) {
                    pphp_runtime_error(state, frame->line, "unsupported operand type for unary -");
                } else {
                    (void)push(state, integer ? pv_int(-(pphp_int)number) : pv_float(-number));
                }
                pv_release(value);
                break;
            }
            case OP_BNOT: {
                pvalue value = pop(state);
                pphp_float number;
                int integer;
                if (!pv_to_number(value, &number, &integer)) {
                    pphp_runtime_error(state, frame->line, "unsupported operand type for ~");
                } else {
                    (void)push(state, pv_int(~(pphp_int)number));
                }
                pv_release(value);
                break;
            }
            case OP_EQ: case OP_NE: case OP_IDENT: case OP_NIDENT:
            case OP_LT: case OP_LE: case OP_GT: case OP_GE: case OP_CMP:
                (void)execute_compare(state, opcode);
                break;
            case OP_NOT: {
                pvalue value = pop(state);
                int truth = !pv_is_truthy(value);
                pv_release(value);
                (void)push(state, pv_bool(truth));
                break;
            }
            case OP_JMP:
                (void)jump_relative(state, frame, read_i16(state, frame));
                break;
            case OP_JMP_IF:
            case OP_JMP_UNLESS: {
                int16_t relative = read_i16(state, frame);
                pvalue condition = pop(state);
                int truth = pv_is_truthy(condition);
                pv_release(condition);
                if ((opcode == OP_JMP_IF && truth) || (opcode == OP_JMP_UNLESS && !truth)) {
                    (void)jump_relative(state, frame, relative);
                }
                break;
            }
            case OP_JMP_IF_KEEP:
            case OP_JMP_UNLESS_KEEP:
            case OP_JMP_NOTNULL_KEEP: {
                int16_t relative = read_i16(state, frame);
                pvalue condition;
                int take;
                if (state->stack_count == 0U) {
                    pphp_runtime_error(state, frame->line, "conditional jump on empty stack");
                    break;
                }
                condition = state->stack[state->stack_count - 1U];
                take = opcode == OP_JMP_NOTNULL_KEEP
                           ? condition.type != PT_NULL
                           : (opcode == OP_JMP_IF_KEEP ? pv_is_truthy(condition)
                                                       : !pv_is_truthy(condition));
                if (take) {
                    (void)jump_relative(state, frame, relative);
                } else {
                    pv_release(pop(state));
                }
                break;
            }
            case OP_CALL: {
                uint16_t name = read_u16(state, frame);
                uint8_t count = read_u8(state, frame);
                (void)call_function(state, frame, name, count);
                break;
            }
            case OP_RET: {
                pvalue result = pop(state);
                (void)return_from_function(state, result);
                break;
            }
            case OP_RET_NULL:
                (void)return_from_function(state, pv_null());
                break;
            case OP_ECHO: {
                uint8_t count = read_u8(state, frame);
                size_t base;
                if (state->stack_count < count) {
                    pphp_runtime_error(state, frame->line, "ECHO stack underflow");
                    break;
                }
                base = state->stack_count - count;
                for (i = base; i < state->stack_count; i++) {
                    if (!output_echo(state, state->stack[i])) {
                        break;
                    }
                }
                release_range(state, base, state->stack_count);
                state->stack_count = base;
                break;
            }
            case OP_NEW_ARRAY: {
                uint16_t hint = read_u16(state, frame);
                parray *array = pa_new(hint);
                if (array == NULL) {
                    pphp_runtime_error(state, frame->line, "out of memory creating array");
                } else {
                    (void)push(state, pv_heap(PT_ARRAY, &array->header));
                }
                break;
            }
            case OP_ARR_PUSH:
            case OP_ARR_SET: {
                pvalue value = pop(state);
                pvalue key = pv_null();
                pvalue array_value;
                parray *array;
                int ok;
                if (opcode == OP_ARR_SET) key = pop(state);
                array_value = pop(state);
                if (array_value.type != PT_ARRAY) {
                    pv_release(array_value);
                    pv_release(key);
                    pv_release(value);
                    pphp_runtime_error(state, frame->line, "array construction stack is invalid");
                    break;
                }
                array = (parray *)array_value.as.gc;
                ok = opcode == OP_ARR_PUSH ? pa_push(array, value) : pa_set(array, key, value);
                pv_release(key);
                pv_release(value);
                if (!ok) {
                    pv_release(array_value);
                    pphp_runtime_error(state, frame->line, "array construction failed");
                } else {
                    (void)push(state, array_value);
                }
                break;
            }
            case OP_IDX_GET: {
                pvalue key = pop(state);
                pvalue base = pop(state);
                pvalue value = pv_null();
                if (base.type != PT_ARRAY) {
                    pphp_runtime_error(state, frame->line, "Cannot use a non-array value as an array");
                } else {
                    (void)pa_get((const parray *)base.as.gc, key, &value);
                    (void)push(state, value);
                }
                pv_release(base);
                pv_release(key);
                break;
            }
            case OP_IDX_SET:
                (void)execute_array_set(state, 0);
                break;
            case OP_IDX_APPEND:
                (void)execute_array_set(state, 1);
                break;
            case OP_FE_INIT: {
                pvalue iterable = pop(state);
                parray_iterator *iterator;
                if (iterable.type != PT_ARRAY) {
                    pv_release(iterable);
                    pphp_runtime_error(state, frame->line, "foreach argument must be array");
                    break;
                }
                iterator = pa_iterator_new((parray *)iterable.as.gc);
                pv_release(iterable);
                if (iterator == NULL) {
                    pphp_runtime_error(state, frame->line, "out of memory creating iterator");
                } else {
                    (void)push(state, pv_heap(PT_RESOURCE, &iterator->resource.header));
                }
                break;
            }
            case OP_FE_NEXT: {
                int16_t relative = read_i16(state, frame);
                uint8_t has_key = read_u8(state, frame);
                pvalue iterator_value;
                pvalue key;
                pvalue value;
                if (state->stack_count == 0U ||
                    state->stack[state->stack_count - 1U].type != PT_RESOURCE) {
                    pphp_runtime_error(state, frame->line, "invalid foreach iterator");
                    break;
                }
                iterator_value = state->stack[state->stack_count - 1U];
                if (!pa_iterator_next((parray_iterator *)iterator_value.as.gc, &key, &value)) {
                    pv_release(pop(state));
                    (void)jump_relative(state, frame, relative);
                } else {
                    if (has_key) {
                        (void)push(state, key);
                    } else {
                        pv_release(key);
                    }
                    (void)push(state, value);
                }
                break;
            }
            case OP_FE_FREE:
                if (state->stack_count == 0U ||
                    state->stack[state->stack_count - 1U].type != PT_RESOURCE) {
                    pphp_runtime_error(state, frame->line, "invalid foreach cleanup");
                } else {
                    pv_release(pop(state));
                }
                break;
            case OP_DEF_CLASS: {
                uint16_t name_index = read_u16(state, frame);
                uint16_t parent_index = read_u16(state, frame);
                uint8_t flags = read_u8(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pclass *parent = NULL;
                if (state->building_class != NULL || name == NULL) {
                    pphp_runtime_error(state, frame->line, "invalid nested class definition");
                    break;
                }
                if (parent_index != UINT16_MAX) {
                    const pstring *parent_name = constant_string(state, frame, parent_index);
                    if (parent_name == NULL ||
                        (parent = pphp_find_class(state, parent_name->data,
                                                  parent_name->length)) == NULL) {
                        pphp_runtime_error(state, frame->line, "parent class is not defined");
                        break;
                    }
                    if ((parent->flags & PC_FINAL) != 0U) {
                        pphp_runtime_error(state, frame->line, "cannot extend final class");
                        break;
                    }
                }
                state->building_class = pclass_new(name->data, name->length, parent, flags);
                if (state->building_class == NULL) {
                    pphp_runtime_error(state, frame->line, "cannot create class definition");
                }
                break;
            }
            case OP_DEF_PROP: {
                uint16_t name_index = read_u16(state, frame);
                uint8_t flags = read_u8(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue default_value = pop(state);
                if (state->building_class == NULL || name == NULL ||
                    !pclass_add_property(state->building_class, name->data,
                                         name->length, flags, default_value)) {
                    pphp_runtime_error(state, frame->line, "cannot define property");
                }
                pv_release(default_value);
                break;
            }
            case OP_DEF_METHOD: {
                uint16_t name_index = read_u16(state, frame);
                uint16_t proto_index = read_u16(state, frame);
                uint8_t flags = read_u8(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                if (state->building_class == NULL || name == NULL ||
                    proto_index >= state->module->count ||
                    !pclass_add_method(state->building_class, name->data, name->length,
                                       flags, state->module->protos[proto_index])) {
                    pphp_runtime_error(state, frame->line, "cannot define method");
                }
                break;
            }
            case OP_DEF_END:
                if (state->building_class == NULL ||
                    !pphp_register_class(state, state->building_class)) {
                    pphp_runtime_error(state, frame->line, "cannot register class");
                } else {
                    state->building_class = NULL;
                }
                break;
            case OP_NEW_OBJ: {
                uint16_t name_index = read_u16(state, frame);
                uint8_t argument_count = read_u8(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pclass *class_entry;
                pobject *object;
                const pmethod *constructor;
                size_t base;
                if (name == NULL || state->stack_count < argument_count) break;
                class_entry = pphp_find_class(state, name->data, name->length);
                if (class_entry == NULL) {
                    pphp_runtime_error(state, frame->line, "Class %.*s not found",
                                       (int)name->length, name->data);
                    break;
                }
                object = pobject_new(class_entry);
                if (object == NULL) {
                    pphp_runtime_error(state, frame->line, "cannot instantiate class %.*s",
                                       (int)name->length, name->data);
                    break;
                }
                base = state->stack_count - argument_count;
                if (state->stack_count + 1U >= PPHP_STACK_SLOTS) {
                    pv_release(pv_heap(PT_OBJECT, &object->header));
                    pphp_runtime_error(state, frame->line, "stack overflow constructing object");
                    break;
                }
                memmove(state->stack + base + 1U, state->stack + base,
                        argument_count * sizeof(*state->stack));
                state->stack[base] = pv_heap(PT_OBJECT, &object->header);
                state->stack_count++;
                constructor = pclass_find_method(class_entry, "__construct", 11U);
                if (constructor != NULL) {
                    if (!pclass_member_visible(constructor->flags, constructor->owner,
                                               frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public constructor");
                    } else {
                        (void)enter_method(state, frame, constructor, argument_count, 1);
                    }
                } else if (argument_count != 0U) {
                    pphp_runtime_error(state, frame->line,
                                       "Class %.*s has no constructor",
                                       (int)name->length, name->data);
                }
                break;
            }
            case OP_PROP_GET: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue object_value = pop(state);
                if (name == NULL || object_value.type != PT_OBJECT) {
                    pv_release(object_value);
                    pphp_runtime_error(state, frame->line, "property access on non-object");
                    break;
                }
                {
                    pobject *object = (pobject *)object_value.as.gc;
                    const pproperty *property = pclass_find_property(
                        object->class_entry, name->data, name->length);
                    if (property == NULL) {
                        pphp_runtime_error(state, frame->line, "undefined property %.*s",
                                           (int)name->length, name->data);
                    } else if (!pclass_member_visible(property->flags, property->owner,
                                                      frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public property %.*s",
                                           (int)name->length, name->data);
                    } else {
                        pvalue value = object->slots[property->slot];
                        pv_retain(value);
                        (void)push(state, value);
                    }
                }
                pv_release(object_value);
                break;
            }
            case OP_PROP_SET: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue value = pop(state);
                pvalue object_value = pop(state);
                if (name == NULL || object_value.type != PT_OBJECT) {
                    pv_release(object_value);
                    pv_release(value);
                    pphp_runtime_error(state, frame->line, "property assignment on non-object");
                    break;
                }
                {
                    pobject *object = (pobject *)object_value.as.gc;
                    const pproperty *property = pclass_find_property(
                        object->class_entry, name->data, name->length);
                    if (property == NULL) {
                        pphp_runtime_error(state, frame->line, "undefined property %.*s",
                                           (int)name->length, name->data);
                    } else if (!pclass_member_visible(property->flags, property->owner,
                                                      frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public property %.*s",
                                           (int)name->length, name->data);
                    } else if ((property->flags & PC_READONLY) != 0U &&
                               (frame->called_scope != property->owner ||
                                pobject_property_written(object, property->slot))) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot modify readonly property %.*s",
                                           (int)name->length, name->data);
                    } else {
                        pv_retain(value);
                        pv_release(object->slots[property->slot]);
                        object->slots[property->slot] = value;
                        pobject_mark_property_written(object, property->slot);
                        (void)push(state, value);
                        value = pv_null();
                    }
                }
                pv_release(object_value);
                pv_release(value);
                break;
            }
            case OP_MCALL: {
                uint16_t name_index = read_u16(state, frame);
                uint8_t argument_count = read_u8(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                size_t base;
                pobject *object;
                const pmethod *method;
                if (name == NULL || state->stack_count < (size_t)argument_count + 1U) break;
                base = state->stack_count - argument_count - 1U;
                if (state->stack[base].type != PT_OBJECT) {
                    pphp_runtime_error(state, frame->line, "method call on non-object");
                    break;
                }
                object = (pobject *)state->stack[base].as.gc;
                method = pclass_find_method(object->class_entry, name->data, name->length);
                if (method == NULL) {
                    pphp_runtime_error(state, frame->line, "undefined method %.*s()",
                                       (int)name->length, name->data);
                    break;
                } else if (!pclass_member_visible(method->flags, method->owner,
                                                  frame->called_scope)) {
                    pphp_runtime_error(state, frame->line,
                                       "cannot access non-public method %.*s()",
                                       (int)name->length, name->data);
                } else {
                    (void)enter_method(state, frame, method, argument_count, 0);
                }
                break;
            }
            case OP_INSTANCEOF: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue value = pop(state);
                int matches = 0;
                if (name != NULL && value.type == PT_OBJECT) {
                    pclass *expected = pphp_find_class(state, name->data, name->length);
                    matches = expected != NULL &&
                              pclass_is_a(((pobject *)value.as.gc)->class_entry, expected);
                }
                pv_release(value);
                (void)push(state, pv_bool(matches));
                break;
            }
            case OP_LINE:
                frame->line = read_u16(state, frame);
                break;
            default:
                pphp_runtime_error(state, frame->line, "unknown opcode 0x%02x", opcode);
                break;
        }
    }
    if (state->error[0] != '\0') {
        for (i = 0U; i < state->frame_count; i++) {
            if (state->frames[i].has_return_override) {
                pv_release(state->frames[i].return_override);
                state->frames[i].has_return_override = 0;
            }
        }
        release_range(state, 0U, state->stack_count);
        state->stack_count = 0U;
        state->frame_count = 0U;
        return PPHP_E_RUNTIME;
    }
    return PPHP_OK;
}
