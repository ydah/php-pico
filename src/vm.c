#include "vm.h"

#include "builtins.h"
#include "opcode.h"
#include "parray.h"
#include "resource.h"
#include "pclass.h"
#include "pphp/hal.h"
#include "closure.h"
#include "value_ops.h"
#include "pgems.h"
#include "gc.h"

#include <stdarg.h>
#include <stdio.h>
#if defined(PPHP_HOST)
#include <stdlib.h>
#endif
#include <string.h>

static int trace_enabled(void) {
#if PPHP_TRACE
    return 1;
#elif defined(PPHP_HOST)
    static int cached = -1;
    if (cached < 0) {
        const char *enabled = getenv("PPHP_TRACE");
        cached = enabled != NULL && enabled[0] != '\0' &&
                 !(enabled[0] == '0' && enabled[1] == '\0');
    }
    return cached;
#else
    return 0;
#endif
}

static void trace_instruction(const pphp_state *state, const pframe *frame,
                              size_t pc, uint8_t opcode) {
    char line[112];
    int length;
    if (!trace_enabled()) return;
    length = snprintf(line, sizeof(line),
                      "TRACE %.*s pc=%04zu op=%s stack=%zu\n",
                      (int)frame->proto->name->length,
                      ps_data(frame->proto->name), pc,
                      pphp_opcode_name(opcode), state->stack_count);
    if (length <= 0) return;
    if ((size_t)length >= sizeof(line)) length = (int)sizeof(line) - 1;
#if defined(PPHP_HOST)
    (void)fwrite(line, 1U, (size_t)length, stderr);
#else
    hal_console_write(line, (size_t)length);
#endif
}

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

static int slot_mask_has(const uint8_t *mask, uint8_t slot) {
    return (mask[slot >> 3U] & (uint8_t)(1U << (slot & 7U))) != 0U;
}

static void slot_mask_set(uint8_t *mask, uint8_t slot) {
    mask[slot >> 3U] |= (uint8_t)(1U << (slot & 7U));
}

static void slot_mask_clear(uint8_t *mask, uint8_t slot) {
    mask[slot >> 3U] &= (uint8_t)~(uint8_t)(1U << (slot & 7U));
}

static pvalue undefined_local_value(void) {
    pvalue value = pv_null();
    value.reserved[0] = 1U;
    return value;
}

static int local_value_is_undefined(pvalue value) {
    return value.type == PT_NULL && value.reserved[0] != 0U;
}

#if PPHP_WARNINGS
static void warn_undefined_variable(pphp_state *state, const pframe *frame,
                                    uint8_t slot) {
    const pstring *name = frame->proto->locals[slot];
    pphp_warning(state, frame->line, "Undefined variable $%.*s",
                 (int)name->length, ps_data(name));
}
#else
#define warn_undefined_variable(...) ((void)0)
#endif

static pstring *static_slot_key(pphp_state *state, const pframe *frame,
                                uint8_t slot) {
    const pstring *function_name = frame->proto->name;
    const pstring *local_name = frame->proto->locals[slot];
    size_t length = function_name->length + 2U + local_name->length;
    char *bytes = pphp_alloc(length);
    pstring *key;
    if (bytes == NULL) return NULL;
    memcpy(bytes, ps_data(function_name), function_name->length);
    memcpy(bytes + function_name->length, "::", 2U);
    memcpy(bytes + function_name->length + 2U, ps_data(local_name),
           local_name->length);
    key = psymbol_intern(&state->symbols, bytes, length);
    pphp_free(bytes);
    return key;
}

static parray *slot_storage(pphp_state *state, const pframe *frame,
                            uint8_t slot, pstring **key) {
    if (slot_mask_has(frame->static_mask, slot)) {
        *key = static_slot_key(state, frame, slot);
        return state->statics;
    }
    if (frame->all_globals || slot_mask_has(frame->global_mask, slot)) {
        const pstring *local = frame->proto->locals[slot];
        *key = psymbol_intern(&state->symbols, ps_data(local), local->length);
        return state->globals;
    }
    *key = NULL;
    return NULL;
}

static int read_local_value(pphp_state *state, pframe *frame, uint8_t slot,
                            int quiet, pvalue *result) {
    pstring *key;
    parray *storage = slot_storage(state, frame, slot, &key);
    *result = pv_null();
    if (storage == NULL) {
        pvalue value = state->stack[frame->base + slot];
        if (local_value_is_undefined(value)) {
            if (!quiet) warn_undefined_variable(state, frame, slot);
        } else {
            *result = value;
            pv_retain(*result);
        }
        return 1;
    }
    if (key == NULL) {
        pphp_runtime_error(state, frame->line,
                           "out of memory resolving variable storage");
        return 0;
    }
    if (!pa_get(storage, pv_heap(PT_STRING, &key->header), result) && !quiet) {
        warn_undefined_variable(state, frame, slot);
    }
    return 1;
}

static int load_local_value(pphp_state *state, pframe *frame, uint8_t slot,
                            int quiet) {
    pvalue value;
    return read_local_value(state, frame, slot, quiet, &value) &&
           push(state, value);
}

static int store_local_value(pphp_state *state, pframe *frame, uint8_t slot,
                             pvalue value) {
    pstring *key;
    parray *storage = slot_storage(state, frame, slot, &key);
    if (storage == NULL) {
        pv_release(state->stack[frame->base + slot]);
        state->stack[frame->base + slot] = value;
        return 1;
    }
    if (key == NULL ||
        !pa_set(storage, pv_heap(PT_STRING, &key->header), value)) {
        pv_release(value);
        pphp_runtime_error(state, frame->line,
                           "out of memory storing persistent variable");
        return 0;
    }
    pv_release(value);
    return 1;
}

static int unset_local_value(pphp_state *state, pframe *frame, uint8_t slot) {
    pstring *key;
    parray *storage = slot_storage(state, frame, slot, &key);
    if (storage == NULL) {
        pv_release(state->stack[frame->base + slot]);
        state->stack[frame->base + slot] = undefined_local_value();
        return 1;
    }
    if (key == NULL) {
        pphp_runtime_error(state, frame->line,
                           "out of memory resolving variable storage");
        return 0;
    }
    (void)pa_remove(storage, pv_heap(PT_STRING, &key->header));
    return 1;
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

static int invoke_magic(pphp_state *state, pobject *object, const char *name,
                        size_t length, const pvalue *arguments, size_t count,
                        pvalue *result) {
    const pmethod *method = pclass_find_method(object->class_entry, name, length);
    pstring *method_name;
    parray *callable;
    pvalue callable_value;
    int invoked;
    if (method == NULL || (method->flags & PC_STATIC) != 0U) return 0;
    method_name = ps_new(name, length);
    callable = pa_new(2U);
    if (method_name == NULL || callable == NULL ||
        !pa_push(callable, pv_heap(PT_OBJECT, &object->header)) ||
        !pa_push(callable, pv_heap(PT_STRING, &method_name->header))) {
        ps_destroy(method_name);
        pa_destroy(callable);
        pphp_runtime_error(state, 0U, "out of memory invoking magic method");
        return -1;
    }
    callable_value = pv_heap(PT_ARRAY, &callable->header);
    invoked = pphp_vm_invoke(state, callable_value, arguments, count, result);
    pv_release(callable_value);
    pv_release(pv_heap(PT_STRING, &method_name->header));
    return invoked ? 1 : -1;
}

static pstring *runtime_string(pphp_state *state, pvalue value) {
    if (value.type == PT_OBJECT) {
        pvalue converted = pv_null();
        pstring *copy;
        int invoked = invoke_magic(state, (pobject *)value.as.gc,
                                   "__toString", 10U, NULL, 0U, &converted);
        if (invoked <= 0 || converted.type != PT_STRING) {
            pv_release(converted);
            if (invoked == 0) {
                pphp_runtime_error(state, 0U,
                                   "object cannot be converted to string");
            } else if (invoked > 0) {
                pphp_runtime_error(state, 0U,
                                   "__toString() must return a string");
            }
            return NULL;
        }
        copy = ps_new(ps_data((pstring *)converted.as.gc),
                      ((pstring *)converted.as.gc)->length);
        pv_release(converted);
        return copy;
    }
    return pv_to_string(value);
}

static int output_echo(pphp_state *state, pvalue value) {
    pstring *string = runtime_string(state, value);
    if (string == NULL) {
        pphp_runtime_error(state, 0U, "value cannot be converted to string");
        return 0;
    }
    pphp_output(state, ps_data(string), string->length);
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
    unsigned non_numeric_operands = 0U;
    int ok;
    if (opcode == OP_CONCAT &&
        (left.type == PT_OBJECT || right.type == PT_OBJECT)) {
        pstring *left_string = runtime_string(state, left);
        pstring *right_string = runtime_string(state, right);
        if (left_string == NULL || right_string == NULL) {
            ps_destroy(left_string);
            ps_destroy(right_string);
            ok = 0;
            error = "value cannot be converted to string";
        } else {
            pvalue left_value = pv_heap(PT_STRING, &left_string->header);
            pvalue right_value = pv_heap(PT_STRING, &right_string->header);
            ok = pv_binary_operation(PV_CONCAT, left_value, right_value,
                                     &result, &error, NULL);
            pv_release(left_value);
            pv_release(right_value);
        }
    } else {
        ok = pv_binary_operation(operation_for(opcode), left, right,
                                 &result, &error,
                                 &non_numeric_operands);
    }
    if ((non_numeric_operands & 1U) != 0U) {
        pphp_warning(state, 0U, "A non-numeric value encountered");
    }
    if ((non_numeric_operands & 2U) != 0U) {
        pphp_warning(state, 0U, "A non-numeric value encountered");
    }
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
    if (array_value.type == PT_NULL) {
        array = pa_new(4U);
        pv_release(array_value);
        if (array == NULL) {
            pv_release(key);
            pv_release(value);
            pphp_runtime_error(state, 0U,
                               "out of memory creating array");
            return 0;
        }
        array_value = pv_heap(PT_ARRAY, &array->header);
    }
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
        pphp_runtime_error(state, 0U, "out of memory updating array");
        return 0;
    }
    if (!push(state, array_value)) {
        pv_release(value);
        return 0;
    }
    return push(state, value);
}

static int execute_array_unset(pphp_state *state) {
    pvalue key = pop(state);
    pvalue array_value = pop(state);
    parray *array;
    parray *writable;
    if (array_value.type != PT_ARRAY) {
        pv_release(array_value);
        pv_release(key);
        pphp_runtime_error(state, 0U,
                           "Cannot use a non-array value as an array");
        return 0;
    }
    array = (parray *)array_value.as.gc;
    writable = array_for_write(array_value);
    if (writable == NULL) {
        pv_release(array_value);
        pv_release(key);
        pphp_runtime_error(state, 0U,
                           "out of memory separating array");
        return 0;
    }
    if (writable != array) {
        pv_release(array_value);
        array_value = pv_heap(PT_ARRAY, &writable->header);
    }
    (void)pa_remove(writable, key);
    pv_release(key);
    return push(state, array_value);
}

static int execute_index_get(pphp_state *state, int quiet) {
    pvalue key = pop(state);
    pvalue base = pop(state);
    pvalue value = pv_null();
    if (base.type == PT_ARRAY) {
        (void)pa_get((const parray *)base.as.gc, key, &value);
    } else if (base.type == PT_STRING) {
        pphp_numeric numeric;
        pphp_int offset;
        const pstring *string = (const pstring *)base.as.gc;
        if (pv_to_numeric(key, 1, &numeric) &&
            pphp_numeric_to_integer(&numeric, 0, &offset)) {
            if (offset < 0) offset += (pphp_int)string->length;
            if (offset >= 0 && (size_t)offset < string->length) {
                pstring *character = ps_new(ps_data(string) + offset, 1U);
                if (character == NULL) {
                    pphp_runtime_error(state, 0U,
                                       "out of memory reading string offset");
                } else {
                    value = pv_heap(PT_STRING, &character->header);
                }
            }
        }
    } else if (!quiet && base.type != PT_NULL) {
        pphp_runtime_error(state, 0U,
                           "Cannot use a non-array value as an array");
    }
    pv_release(base);
    pv_release(key);
    return state->error[0] == '\0' && push(state, value);
}

static int execute_cast(pphp_state *state, uint8_t target) {
    pvalue value = pop(state);
    pvalue result = pv_null();
    if (target == PT_TRUE) {
        result = pv_bool(pv_is_truthy(value));
    } else if (target == PT_INT
#if PPHP_ENABLE_FLOAT
               || target == PT_FLOAT
#endif
    ) {
        pphp_numeric numeric;
        if (!pv_to_numeric(value, value.type != PT_STRING, &numeric) &&
            numeric.is_integer < 0) {
            pv_release(value);
            pphp_runtime_error(state, 0U,
                               "integer overflow requires float support");
            return 0;
        }
        if (target == PT_INT) {
            pphp_int integer_value;
            if (!pphp_numeric_to_integer(&numeric, 0, &integer_value)) {
                pv_release(value);
                pphp_runtime_error(state, 0U,
                                   "integer conversion is out of range");
                return 0;
            }
            result = pv_int(integer_value);
        }
#if PPHP_ENABLE_FLOAT
        if (target == PT_FLOAT) result = pv_float(numeric.number);
#endif
    } else if (target == PT_STRING) {
        pstring *string = pv_to_string(value);
        if (string == NULL) {
            pv_release(value);
            pphp_runtime_error(state, 0U,
                               "value cannot be converted to string");
            return 0;
        }
        result = pv_heap(PT_STRING, &string->header);
    } else if (target == PT_ARRAY) {
        if (value.type == PT_ARRAY) {
            result = value;
            pv_retain(result);
        } else {
            parray *array = pa_new(value.type == PT_NULL ? 0U : 1U);
            if (array == NULL ||
                (value.type != PT_NULL && !pa_push(array, value))) {
                if (array != NULL) {
                    pv_release(pv_heap(PT_ARRAY, &array->header));
                }
                pv_release(value);
                pphp_runtime_error(state, 0U,
                                   "out of memory casting to array");
                return 0;
            }
            result = pv_heap(PT_ARRAY, &array->header);
        }
    } else {
        pv_release(value);
        pphp_runtime_error(state, 0U, "invalid cast target");
        return 0;
    }
    pv_release(value);
    return push(state, result);
}

static void release_range(pphp_state *state, size_t begin, size_t end) {
    size_t i;
    for (i = begin; i < end; i++) {
        pv_release(state->stack[i]);
    }
}

static void release_frame_owners(pframe *frame) {
    if (frame == NULL) return;
    if (frame->has_return_override) {
        pv_release(frame->return_override);
        frame->return_override = pv_null();
        frame->has_return_override = 0;
    }
    if (!frame->owns_closure_context) return;
    frame->owns_closure_context = 0U;
    pmodule_release((pmodule *)frame->module);
    pclass_release_runtime(frame->called_scope);
    pclass_release_runtime(frame->called_class);
    frame->module = NULL;
    frame->called_scope = NULL;
    frame->called_class = NULL;
}

static void release_all_frames(pphp_state *state) {
    while (state != NULL && state->frame_count != 0U) {
        state->frame_count--;
        release_frame_owners(&state->frames[state->frame_count]);
    }
}

static int throw_exception(pphp_state *state, pvalue exception,
                           size_t throw_pc) {
    if (exception.type == PT_OBJECT) {
        pphp_exception_capture_trace(state, (pobject *)exception.as.gc);
    }
    while (state->frame_count != 0U) {
        pframe *frame = &state->frames[state->frame_count - 1U];
        size_t i;
        for (i = 0U; i < frame->proto->catch_count; i++) {
            const pcatch *entry = &frame->proto->catches[i];
            int matches = 0;
            if (throw_pc < entry->try_start || throw_pc >= entry->try_end) continue;
            if (entry->class_constant == UINT16_MAX) {
                matches = 1;
            } else if (entry->class_constant < frame->proto->constant_count &&
                       frame->proto->constants[entry->class_constant].type == PT_STRING) {
                const pstring *name = (const pstring *)
                    frame->proto->constants[entry->class_constant].as.gc;
                pclass *expected = pphp_find_class(state, ps_data(name), name->length);
                matches = exception.type == PT_OBJECT && expected != NULL &&
                          pclass_is_a(((pobject *)exception.as.gc)->class_entry, expected);
            }
            if (!matches) continue;
            release_range(state, frame->base + frame->proto->n_locals,
                          state->stack_count);
            state->stack_count = frame->base + frame->proto->n_locals;
            if (entry->class_constant == UINT16_MAX) {
                if (entry->variable_slot != UINT8_MAX &&
                    entry->variable_slot < frame->proto->n_locals) {
                    (void)store_local_value(state, frame,
                                            entry->variable_slot, exception);
                } else {
                    pv_release(exception);
                    pphp_runtime_error(state, frame->line,
                                       "finally handler has no pending exception slot");
                    return 0;
                }
            } else if (entry->variable_slot != UINT8_MAX &&
                       entry->variable_slot < frame->proto->n_locals) {
                (void)store_local_value(state, frame, entry->variable_slot,
                                        exception);
            } else {
                pv_release(exception);
            }
            frame->pc = entry->handler_pc;
            return 1;
        }
        release_range(state, frame->base, state->stack_count);
        state->stack_count = frame->base;
        release_frame_owners(frame);
        state->frame_count--;
        if (state->frame_count != 0U) {
            pframe *caller = &state->frames[state->frame_count - 1U];
            throw_pc = caller->pc == 0U ? 0U : caller->pc - 1U;
        }
    }
    if (exception.type == PT_OBJECT) {
        pobject *object = (pobject *)exception.as.gc;
        const pstring *message = pphp_exception_message(object);
        const pvalue *file_value = pphp_exception_field(object, "file", 4U);
        const pvalue *line_value = pphp_exception_field(object, "line", 4U);
        const pstring *file = file_value != NULL && file_value->type == PT_STRING
                                  ? (const pstring *)file_value->as.gc
                                  : NULL;
        uint32_t line = line_value != NULL && line_value->type == PT_INT &&
                                line_value->as.i >= 0
                            ? (uint32_t)line_value->as.i
                            : 0U;
        pphp_runtime_error(state, line,
                           "PHP Fatal error: Uncaught %.*s: %.*s in %.*s:%lu",
                           (int)object->class_entry->name->length,
                           ps_data(object->class_entry->name),
                           message == NULL ? 0 : (int)message->length,
                           message == NULL ? "" : ps_data(message),
                           file == NULL ? 8 : (int)file->length,
                           file == NULL ? "<source>" : ps_data(file),
                           (unsigned long)line);
    } else {
        pphp_runtime_error(state, 0U, "Uncaught non-object exception");
    }
    pv_release(exception);
    return 0;
}

static void convert_runtime_error(pphp_state *state, size_t throw_pc) {
    char message[sizeof(state->error)];
    char raised_class[sizeof(state->raised_class)];
    const char *class_name = "Error";
    pobject *exception;
    if (state->error[0] == '\0' || state->frame_count == 0U) return;
    (void)snprintf(message, sizeof(message), "%s", state->error);
    if (state->error_line == 0U) {
        state->error_line = state->frames[state->frame_count - 1U].line;
    }
    if (state->raised_class[0] != '\0') {
        (void)snprintf(raised_class, sizeof(raised_class), "%s",
                       state->raised_class);
        class_name = raised_class;
    } else if (ps_contains_cstr(message, "Division by zero") ||
        ps_contains_cstr(message, "Modulo by zero")) {
        class_name = "DivisionByZeroError";
    } else if (ps_contains_cstr(message, "out of memory") ||
               ps_contains_cstr(message, "Out of memory")) {
        class_name = "OutOfMemoryError";
    } else if (ps_contains_cstr(message, "Too few arguments") ||
               ps_contains_cstr(message, "Too many arguments") ||
               ps_contains_cstr(message, "expects exactly") ||
               ps_contains_cstr(message, "expects at most")) {
        class_name = "ArgumentCountError";
    } else if (ps_contains_cstr(message, "expects") ||
               ps_contains_cstr(message, "unsupported operand")) {
        class_name = "TypeError";
    }
    state->error[0] = '\0';
    state->raised_class[0] = '\0';
    if (strcmp(class_name, "OutOfMemoryError") == 0 &&
        state->oom_exception != NULL) {
        exception = state->oom_exception;
        pv_retain(pv_heap(PT_OBJECT, &exception->header));
    } else {
        exception = pphp_exception_new(state, class_name, message);
    }
    if (exception == NULL) {
        (void)snprintf(state->error, sizeof(state->error), "%s", message);
        return;
    }
    (void)throw_exception(state, pv_heap(PT_OBJECT, &exception->header), throw_pc);
}

static int enter_method(pphp_state *state, pframe *caller,
                        const pmethod *method, uint8_t argument_count,
                        int constructor);
static int enter_static_method(pphp_state *state, pframe *caller,
                               const pmethod *method, pclass *called_class,
                               uint8_t argument_count);

static const pmethod *find_static_callable_method(
    pphp_state *state, const pframe *frame, const char *class_name,
    size_t class_length, const char *method_name, size_t method_length,
    pclass **called_class) {
    pclass *class_entry = pphp_find_class(state, class_name, class_length);
    const pmethod *method;
    if (called_class != NULL) *called_class = NULL;
    if (class_entry == NULL) return NULL;
    method = pclass_find_method(class_entry, method_name, method_length);
    if (method == NULL || (method->flags & PC_STATIC) == 0U ||
        !pclass_member_visible(method->flags, method->owner,
                               frame->called_scope)) return NULL;
    if (called_class != NULL) *called_class = class_entry;
    return method;
}

static const pmethod *find_string_static_callable(
    pphp_state *state, const pframe *frame, const pstring *callable,
    pclass **called_class) {
    const char *bytes = ps_data(callable);
    size_t i;
    for (i = 1U; i + 2U < callable->length; i++) {
        if (bytes[i] == ':' && bytes[i + 1U] == ':') {
            return find_static_callable_method(
                state, frame, bytes, i, bytes + i + 2U,
                callable->length - i - 2U, called_class);
        }
    }
    return NULL;
}

static int expand_argument_array(pphp_state *state, uint32_t line,
                                 uint8_t *argument_count) {
    pvalue array_value = pop(state);
    const parray *array;
    size_t position = 0U;
    uint8_t count = 0U;
    if (array_value.type != PT_ARRAY) {
        pv_release(array_value);
        pphp_runtime_error(state, line, "only arrays can be unpacked as arguments");
        return 0;
    }
    array = (const parray *)array_value.as.gc;
    while (position < array->used) {
        pvalue key;
        pvalue value;
        size_t next;
        if (!pa_entry_at(array, position, &key, &value, &next)) break;
        if (key.type != PT_INT) {
            pv_release(key);
            pv_release(value);
            pv_release(array_value);
            pphp_runtime_error(state, line,
                               "named arguments are not supported in argument unpacking");
            return 0;
        }
        pv_release(key);
        if (count >= 31U) {
            pv_release(value);
            pv_release(array_value);
            pphp_runtime_error(state, line, "a call may have at most 31 arguments");
            return 0;
        }
        if (!push(state, value)) {
            pv_release(array_value);
            return 0;
        }
        count++;
        position = next;
    }
    pv_release(array_value);
    *argument_count = count;
    return 1;
}

#if PPHP_TYPECHECK
static pclass *type_scope_class(pphp_state *state, const pframe *frame,
                                const ptype_member *member) {
    if (member->kind == PTYPE_SELF) return frame->called_scope;
    if (member->kind == PTYPE_PARENT) {
        return frame->called_scope == NULL ? NULL : frame->called_scope->parent;
    }
    if (member->kind == PTYPE_STATIC) {
        return frame->called_class == NULL ? frame->called_scope
                                           : frame->called_class;
    }
    if (member->kind != PTYPE_NAMED || member->name == NULL) return NULL;
    return pphp_find_class(state, ps_data(member->name), member->name->length);
}

static int value_is_callable(pphp_state *state, const pframe *frame,
                             pvalue value) {
    if (value.type == PT_CLOSURE || value.type == PT_CFUNC) return 1;
    if (value.type == PT_STRING) {
        const pstring *name = (const pstring *)value.as.gc;
        return pphp_builtin_exists(name) ||
               pphp_native_function_exists(state, name) ||
               pphp_find_function(state, name, NULL) != NULL ||
               find_string_static_callable(state, frame, name, NULL) != NULL;
    }
    if (value.type == PT_OBJECT) {
        const pmethod *method = pclass_find_method(
            ((const pobject *)value.as.gc)->class_entry, "__invoke", 8U);
        return method != NULL && (method->flags & PC_STATIC) == 0U &&
               pclass_member_visible(
            method->flags, method->owner, frame->called_scope);
    }
    if (value.type == PT_ARRAY) {
        pvalue target = pv_null();
        pvalue method_name = pv_null();
        const pmethod *method = NULL;
        int valid = pa_get((const parray *)value.as.gc, pv_int(0), &target) &&
                    pa_get((const parray *)value.as.gc, pv_int(1), &method_name) &&
                    pa_count((const parray *)value.as.gc) == 2U &&
                    (target.type == PT_OBJECT || target.type == PT_STRING) &&
                    method_name.type == PT_STRING;
        if (valid) {
            const pstring *name = (const pstring *)method_name.as.gc;
            if (target.type == PT_OBJECT) {
                method = pclass_find_method(
                    ((const pobject *)target.as.gc)->class_entry,
                    ps_data(name), name->length);
                valid = method != NULL && pclass_member_visible(
                    method->flags, method->owner, frame->called_scope);
            } else {
                const pstring *class_name = (const pstring *)target.as.gc;
                method = find_static_callable_method(
                    state, frame, ps_data(class_name), class_name->length,
                    ps_data(name), name->length, NULL);
                valid = method != NULL;
            }
        }
        pv_release(target);
        pv_release(method_name);
        return valid;
    }
    return 0;
}

static int type_member_accepts(pphp_state *state, const pframe *frame,
                               const ptype_member *member, pvalue value) {
    pclass *expected;
    switch (member->kind) {
        case PTYPE_INT: return value.type == PT_INT;
#if PPHP_ENABLE_FLOAT
        case PTYPE_FLOAT: return value.type == PT_FLOAT;
#endif
        case PTYPE_STRING: return value.type == PT_STRING;
        case PTYPE_BOOL: return value.type == PT_FALSE || value.type == PT_TRUE;
        case PTYPE_ARRAY: return value.type == PT_ARRAY;
        case PTYPE_CALLABLE: return value_is_callable(state, frame, value);
        case PTYPE_MIXED: return 1;
        case PTYPE_VOID:
        case PTYPE_NULL: return value.type == PT_NULL;
        case PTYPE_SELF:
        case PTYPE_STATIC:
        case PTYPE_PARENT:
        case PTYPE_NAMED:
            expected = type_scope_class(state, frame, member);
            return expected != NULL && value.type == PT_OBJECT &&
                   pclass_is_a(((const pobject *)value.as.gc)->class_entry,
                               expected);
        default: return 0;
    }
}

static int type_spec_accepts(pphp_state *state, const pframe *frame,
                             const ptype_spec *spec, pvalue value) {
    size_t i;
    if (spec == NULL || spec->count == 0U) return 1;
    for (i = 0U; i < spec->count; i++) {
        if (type_member_accepts(state, frame, &spec->members[i], value)) return 1;
    }
    return 0;
}

static const char *type_kind_name(const ptype_member *member) {
    switch (member->kind) {
        case PTYPE_INT: return "int";
        case PTYPE_FLOAT: return "float";
        case PTYPE_STRING: return "string";
        case PTYPE_BOOL: return "bool";
        case PTYPE_ARRAY: return "array";
        case PTYPE_CALLABLE: return "callable";
        case PTYPE_MIXED: return "mixed";
        case PTYPE_VOID: return "void";
        case PTYPE_NULL: return "null";
        case PTYPE_SELF: return "self";
        case PTYPE_STATIC: return "static";
        case PTYPE_PARENT: return "parent";
        case PTYPE_NAMED:
            return member->name == NULL ? "object" : ps_data(member->name);
        default: return "unknown";
    }
}

static void type_spec_name(const ptype_spec *spec, char *output,
                           size_t capacity) {
    size_t used = 0U;
    size_t i;
    if (capacity == 0U) return;
    output[0] = '\0';
    for (i = 0U; spec != NULL && i < spec->count; i++) {
        const char *name = type_kind_name(&spec->members[i]);
        size_t length = spec->members[i].kind == PTYPE_NAMED &&
                                spec->members[i].name != NULL
                            ? spec->members[i].name->length : strlen(name);
        if (i != 0U && used + 1U < capacity) output[used++] = '|';
        if (length >= capacity - used) length = capacity - used - 1U;
        memcpy(output + used, name, length);
        used += length;
        output[used] = '\0';
    }
}

static void raise_declared_type_error(pphp_state *state, uint32_t line,
                                      const char *format, ...) {
    va_list arguments;
    if (state == NULL || state->error[0] != '\0') return;
    va_start(arguments, format);
    (void)vsnprintf(state->error, sizeof(state->error), format, arguments);
    va_end(arguments);
    state->error_line = line;
    (void)snprintf(state->raised_class, sizeof(state->raised_class),
                   "%s", "TypeError");
}

static int type_name_equal(const char *bytes, size_t length,
                           const char *expected) {
    size_t i;
    size_t expected_length = strlen(expected);
    if (length != expected_length) return 0;
    for (i = 0U; i < length; i++) {
        unsigned char left = (unsigned char)bytes[i];
        unsigned char right = (unsigned char)expected[i];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left - 'A' + 'a');
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right - 'A' + 'a');
        if (left != right) return 0;
    }
    return 1;
}

static uint8_t encoded_type_kind(const char *bytes, size_t length) {
    if (type_name_equal(bytes, length, "int")) return PTYPE_INT;
    if (type_name_equal(bytes, length, "float")) return PTYPE_FLOAT;
    if (type_name_equal(bytes, length, "string")) return PTYPE_STRING;
    if (type_name_equal(bytes, length, "bool")) return PTYPE_BOOL;
    if (type_name_equal(bytes, length, "array")) return PTYPE_ARRAY;
    if (type_name_equal(bytes, length, "callable")) return PTYPE_CALLABLE;
    if (type_name_equal(bytes, length, "mixed")) return PTYPE_MIXED;
    if (type_name_equal(bytes, length, "void")) return PTYPE_VOID;
    if (type_name_equal(bytes, length, "null")) return PTYPE_NULL;
    if (type_name_equal(bytes, length, "self")) return PTYPE_SELF;
    if (type_name_equal(bytes, length, "static")) return PTYPE_STATIC;
    if (type_name_equal(bytes, length, "parent")) return PTYPE_PARENT;
    return PTYPE_NAMED;
}

static int parse_property_type(const pstring *encoded, ptype_spec *spec) {
    const char *bytes;
    size_t start = 0U;
    size_t i;
    memset(spec, 0, sizeof(*spec));
    if (encoded == NULL) return 1;
    bytes = ps_data(encoded);
    for (i = 0U; i <= encoded->length; i++) {
        if (i == encoded->length || bytes[i] == '|') {
            size_t length = i - start;
            uint8_t kind;
            if (length == 0U || spec->count >= 16U) goto failed;
            kind = encoded_type_kind(bytes + start, length);
#if !PPHP_ENABLE_FLOAT
            if (kind == PTYPE_FLOAT) goto failed;
#endif
            if (!ptype_spec_add(spec, kind,
                                kind == PTYPE_NAMED ? bytes + start : NULL,
                                kind == PTYPE_NAMED ? length : 0U)) goto failed;
            start = i + 1U;
        }
    }
    return 1;
failed:
    ptype_spec_destroy(spec);
    return 0;
}

static int check_property_type(pphp_state *state, const pclass *actual_class,
                               const pproperty *property, pvalue value,
                               uint32_t line) {
    pframe scope;
    char expected[128];
    if (property == NULL || property->type.count == 0U) return 1;
    memset(&scope, 0, sizeof(scope));
    scope.called_scope = property->owner;
    scope.called_class = (pclass *)actual_class;
    if (type_spec_accepts(state, &scope, &property->type, value)) return 1;
    type_spec_name(&property->type, expected, sizeof(expected));
    raise_declared_type_error(
        state, line, "Cannot assign %s to property %.*s::$%.*s of type %s",
        pv_type_name((pvalue_type)value.type),
        property->owner == NULL ? 0 : (int)property->owner->name->length,
        property->owner == NULL ? "" : ps_data(property->owner->name),
        (int)property->name->length, ps_data(property->name), expected);
    return 0;
}

static int check_argument_types(pphp_state *state, pframe *frame) {
    const pproto *proto = frame->proto;
    size_t local_offset = proto->is_method ? 1U : 0U;
    size_t fixed_count = proto->variadic && proto->n_params != 0U
                             ? (size_t)proto->n_params - 1U
                             : proto->n_params;
    size_t i;
    char expected[128];
    if (proto->parameter_types == NULL) return 1;
    for (i = 0U; i < fixed_count; i++) {
        pvalue value = state->stack[frame->base + local_offset + i];
        const ptype_spec *spec = &proto->parameter_types[i];
        if (type_spec_accepts(state, frame, spec, value)) continue;
        type_spec_name(spec, expected, sizeof(expected));
        raise_declared_type_error(
            state, frame->line,
            "%.*s(): Argument #%zu ($%.*s) must be of type %s, %s given",
            (int)proto->name->length, ps_data(proto->name), i + 1U,
            (int)proto->locals[local_offset + i]->length,
            ps_data(proto->locals[local_offset + i]), expected,
            pv_type_name((pvalue_type)value.type));
        return 0;
    }
    if (proto->variadic && proto->n_params != 0U) {
        const ptype_spec *spec = &proto->parameter_types[fixed_count];
        pvalue rest_value = state->stack[frame->base + local_offset + fixed_count];
        size_t position = 0U;
        size_t extra = 0U;
        if (rest_value.type != PT_ARRAY) return 1;
        while (position < ((const parray *)rest_value.as.gc)->used) {
            pvalue key;
            pvalue value;
            size_t next;
            if (!pa_entry_at((const parray *)rest_value.as.gc, position,
                             &key, &value, &next)) break;
            pv_release(key);
            if (!type_spec_accepts(state, frame, spec, value)) {
                type_spec_name(spec, expected, sizeof(expected));
                raise_declared_type_error(
                    state, frame->line,
                    "%.*s(): Argument #%zu ($%.*s) must be of type %s, %s given",
                    (int)proto->name->length, ps_data(proto->name),
                    fixed_count + extra + 1U,
                    (int)proto->locals[local_offset + fixed_count]->length,
                    ps_data(proto->locals[local_offset + fixed_count]), expected,
                    pv_type_name((pvalue_type)value.type));
                pv_release(value);
                return 0;
            }
            pv_release(value);
            position = next;
            extra++;
        }
    }
    return 1;
}
#endif

static int bind_arguments(pphp_state *state, const pproto *callee,
                          size_t target_base, size_t argument_base,
                          uint8_t argument_count, size_t local_offset,
                          uint32_t line) {
    size_t fixed_count = callee->n_params;
    size_t supplied;
    size_t i;
    parray *rest = NULL;
    if (callee->variadic) {
        if (fixed_count == 0U) {
            pphp_runtime_error(state, line, "invalid variadic function metadata");
            return 0;
        }
        fixed_count--;
    }
    if (target_base + callee->n_locals >= PPHP_STACK_SLOTS ||
        local_offset + callee->n_params > callee->n_locals) {
        pphp_runtime_error(state, line, "value stack overflow entering function");
        return 0;
    }
    supplied = argument_count < fixed_count ? argument_count : fixed_count;
    if (callee->variadic) {
        size_t extra_count = argument_count > fixed_count
                                 ? (size_t)argument_count - fixed_count
                                 : 0U;
        rest = pa_new(extra_count);
        if (rest == NULL) {
            pphp_runtime_error(state, line,
                               "out of memory collecting variadic arguments");
            return 0;
        }
        for (i = 0U; i < extra_count; i++) {
            if (!pa_push(rest, state->stack[argument_base + fixed_count + i])) {
                pv_release(pv_heap(PT_ARRAY, &rest->header));
                pphp_runtime_error(state, line,
                                   "out of memory collecting variadic arguments");
                return 0;
            }
        }
    }
    if ((size_t)argument_count > fixed_count) {
        release_range(state, argument_base + fixed_count,
                      argument_base + argument_count);
    }
    if (supplied != 0U && target_base + local_offset != argument_base) {
        memmove(state->stack + target_base + local_offset,
                state->stack + argument_base, supplied * sizeof(*state->stack));
    }
    for (i = supplied; i < fixed_count; i++) {
        state->stack[target_base + local_offset + i] = undefined_local_value();
    }
    if (callee->variadic) {
        state->stack[target_base + local_offset + fixed_count] =
            pv_heap(PT_ARRAY, &rest->header);
    }
    for (i = local_offset + callee->n_params; i < callee->n_locals; i++) {
        state->stack[target_base + i] = undefined_local_value();
    }
    return 1;
}

static int call_function(pphp_state *state, pframe *caller, uint16_t name_index,
                         uint8_t argument_count) {
    const pvalue *name_value;
    const pstring *name;
    size_t argument_base;
    pvalue result = pv_null();
    int builtin;
    const pproto *callee;
    const pmodule *callee_module;
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
    if (builtin == 0) {
        builtin = pphp_call_native_function(
            state, name, state->stack + argument_base, argument_count,
            &result);
    }
    if (builtin != 0) {
        release_range(state, argument_base, state->stack_count);
        state->stack_count = argument_base;
        return builtin > 0 && push(state, result);
    }
    callee = pphp_find_function(state, name, &callee_module);
    if (callee == NULL) {
        pphp_runtime_error(state, caller->line, "Call to undefined function %.*s()",
                           (int)name->length, ps_data(name));
        return 0;
    }
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to function %.*s(), %u required, %u given",
                           (int)name->length, ps_data(name), callee->n_required,
                           argument_count);
        return 0;
    }
    if (state->frame_count >= PPHP_FRAME_MAX) {
        pphp_runtime_error(state, caller->line, "call stack overflow");
        return 0;
    }
    if (!bind_arguments(state, callee, argument_base, argument_base,
                        argument_count, 0U, caller->line)) {
        return 0;
    }
    state->stack_count = argument_base + callee->n_locals;
    memset(&state->frames[state->frame_count], 0,
           sizeof(state->frames[state->frame_count]));
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].module = callee_module;
    state->frames[state->frame_count].pc = 0U;
    state->frames[state->frame_count].base = argument_base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].argument_count = argument_count;
    state->frames[state->frame_count].has_return_override = 0;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = NULL;
    state->frames[state->frame_count].called_class = NULL;
    state->frame_count++;
    return 1;
}

static int call_named_value(pphp_state *state, pframe *caller,
                            const pstring *name, uint8_t argument_count,
                            size_t base) {
    pvalue callable = state->stack[base];
    pvalue result = pv_null();
    const pproto *callee;
    const pmodule *callee_module;
    int builtin = pphp_call_builtin(state, name, state->stack + base + 1U,
                                    argument_count, &result);
    if (builtin == 0) {
        builtin = pphp_call_native_function(
            state, name, state->stack + base + 1U, argument_count, &result);
    }
    if (builtin != 0) {
        release_range(state, base, state->stack_count);
        state->stack_count = base;
        return builtin > 0 && push(state, result);
    }
    callee = pphp_find_function(state, name, &callee_module);
    if (callee == NULL) {
        pphp_runtime_error(state, caller->line,
                           "Call to undefined function %.*s()",
                           (int)name->length, ps_data(name));
        return 0;
    }
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to function %.*s(), %u required, %u given",
                           (int)name->length, ps_data(name), callee->n_required,
                           argument_count);
        return 0;
    }
    if (state->frame_count >= PPHP_FRAME_MAX) {
        pphp_runtime_error(state, caller->line,
                           "stack overflow entering callable function");
        return 0;
    }
    if (!bind_arguments(state, callee, base, base + 1U, argument_count,
                        0U, caller->line)) {
        return 0;
    }
    pv_release(callable);
    state->stack_count = base + callee->n_locals;
    memset(&state->frames[state->frame_count], 0,
           sizeof(state->frames[state->frame_count]));
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].module = callee_module;
    state->frames[state->frame_count].pc = 0U;
    state->frames[state->frame_count].base = base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].argument_count = argument_count;
    state->frames[state->frame_count].has_return_override = 0;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = NULL;
    state->frames[state->frame_count].called_class = NULL;
    state->frame_count++;
    return 1;
}

static int call_value(pphp_state *state, pframe *caller,
                      uint8_t argument_count) {
    size_t base;
    size_t i;
    pvalue callable;
    pclosure *closure;
    const pproto *callee;
    if (state->stack_count < (size_t)argument_count + 1U) {
        pphp_runtime_error(state, caller->line, "invalid CALL_VALUE instruction");
        return 0;
    }
    base = state->stack_count - argument_count - 1U;
    callable = state->stack[base];
    if (callable.type == PT_STRING) {
        pclass *called_class = NULL;
        const pmethod *method = find_string_static_callable(
            state, caller, (const pstring *)callable.as.gc, &called_class);
        if (method != NULL) {
            pv_release(callable);
            memmove(state->stack + base, state->stack + base + 1U,
                    (size_t)argument_count * sizeof(*state->stack));
            state->stack_count--;
            return enter_static_method(state, caller, method, called_class,
                                       argument_count);
        }
        return call_named_value(state, caller,
                                (const pstring *)callable.as.gc,
                                argument_count, base);
    }
    if (callable.type == PT_ARRAY) {
        pvalue target = pv_null();
        pvalue method_name = pv_null();
        const pmethod *method;
        pclass *called_class = NULL;
        if (!pa_get((const parray *)callable.as.gc, pv_int(0), &target) ||
            !pa_get((const parray *)callable.as.gc, pv_int(1), &method_name) ||
            pa_count((const parray *)callable.as.gc) != 2U ||
            (target.type != PT_OBJECT && target.type != PT_STRING) ||
            method_name.type != PT_STRING) {
            pv_release(target);
            pv_release(method_name);
            pphp_runtime_error(state, caller->line, "array is not a valid callable");
            return 0;
        }
        if (target.type == PT_OBJECT) {
            method = pclass_find_method(
                ((pobject *)target.as.gc)->class_entry,
                ps_data((const pstring *)method_name.as.gc),
                ((const pstring *)method_name.as.gc)->length);
            if (method != NULL && (method->flags & PC_STATIC) != 0U) {
                called_class = ((pobject *)target.as.gc)->class_entry;
            }
        } else {
            const pstring *class_name = (const pstring *)target.as.gc;
            const pstring *name = (const pstring *)method_name.as.gc;
            method = find_static_callable_method(
                state, caller, ps_data(class_name), class_name->length,
                ps_data(name), name->length, &called_class);
        }
        if (method == NULL || !pclass_member_visible(
                method->flags, method->owner, caller->called_scope)) {
            pv_release(target);
            pv_release(method_name);
            pphp_runtime_error(state, caller->line,
                               "array callable method is not accessible");
            return 0;
        }
        pv_release(callable);
        pv_release(method_name);
        if (called_class != NULL) {
            pv_release(target);
            memmove(state->stack + base, state->stack + base + 1U,
                    (size_t)argument_count * sizeof(*state->stack));
            state->stack_count--;
            return enter_static_method(state, caller, method, called_class,
                                       argument_count);
        }
        state->stack[base] = target;
        return enter_method(state, caller, method, argument_count, 0);
    }
    if (callable.type == PT_OBJECT) {
        pobject *object = (pobject *)callable.as.gc;
        const pmethod *invoke = pclass_find_method(object->class_entry,
                                                   "__invoke", 8U);
        if (invoke == NULL ||
            (invoke->flags & PC_STATIC) != 0U ||
            !pclass_member_visible(invoke->flags, invoke->owner,
                                   caller->called_scope)) {
            pphp_runtime_error(state, caller->line, "object is not callable");
            return 0;
        }
        return enter_method(state, caller, invoke, argument_count, 0);
    }
    if (callable.type != PT_CLOSURE) {
        pphp_runtime_error(state, caller->line, "value is not callable");
        return 0;
    }
    closure = (pclosure *)callable.as.gc;
    callee = closure->proto;
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to closure, %u required, %u given",
                           callee->n_required, argument_count);
        return 0;
    }
    if (state->frame_count >= PPHP_FRAME_MAX ||
        (size_t)callee->n_params + closure->capture_count > callee->n_locals) {
        pphp_runtime_error(state, caller->line, "stack overflow entering closure");
        return 0;
    }
    if (!bind_arguments(state, callee, base, base + 1U, argument_count,
                        0U, caller->line)) {
        return 0;
    }
    for (i = 0U; i < closure->capture_count; i++) {
        state->stack[base + callee->n_params + i] = closure->captures[i];
        pv_retain(closure->captures[i]);
    }
    state->stack_count = base + callee->n_locals;
    memset(&state->frames[state->frame_count], 0,
           sizeof(state->frames[state->frame_count]));
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].module = closure->module;
    state->frames[state->frame_count].pc = 0U;
    state->frames[state->frame_count].base = base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].argument_count = argument_count;
    state->frames[state->frame_count].has_return_override = 0;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = closure->called_scope;
    state->frames[state->frame_count].called_class = closure->called_class;
    pmodule_retain((pmodule *)closure->module);
    pclass_retain_runtime(closure->called_scope);
    pclass_retain_runtime(closure->called_class);
    state->frames[state->frame_count].owns_closure_context = 1U;
    pv_release(callable);
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

static pclass *resolve_class_name(pphp_state *state, pframe *frame,
                                  const pstring *name) {
    if (ps_equal_bytes(name, "self", 4U)) return frame->called_scope;
    if (ps_equal_bytes(name, "parent", 6U)) {
        return frame->called_scope == NULL ? NULL : frame->called_scope->parent;
    }
    if (ps_equal_bytes(name, "static", 6U)) {
        return frame->called_class == NULL ? frame->called_scope
                                           : frame->called_class;
    }
    return pphp_find_class(state, ps_data(name), name->length);
}

static int invoke_named_method(pphp_state *state, pframe *frame,
                               const pstring *name,
                               uint8_t argument_count) {
    size_t base;
    pobject *object;
    const pmethod *method;
    if (name == NULL || state->stack_count < (size_t)argument_count + 1U) {
        pphp_runtime_error(state, frame->line, "invalid method call operands");
        return 0;
    }
    base = state->stack_count - argument_count - 1U;
    if (state->stack[base].type != PT_OBJECT) {
        pphp_runtime_error(state, frame->line, "method call on non-object");
        return 0;
    }
    object = (pobject *)state->stack[base].as.gc;
    if (pphp_object_is_throwable(state, object) && argument_count == 0U) {
        const char *field = NULL;
        size_t field_length = 0U;
        const pvalue *value;
        if (ps_equal_bytes(name, "getMessage", 10U)) {
            field = "message";
            field_length = 7U;
        } else if (ps_equal_bytes(name, "getCode", 7U)) {
            field = "code";
            field_length = 4U;
        } else if (ps_equal_bytes(name, "getFile", 7U)) {
            field = "file";
            field_length = 4U;
        } else if (ps_equal_bytes(name, "getLine", 7U)) {
            field = "line";
            field_length = 4U;
        } else if (ps_equal_bytes(name, "getTraceAsString", 16U)) {
            field = "trace";
            field_length = 5U;
        }
        value = field == NULL ? NULL
                              : pphp_exception_field(object, field, field_length);
        if (value != NULL) {
            pvalue result = *value;
            pv_retain(result);
            release_range(state, base, state->stack_count);
            state->stack_count = base;
            return push(state, result);
        }
    }
    method = pclass_find_method(object->class_entry, ps_data(name), name->length);
    if (method == NULL) {
        pphp_runtime_error(state, frame->line, "undefined method %.*s()",
                           (int)name->length, ps_data(name));
        return 0;
    }
    if (!pclass_member_visible(method->flags, method->owner,
                               frame->called_scope)) {
        pphp_runtime_error(state, frame->line,
                           "cannot access non-public method %.*s()",
                           (int)name->length, ps_data(name));
        return 0;
    }
    return enter_method(state, frame, method, argument_count, 0);
}

static int enter_method(pphp_state *state, pframe *caller, const pmethod *method,
                        uint8_t argument_count, int constructor) {
    const pproto *callee;
    size_t base;
    pvalue object_value;
    if (method == NULL || (method->flags & PC_STATIC) != 0U ||
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
    if (method->native != NULL) {
        pvalue result = pv_null();
        int status = pphp_call_native_method(
            state, method->native, (pobject *)object_value.as.gc,
            state->stack + base + 1U, argument_count, &result);
        if (constructor) {
            release_range(state, base + 1U, state->stack_count);
            state->stack_count = base + 1U;
            pv_release(result);
            return status > 0;
        }
        release_range(state, base, state->stack_count);
        state->stack_count = base;
        return status > 0 && push(state, result);
    }
    callee = method->proto;
    if (callee == NULL) {
        pphp_runtime_error(state, caller->line, "invalid method call");
        return 0;
    }
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to method %.*s(), %u required, %u given",
                           (int)method->name->length, ps_data(method->name),
                           callee->n_required, argument_count);
        return 0;
    }
    if (state->frame_count >= PPHP_FRAME_MAX) {
        pphp_runtime_error(state, caller->line, "stack overflow entering method");
        return 0;
    }
    if (!bind_arguments(state, callee, base, base + 1U, argument_count,
                        1U, caller->line)) {
        return 0;
    }
    state->stack_count = base + callee->n_locals;
    memset(&state->frames[state->frame_count], 0,
           sizeof(state->frames[state->frame_count]));
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].module = method->module;
    state->frames[state->frame_count].pc = 0U;
    state->frames[state->frame_count].base = base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].argument_count = argument_count;
    state->frames[state->frame_count].has_return_override = constructor;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = method->owner;
    state->frames[state->frame_count].called_class =
        ((pobject *)object_value.as.gc)->class_entry;
    if (constructor) {
        state->frames[state->frame_count].return_override = object_value;
        pv_retain(object_value);
    }
    state->frame_count++;
    return 1;
}

static int enter_static_method(pphp_state *state, pframe *caller,
                               const pmethod *method, pclass *called_class,
                               uint8_t argument_count) {
    const pproto *callee;
    size_t base;
    if (method == NULL ||
        (method->flags & PC_STATIC) == 0U ||
        state->stack_count < argument_count) {
        pphp_runtime_error(state, caller->line, "invalid static method call");
        return 0;
    }
    base = state->stack_count - argument_count;
    if (method->native != NULL) {
        pvalue result = pv_null();
        int status = pphp_call_native_method(
            state, method->native, NULL, state->stack + base,
            argument_count, &result);
        (void)called_class;
        release_range(state, base, state->stack_count);
        state->stack_count = base;
        return status > 0 && push(state, result);
    }
    callee = method->proto;
    if (callee == NULL) {
        pphp_runtime_error(state, caller->line, "invalid static method call");
        return 0;
    }
    if (argument_count < callee->n_required) {
        pphp_runtime_error(state, caller->line,
                           "Too few arguments to static method %.*s()",
                           (int)method->name->length, ps_data(method->name));
        return 0;
    }
    if (state->frame_count >= PPHP_FRAME_MAX) {
        pphp_runtime_error(state, caller->line,
                           "stack overflow entering static method");
        return 0;
    }
    if (!bind_arguments(state, callee, base, base, argument_count, 0U,
                        caller->line)) return 0;
    state->stack_count = base + callee->n_locals;
    memset(&state->frames[state->frame_count], 0,
           sizeof(state->frames[state->frame_count]));
    state->frames[state->frame_count].proto = callee;
    state->frames[state->frame_count].module = method->module;
    state->frames[state->frame_count].base = base;
    state->frames[state->frame_count].line = 1U;
    state->frames[state->frame_count].argument_count = argument_count;
    state->frames[state->frame_count].return_override = pv_null();
    state->frames[state->frame_count].called_scope = method->owner;
    state->frames[state->frame_count].called_class = called_class;
    state->frame_count++;
    return 1;
}

static int invoke_static_method(pphp_state *state, pframe *frame,
                                const pstring *class_name,
                                const pstring *method_name,
                                uint8_t argument_count) {
    pclass *class_entry;
    const pmethod *method;
    if (class_name == NULL || method_name == NULL ||
        state->stack_count < argument_count) {
        pphp_runtime_error(state, frame->line,
                           "invalid static method operands");
        return 0;
    }
    class_entry = resolve_class_name(state, frame, class_name);
    if (class_entry == NULL) {
        pphp_runtime_error(state, frame->line, "Class %.*s not found",
                           (int)class_name->length, ps_data(class_name));
        return 0;
    }
    method = pclass_find_method(class_entry, ps_data(method_name),
                                method_name->length);
    if (method == NULL ||
        !pclass_member_visible(method->flags, method->owner,
                               frame->called_scope)) {
        pphp_runtime_error(state, frame->line, "undefined static method %.*s()",
                           (int)method_name->length, ps_data(method_name));
        return 0;
    }
    if ((method->flags & PC_STATIC) != 0U) {
        return enter_static_method(state, frame, method, class_entry,
                                   argument_count);
    }
    if (frame->proto->is_method && frame->base < state->stack_count &&
        state->stack[frame->base].type == PT_OBJECT) {
        size_t base = state->stack_count - argument_count;
        pvalue object = state->stack[frame->base];
        if (state->stack_count >= PPHP_STACK_SLOTS) {
            pphp_runtime_error(state, frame->line,
                               "stack overflow entering parent method");
            return 0;
        }
        memmove(state->stack + base + 1U, state->stack + base,
                argument_count * sizeof(*state->stack));
        pv_retain(object);
        state->stack[base] = object;
        state->stack_count++;
        return enter_method(state, frame, method, argument_count, 0);
    }
    pphp_runtime_error(state, frame->line,
                       "non-static method cannot be called statically");
    return 0;
}

static int construct_object(pphp_state *state, pframe *frame,
                            const pstring *name, uint8_t argument_count) {
    pclass *class_entry;
    pobject *object;
    const pmethod *constructor;
    size_t base;
    if (name == NULL || state->stack_count < argument_count) return 0;
    class_entry = resolve_class_name(state, frame, name);
    if (class_entry == NULL) {
        pphp_runtime_error(state, frame->line, "Class %.*s not found",
                           (int)name->length, ps_data(name));
        return 0;
    }
    object = pobject_new(state, class_entry);
    if (object == NULL) {
        pphp_runtime_error(state, frame->line, "cannot instantiate class %.*s",
                           (int)name->length, ps_data(name));
        return 0;
    }
    base = state->stack_count - argument_count;
    if (state->stack_count + 1U >= PPHP_STACK_SLOTS) {
        pv_release(pv_heap(PT_OBJECT, &object->header));
        pphp_runtime_error(state, frame->line,
                           "stack overflow constructing object");
        return 0;
    }
    memmove(state->stack + base + 1U, state->stack + base,
            argument_count * sizeof(*state->stack));
    state->stack[base] = pv_heap(PT_OBJECT, &object->header);
    state->stack_count++;
    if (pphp_object_is_throwable(state, object)) {
        pphp_exception_set_location(
            object, state->chunk_name == NULL ? "<source>" : state->chunk_name,
            frame->line);
    }
    constructor = pclass_find_method(class_entry, "__construct", 11U);
    if (constructor != NULL) {
        if (!pclass_member_visible(constructor->flags, constructor->owner,
                                   frame->called_scope)) {
            pphp_runtime_error(state, frame->line,
                               "cannot access non-public constructor");
            return 0;
        }
        return enter_method(state, frame, constructor, argument_count, 1);
    }
    if (pphp_object_is_throwable(state, object)) {
        int valid = 1;
        if (argument_count > 2U) {
            pphp_runtime_error(state, frame->line,
                               "exception constructor expects at most two arguments");
            return 0;
        }
        if (argument_count >= 1U) {
            const pproperty *property = pclass_find_property(
                class_entry, "message", 7U);
            pstring *message = pv_to_string(state->stack[base + 1U]);
            if (property == NULL || message == NULL) {
                ps_destroy(message);
                pphp_runtime_error(state, frame->line,
                                   "invalid exception message");
                valid = 0;
            } else {
                pv_release(object->slots[property->slot]);
                object->slots[property->slot] =
                    pv_heap(PT_STRING, &message->header);
            }
        }
        if (valid && argument_count == 2U) {
            if (state->stack[base + 2U].type != PT_INT) {
                pphp_runtime_error(state, frame->line,
                                   "exception code expects int");
                valid = 0;
            } else {
                pphp_exception_set_code(object,
                                        state->stack[base + 2U].as.i);
            }
        }
        if (valid) {
            release_range(state, base + 1U, state->stack_count);
            state->stack_count = base + 1U;
            return 1;
        }
        return 0;
    }
    if (argument_count != 0U) {
        pphp_runtime_error(state, frame->line,
                           "Class %.*s has no constructor",
                           (int)name->length, ps_data(name));
        return 0;
    }
    return 1;
}

static int return_from_function(pphp_state *state, pvalue result) {
    pframe *frame = &state->frames[state->frame_count - 1U];
    size_t base = frame->base;
#if PPHP_TYPECHECK
    if (!type_spec_accepts(state, frame, &frame->proto->return_type, result)) {
        char expected[128];
        type_spec_name(&frame->proto->return_type, expected, sizeof(expected));
        raise_declared_type_error(
            state, frame->line,
            "%.*s(): Return value must be of type %s, %s returned",
            (int)frame->proto->name->length, ps_data(frame->proto->name),
            expected, pv_type_name((pvalue_type)result.type));
        pv_release(result);
        return 0;
    }
#endif
    if (frame->has_return_override) {
        pv_release(result);
        result = frame->return_override;
        frame->return_override = pv_null();
        frame->has_return_override = 0;
    }
    release_range(state, base, state->stack_count);
    state->stack_count = base;
    release_frame_owners(frame);
    state->frame_count--;
    if (state->frame_count == 0U) {
        pv_release(result);
        return 1;
    }
    return push(state, result);
}

static int validate_class_signatures(pphp_state *state, uint32_t line,
                                     int reject_pending) {
    size_t i;
    for (i = 0U; i < state->class_count; i++) {
        const pmethod *missing = NULL;
        int complete = pclass_is_complete(state, state->classes[i], &missing);
        if (complete != 0 && (!reject_pending || complete == 1)) continue;
        pphp_runtime_error(
            state, line, "class %.*s must implement method %.*s()",
            (int)state->classes[i]->name->length,
            ps_data(state->classes[i]->name),
            missing == NULL ? 0 : (int)missing->name->length,
            missing == NULL ? "" : ps_data(missing->name));
        return 0;
    }
    return 1;
}

int pphp_vm_execute(pphp_state *state, const pmodule *module) {
    size_t i;
    if (module == NULL || module->count == 0U) {
        pphp_runtime_error(state, 0U, "module has no entry point");
        return PPHP_E_RUNTIME;
    }
    if (pphp_find_class(state, "Throwable", 9U) == NULL &&
        !pphp_register_exception_classes(state)) {
        pphp_runtime_error(state, 0U, "cannot initialize exception classes");
        return PPHP_E_RUNTIME;
    }
    state->module = module;
    state->stack_count = module->protos[0]->n_locals;
    state->frame_count = 1U;
    memset(&state->frames[0], 0, sizeof(state->frames[0]));
    state->frames[0].proto = module->protos[0];
    state->frames[0].module = module;
    state->frames[0].pc = 0U;
    state->frames[0].base = 0U;
    state->frames[0].line = 1U;
    state->frames[0].argument_count = 0U;
    state->frames[0].all_globals = 1U;
    state->frames[0].has_return_override = 0;
    state->frames[0].return_override = pv_null();
    state->frames[0].called_scope = NULL;
    state->frames[0].called_class = NULL;
    for (i = 0U; i < state->stack_count; i++) {
        state->stack[i] = undefined_local_value();
    }
    while (state->frame_count != 0U && state->error[0] == '\0' &&
           !state->exit_requested) {
        pframe *frame = &state->frames[state->frame_count - 1U];
        size_t instruction_pc = frame->pc;
        int exception_processed = 0;
        uint8_t opcode = read_u8(state, frame);
        trace_instruction(state, frame, instruction_pc, opcode);
        pphp_gc_maybe_collect(state);
        if (state->processed_ticks != state->ticks) {
            state->processed_ticks = state->ticks;
            if (hal_interrupt_requested()) {
                pphp_runtime_error(state, frame->line,
                                   "execution interrupted");
                continue;
            }
            pphp_poll_pgems(state);
            if (state->error[0] != '\0') continue;
        }
        switch ((pphp_opcode)opcode) {
            case OP_NOP: break;
            case OP_HALT:
                if (state->frame_count != 1U) {
                    pphp_runtime_error(state, frame->line, "HALT inside function");
                    break;
                }
                if (state->capture_halt_result && state->halt_result != NULL &&
                    state->stack_count > frame->base + frame->proto->n_locals) {
                    *state->halt_result = state->stack[state->stack_count - 1U];
                    pv_retain(*state->halt_result);
                }
                release_range(state, 0U, state->stack_count);
                state->stack_count = 0U;
                release_all_frames(state);
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
                    (void)load_local_value(state, frame, slot, 0);
                }
                break;
            }
            case OP_LOAD_LOCAL_QUIET: {
                uint8_t slot = read_u8(state, frame);
                if (slot >= frame->proto->n_locals) {
                    pphp_runtime_error(state, frame->line,
                                       "local index out of range");
                } else {
                    (void)load_local_value(state, frame, slot, 1);
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
                    (void)store_local_value(state, frame, slot, value);
                }
                break;
            }
            case OP_UNSET_LOCAL: {
                uint8_t slot = read_u8(state, frame);
                if (slot >= frame->proto->n_locals) {
                    pphp_runtime_error(state, frame->line,
                                       "local index out of range");
                } else {
                    (void)unset_local_value(state, frame, slot);
                }
                break;
            }
            case OP_LOAD_ARGC:
                (void)push(state, pv_int((pphp_int)frame->argument_count));
                break;
            case OP_BIND_GLOBAL: {
                uint8_t slot = read_u8(state, frame);
                if (slot >= frame->proto->n_locals) {
                    pphp_runtime_error(state, frame->line,
                                       "global binding slot is invalid");
                } else {
                    slot_mask_clear(frame->static_mask, slot);
                    slot_mask_set(frame->global_mask, slot);
                    pv_release(state->stack[frame->base + slot]);
                    state->stack[frame->base + slot] = pv_null();
                }
                break;
            }
            case OP_STATIC_INIT: {
                uint8_t slot = read_u8(state, frame);
                int16_t relative = read_i16(state, frame);
                pstring *key;
                pvalue existing = pv_null();
                if (slot >= frame->proto->n_locals) {
                    pphp_runtime_error(state, frame->line,
                                       "static binding slot is invalid");
                    break;
                }
                slot_mask_clear(frame->global_mask, slot);
                slot_mask_set(frame->static_mask, slot);
                pv_release(state->stack[frame->base + slot]);
                state->stack[frame->base + slot] = pv_null();
                key = static_slot_key(state, frame, slot);
                if (key == NULL) {
                    pphp_runtime_error(state, frame->line,
                                       "out of memory binding static variable");
                } else if (pa_get(state->statics,
                                  pv_heap(PT_STRING, &key->header), &existing)) {
                    pv_release(existing);
                    (void)jump_relative(state, frame, relative);
                }
                break;
            }
            case OP_LOAD_NAMED_CONST: {
                uint16_t index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, index);
                pvalue value = pv_null();
                if (name == NULL) break;
                if (!pa_get(state->constants,
                            pv_heap(PT_STRING, (pheader *)&name->header), &value)) {
                    pphp_runtime_error(state, frame->line,
                                       "undefined constant %.*s",
                                       (int)name->length, ps_data(name));
                } else {
                    (void)push(state, value);
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
                pphp_numeric numeric;
                int converted = pv_to_numeric(value, value.type != PT_STRING,
                                              &numeric);
                if (value.type == PT_STRING &&
                    numeric.string_status != PPHP_NUMERIC_STRING_EXACT) {
                    pphp_warning(state, frame->line,
                                 "A non-numeric value encountered");
                }
                if (!converted) {
                    pphp_runtime_error(
                        state, frame->line, "%s",
                        numeric.is_integer < 0
                            ? "integer overflow requires float support"
                            : "unsupported operand type for unary -");
                } else {
#if PPHP_ENABLE_FLOAT
                    pphp_int negated;
                    if (numeric.integer_exact &&
                        pphp_integer_negate(numeric.integer, &negated)) {
                        (void)push(state, pv_int(negated));
                    } else if (numeric.integer_exact) {
                        (void)push(
                            state,
                            pv_float(-(pphp_float)numeric.integer));
                    } else {
                        (void)push(state, pv_float(-numeric.number));
                    }
#else
                    pphp_int negated;
                    if (!pphp_integer_negate(numeric.integer, &negated)) {
                        pphp_runtime_error(
                            state, frame->line,
                            "integer overflow requires float support");
                    } else {
                        (void)push(state, pv_int(negated));
                    }
#endif
                }
                pv_release(value);
                break;
            }
            case OP_BNOT: {
                pvalue value = pop(state);
                pphp_numeric numeric;
                int converted = pv_to_numeric(value, value.type != PT_STRING,
                                              &numeric);
                if (value.type == PT_STRING &&
                    numeric.string_status != PPHP_NUMERIC_STRING_EXACT) {
                    pphp_warning(state, frame->line,
                                 "A non-numeric value encountered");
                }
                if (!converted) {
                    pphp_runtime_error(
                        state, frame->line, "%s",
                        numeric.is_integer < 0
                            ? "integer overflow requires float support"
                            : "unsupported operand type for ~");
                } else {
                    pphp_int integer_value;
                    if (!pphp_numeric_to_integer(&numeric, 0,
                                                 &integer_value)) {
                        pphp_runtime_error(
                            state, frame->line,
                            "integer conversion is out of range");
                        pv_release(value);
                        break;
                    }
                    (void)push(state, pv_int(~integer_value));
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
            case OP_CAST:
                (void)execute_cast(state, read_u8(state, frame));
                break;
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
            case OP_JMP_IFNULL_KEEP: {
                int16_t relative = read_i16(state, frame);
                if (state->stack_count == 0U) {
                    pphp_runtime_error(state, frame->line,
                                       "conditional jump on empty stack");
                } else if (state->stack[state->stack_count - 1U].type ==
                           PT_NULL) {
                    (void)jump_relative(state, frame, relative);
                }
                break;
            }
            case OP_CALL: {
                uint16_t name = read_u16(state, frame);
                uint8_t count = read_u8(state, frame);
                (void)call_function(state, frame, name, count);
                break;
            }
            case OP_CALL_VALUE: {
                uint8_t count = read_u8(state, frame);
                (void)call_value(state, frame, count);
                break;
            }
            case OP_CALL_ARRAY: {
                uint16_t name = read_u16(state, frame);
                uint8_t count;
                if (expand_argument_array(state, frame->line, &count)) {
                    (void)call_function(state, frame, name, count);
                }
                break;
            }
            case OP_CALL_VALUE_ARRAY: {
                uint8_t count;
                if (expand_argument_array(state, frame->line, &count)) {
                    (void)call_value(state, frame, count);
                }
                break;
            }
            case OP_INCLUDE: {
                uint8_t mode = read_u8(state, frame);
                pvalue path_value = pop(state);
                pstring *path = pv_to_string(path_value);
                pvalue include_result = pv_bool(0);
                pv_release(path_value);
                if (path == NULL) {
                    pphp_runtime_error(state, frame->line,
                                       "include path must be string-compatible");
                } else {
                    (void)pphp_exec_include(state, ps_data(path), mode,
                                            &include_result);
                    ps_destroy(path);
                    if (state->error[0] == '\0') {
                        (void)push(state, include_result);
                    } else {
                        pv_release(include_result);
                    }
                }
                break;
            }
#if PPHP_TYPECHECK
            case OP_TYPECHECK_ARGS:
                (void)check_argument_types(state, frame);
                break;
#endif
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
            case OP_ARR_EXTEND: {
                pvalue source_value = pop(state);
                pvalue target_value = pop(state);
                size_t position = 0U;
                int ok = 1;
                if (source_value.type != PT_ARRAY || target_value.type != PT_ARRAY) {
                    pphp_runtime_error(state, frame->line,
                                       "only arrays can be unpacked into arrays");
                    ok = 0;
                }
                while (ok && position < ((const parray *)source_value.as.gc)->used) {
                    pvalue key;
                    pvalue value;
                    size_t next;
                    if (!pa_entry_at((const parray *)source_value.as.gc, position,
                                     &key, &value, &next)) {
                        break;
                    }
                    ok = key.type == PT_INT
                             ? pa_push((parray *)target_value.as.gc, value)
                             : pa_set((parray *)target_value.as.gc, key, value);
                    pv_release(key);
                    pv_release(value);
                    position = next;
                }
                pv_release(source_value);
                if (!ok) {
                    pv_release(target_value);
                    if (state->error[0] == '\0') {
                        pphp_runtime_error(state, frame->line,
                                           "array unpacking failed");
                    }
                } else {
                    (void)push(state, target_value);
                }
                break;
            }
            case OP_ARR_SEPARATE: {
                pvalue array_value = pop(state);
                parray *original;
                parray *writable;
                if (array_value.type != PT_ARRAY) {
                    pv_release(array_value);
                    pphp_runtime_error(state, frame->line,
                                       "array mutation expects an array");
                    break;
                }
                original = (parray *)array_value.as.gc;
                writable = array_for_write(array_value);
                if (writable == NULL) {
                    pv_release(array_value);
                    pphp_runtime_error(state, frame->line,
                                       "out of memory separating array");
                } else if (writable != original) {
                    pv_release(array_value);
                    (void)push(state, pv_heap(PT_ARRAY, &writable->header));
                } else {
                    (void)push(state, array_value);
                }
                break;
            }
            case OP_IDX_GET:
                (void)execute_index_get(state, 0);
                break;
            case OP_IDX_GET_QUIET:
                (void)execute_index_get(state, 1);
                break;
            case OP_IDX_SET:
                (void)execute_array_set(state, 0);
                break;
            case OP_IDX_APPEND:
                (void)execute_array_set(state, 1);
                break;
            case OP_IDX_UNSET:
                (void)execute_array_unset(state);
                break;
            case OP_FE_INIT: {
                pvalue iterable = pop(state);
                parray_iterator *iterator;
                parray *iteration_array = NULL;
                if (iterable.type == PT_ARRAY) {
                    iteration_array = (parray *)iterable.as.gc;
                } else if (iterable.type == PT_OBJECT) {
                    pobject *object = (pobject *)iterable.as.gc;
                    size_t property_index;
                    iteration_array = pa_new(
                        object->class_entry->property_count);
                    if (iteration_array != NULL) {
                        for (property_index = 0U;
                             property_index <
                                 object->class_entry->property_count;
                             property_index++) {
                            const pproperty *property =
                                &object->class_entry
                                     ->properties[property_index];
                            pvalue key;
                            if ((property->flags &
                                 (PC_STATIC | PC_PRIVATE | PC_PROTECTED)) !=
                                0U) {
                                continue;
                            }
                            key = pv_heap(PT_STRING,
                                          &property->name->header);
                            if (!pa_set(iteration_array, key,
                                        object->slots[property->slot])) {
                                pv_release(pv_heap(
                                    PT_ARRAY,
                                    &iteration_array->header));
                                iteration_array = NULL;
                                break;
                            }
                        }
                    }
                } else {
                    pv_release(iterable);
                    pphp_runtime_error(
                        state, frame->line,
                        "foreach argument must be array or object");
                    break;
                }
                iterator = pa_iterator_new(iteration_array);
                if (iterable.type == PT_OBJECT && iteration_array != NULL) {
                    pv_release(pv_heap(PT_ARRAY,
                                       &iteration_array->header));
                }
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
            case OP_DEF_FUNC: {
                uint16_t proto_index = read_u16(state, frame);
                const pproto *proto =
                    frame->module != NULL && proto_index < frame->module->count
                        ? frame->module->protos[proto_index] : NULL;
                if (proto == NULL || !proto->conditional) {
                    pphp_runtime_error(state, frame->line,
                                       "invalid conditional function definition");
                } else if (pphp_builtin_exists(proto->name) ||
                           pphp_native_function_exists(state, proto->name) ||
                           pphp_find_function(state, proto->name, NULL) != NULL) {
                    pphp_runtime_error(state, frame->line,
                                       "function %.*s is already defined",
                                       (int)proto->name->length,
                                       ps_data(proto->name));
                } else if (!pphp_register_runtime_function(
                               state, proto, frame->module)) {
                    pphp_runtime_error(state, frame->line,
                                       "cannot register function %.*s",
                                       (int)proto->name->length,
                                       ps_data(proto->name));
                }
                break;
            }
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
                        (parent = pphp_find_class(state, ps_data(parent_name),
                                                  parent_name->length)) == NULL) {
                        pphp_runtime_error(state, frame->line, "parent class is not defined");
                        break;
                    }
                    if ((parent->flags & PC_FINAL) != 0U) {
                        pphp_runtime_error(state, frame->line, "cannot extend final class");
                        break;
                    }
                    if (((parent->flags & PC_READONLY) != 0U) !=
                        ((flags & PC_READONLY) != 0U)) {
                        pphp_runtime_error(
                            state, frame->line,
                            "readonly classes may only extend readonly classes");
                        break;
                    }
                }
                state->building_class = pclass_new(ps_data(name), name->length, parent, flags);
                if (state->building_class == NULL) {
                    pphp_runtime_error(state, frame->line, "cannot create class definition");
                }
                break;
            }
            case OP_DEF_PROP: {
                uint16_t name_index = read_u16(state, frame);
                uint8_t flags = read_u8(state, frame);
#if PPHP_TYPECHECK
                uint16_t type_index = read_u16(state, frame);
                uint8_t has_default = read_u8(state, frame);
                const pstring *encoded_type = type_index == UINT16_MAX
                                                  ? NULL
                                                  : constant_string(
                                                        state, frame,
                                                        type_index);
                ptype_spec type;
#endif
                const pstring *name = constant_string(state, frame, name_index);
                pvalue default_value = pop(state);
#if PPHP_TYPECHECK
                int added = 0;
                memset(&type, 0, sizeof(type));
                if (state->error[0] == '\0' &&
                    !parse_property_type(encoded_type, &type)) {
                    pphp_runtime_error(state, frame->line,
                                       "cannot decode property type");
                } else if (state->error[0] == '\0' &&
                           state->building_class != NULL && name != NULL) {
                    pproperty temporary;
                    memset(&temporary, 0, sizeof(temporary));
                    temporary.name = (pstring *)name;
                    temporary.owner = state->building_class;
                    temporary.type = type;
                    if (has_default != 0U &&
                        !check_property_type(state, state->building_class,
                                             &temporary, default_value,
                                             frame->line)) {
                        added = 0;
                    } else if ((flags & PC_STATIC) != 0U) {
                        added = pclass_add_typed_static_property(
                            state->building_class, ps_data(name), name->length,
                            flags, default_value, &type, has_default);
                    } else {
                        added = pclass_add_typed_property(
                            state->building_class, ps_data(name), name->length,
                            flags, default_value, &type, has_default);
                    }
                }
                ptype_spec_destroy(&type);
                if (state->error[0] == '\0' && !added) {
                    pphp_runtime_error(state, frame->line,
                                       "cannot define property");
                }
                if (state->error[0] != '\0' &&
                    state->building_class != NULL) {
                    pclass_destroy(state->building_class);
                    state->building_class = NULL;
                }
#else
                if (state->building_class == NULL || name == NULL ||
                    ((flags & PC_STATIC) != 0U
                         ? !pclass_add_static_property(state->building_class,
                                                      ps_data(name), name->length,
                                                      flags,
                                                      default_value)
                         : !pclass_add_property(state->building_class,
                                                ps_data(name), name->length,
                                                flags, default_value))) {
                    pphp_runtime_error(state, frame->line, "cannot define property");
                }
#endif
                pv_release(default_value);
                break;
            }
            case OP_DEF_INTERFACE: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pclass *interface_entry = name == NULL
                                              ? NULL
                                              : pphp_find_class(
                                                    state, ps_data(name),
                                                    name->length);
                if (state->building_class == NULL ||
                    interface_entry == NULL ||
                    !pclass_add_interface(state->building_class,
                                          interface_entry)) {
                    pphp_runtime_error(state, frame->line,
                                       "cannot implement interface %.*s",
                                       name == NULL ? 0 : (int)name->length,
                                       name == NULL ? "" : ps_data(name));
                }
                break;
            }
            case OP_DEF_CCONST: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue value = pop(state);
                if (state->building_class == NULL || name == NULL ||
                    !pclass_add_constant(state->building_class, ps_data(name),
                                         name->length, value)) {
                    pphp_runtime_error(state, frame->line,
                                       "cannot define class constant");
                }
                pv_release(value);
                break;
            }
            case OP_DEF_METHOD: {
                uint16_t name_index = read_u16(state, frame);
                uint16_t proto_index = read_u16(state, frame);
                uint8_t flags = read_u8(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                if (state->building_class == NULL || name == NULL ||
                    frame->module == NULL || proto_index >= frame->module->count ||
                    !pclass_add_method(state->building_class, ps_data(name), name->length,
                                       flags, frame->module->protos[proto_index],
                                       frame->module)) {
                    pphp_runtime_error(state, frame->line, "cannot define method");
                }
                break;
            }
            case OP_DEF_CONST: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue value = pop(state);
                pvalue existing = pv_null();
                if (name == NULL) {
                    pv_release(value);
                    break;
                }
                if (pa_get(state->constants,
                           pv_heap(PT_STRING, (pheader *)&name->header),
                           &existing)) {
                    pv_release(existing);
                    pv_release(value);
                    pphp_runtime_error(state, frame->line,
                                       "constant %.*s is already defined",
                                       (int)name->length, ps_data(name));
                } else if (!pa_set(state->constants,
                                   pv_heap(PT_STRING, (pheader *)&name->header),
                                   value)) {
                    pv_release(value);
                    pphp_runtime_error(state, frame->line,
                                       "out of memory defining constant");
                } else {
                    pv_release(value);
                }
                break;
            }
            case OP_DEF_END: {
                const pmethod *missing = NULL;
                int complete = state->building_class == NULL ? 0 :
                    pclass_is_complete(state, state->building_class,
                                       &missing);
                if (state->building_class != NULL && complete == 0) {
                    pphp_runtime_error(
                        state, frame->line,
                        "class %.*s must implement method %.*s()",
                        (int)state->building_class->name->length,
                        ps_data(state->building_class->name),
                        missing == NULL ? 0 : (int)missing->name->length,
                        missing == NULL ? "" : ps_data(missing->name));
                } else if (state->building_class == NULL ||
                           !pphp_register_class(state,
                                                state->building_class)) {
                    pphp_runtime_error(state, frame->line, "cannot register class");
                } else {
                    state->building_class = NULL;
                    (void)validate_class_signatures(state, frame->line, 0);
                }
                break;
            }
            case OP_NEW_OBJ:
            case OP_NEW_OBJ_ARRAY: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame,
                                                       name_index);
                uint8_t argument_count;
                if (opcode == OP_NEW_OBJ_ARRAY) {
                    if (!expand_argument_array(state, frame->line,
                                               &argument_count)) {
                        break;
                    }
                } else {
                    argument_count = read_u8(state, frame);
                }
                (void)construct_object(state, frame, name, argument_count);
                break;
            }
            case OP_NEW_OBJ_DYNAMIC:
            case OP_NEW_OBJ_DYNAMIC_ARRAY: {
                uint8_t argument_count;
                size_t class_position;
                pvalue class_value;
                const pstring *name;
                if (opcode == OP_NEW_OBJ_DYNAMIC_ARRAY) {
                    if (!expand_argument_array(state, frame->line,
                                               &argument_count)) {
                        break;
                    }
                } else {
                    argument_count = read_u8(state, frame);
                }
                if (state->stack_count < (size_t)argument_count + 1U) {
                    pphp_runtime_error(state, frame->line,
                                       "invalid dynamic class operands");
                    break;
                }
                class_position = state->stack_count - argument_count - 1U;
                class_value = state->stack[class_position];
                if (class_value.type != PT_STRING) {
                    pphp_runtime_error(state, frame->line,
                                       "dynamic class name must be a string");
                    break;
                }
                name = (const pstring *)class_value.as.gc;
                memmove(state->stack + class_position,
                        state->stack + class_position + 1U,
                        (size_t)argument_count * sizeof(*state->stack));
                state->stack_count--;
                (void)construct_object(state, frame, name, argument_count);
                pv_release(class_value);
                break;
            }
            case OP_PROP_GET_QUIET: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame,
                                                       name_index);
                pvalue object_value = pop(state);
                pvalue value = pv_null();
                if (name != NULL && object_value.type == PT_OBJECT) {
                    pobject *object = (pobject *)object_value.as.gc;
                    const pproperty *property = pclass_find_property(
                        object->class_entry, ps_data(name), name->length);
                    if (property != NULL &&
                        pclass_member_visible(property->flags,
                                              property->owner,
                                              frame->called_scope)) {
                        value = object->slots[property->slot];
                        pv_retain(value);
                    }
                }
                pv_release(object_value);
                (void)push(state, value);
                break;
            }
            case OP_PROP_GET_DYNAMIC_QUIET: {
                pvalue name_value = pop(state);
                pvalue object_value = pop(state);
                pvalue value = pv_null();
                if (name_value.type == PT_STRING &&
                    object_value.type == PT_OBJECT) {
                    const pstring *name = (const pstring *)name_value.as.gc;
                    pobject *object = (pobject *)object_value.as.gc;
                    const pproperty *property = pclass_find_property(
                        object->class_entry, ps_data(name), name->length);
                    if (property != NULL &&
                        pclass_member_visible(property->flags,
                                              property->owner,
                                              frame->called_scope)) {
                        value = object->slots[property->slot];
                        pv_retain(value);
                    }
                }
                pv_release(name_value);
                pv_release(object_value);
                (void)push(state, value);
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
                        object->class_entry, ps_data(name), name->length);
                    if (property == NULL) {
                        pvalue argument = pv_heap(
                            PT_STRING, (pheader *)&name->header);
                        pvalue value = pv_null();
                        int invoked = invoke_magic(state, object, "__get", 5U,
                                                   &argument, 1U, &value);
                        if (invoked > 0) {
                            (void)push(state, value);
                        } else if (invoked == 0) {
                            pphp_runtime_error(state, frame->line,
                                               "undefined property %.*s",
                                               (int)name->length, ps_data(name));
                        }
                    } else if (!pclass_member_visible(property->flags, property->owner,
                                                      frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public property %.*s",
                                           (int)name->length, ps_data(name));
#if PPHP_TYPECHECK
                    } else if (property->type.count != 0U &&
                               !property->initialized &&
                               !pobject_property_written(object,
                                                         property->slot)) {
                        pphp_runtime_error(
                            state, frame->line,
                            "Typed property %.*s::$%.*s must not be accessed before initialization",
                            property->owner == NULL
                                ? 0 : (int)property->owner->name->length,
                            property->owner == NULL
                                ? "" : ps_data(property->owner->name),
                            (int)property->name->length,
                            ps_data(property->name));
#endif
                    } else {
                        pvalue value = object->slots[property->slot];
                        pv_retain(value);
                        (void)push(state, value);
                    }
                }
                pv_release(object_value);
                break;
            }
            case OP_PROP_GET_DYNAMIC: {
                pvalue name_value = pop(state);
                pvalue object_value = pop(state);
                const pstring *name = name_value.type == PT_STRING
                                          ? (const pstring *)name_value.as.gc
                                          : NULL;
                if (name == NULL || object_value.type != PT_OBJECT) {
                    pphp_runtime_error(state, frame->line,
                                       name == NULL
                                           ? "dynamic property name must be a string"
                                           : "property access on non-object");
                } else {
                    pobject *object = (pobject *)object_value.as.gc;
                    const pproperty *property = pclass_find_property(
                        object->class_entry, ps_data(name), name->length);
                    if (property == NULL) {
                        pvalue argument = name_value;
                        pvalue value = pv_null();
                        int invoked = invoke_magic(state, object, "__get", 5U,
                                                   &argument, 1U, &value);
                        if (invoked > 0) {
                            (void)push(state, value);
                        } else if (invoked == 0) {
                            pphp_runtime_error(state, frame->line,
                                               "undefined property %.*s",
                                               (int)name->length, ps_data(name));
                        }
                    } else if (!pclass_member_visible(
                                   property->flags, property->owner,
                                   frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public property %.*s",
                                           (int)name->length, ps_data(name));
#if PPHP_TYPECHECK
                    } else if (property->type.count != 0U &&
                               !property->initialized &&
                               !pobject_property_written(object,
                                                         property->slot)) {
                        pphp_runtime_error(
                            state, frame->line,
                            "Typed property %.*s::$%.*s must not be accessed before initialization",
                            property->owner == NULL
                                ? 0 : (int)property->owner->name->length,
                            property->owner == NULL
                                ? "" : ps_data(property->owner->name),
                            (int)property->name->length,
                            ps_data(property->name));
#endif
                    } else {
                        pvalue value = object->slots[property->slot];
                        pv_retain(value);
                        (void)push(state, value);
                    }
                }
                pv_release(name_value);
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
                        object->class_entry, ps_data(name), name->length);
                    if (property == NULL) {
                        pvalue arguments[2];
                        pvalue ignored = pv_null();
                        int invoked;
                        arguments[0] = pv_heap(
                            PT_STRING, (pheader *)&name->header);
                        arguments[1] = value;
                        invoked = invoke_magic(state, object, "__set", 5U,
                                               arguments, 2U, &ignored);
                        pv_release(ignored);
                        if (invoked > 0) {
                            (void)push(state, value);
                            value = pv_null();
                        } else if (invoked == 0) {
                            pphp_runtime_error(state, frame->line,
                                               "undefined property %.*s",
                                               (int)name->length, ps_data(name));
                        }
                    } else if (!pclass_member_visible(property->flags, property->owner,
                                                      frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public property %.*s",
                                           (int)name->length, ps_data(name));
                    } else if ((property->flags & PC_READONLY) != 0U &&
                               (frame->called_scope != property->owner ||
                                pobject_property_written(object, property->slot))) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot modify readonly property %.*s",
                                           (int)name->length, ps_data(name));
#if PPHP_TYPECHECK
                    } else if (!check_property_type(
                                   state, object->class_entry, property, value,
                                   frame->line)) {
#endif
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
            case OP_PROP_SET_DYNAMIC: {
                pvalue value = pop(state);
                pvalue name_value = pop(state);
                pvalue object_value = pop(state);
                const pstring *name = name_value.type == PT_STRING
                                          ? (const pstring *)name_value.as.gc
                                          : NULL;
                if (name == NULL || object_value.type != PT_OBJECT) {
                    pphp_runtime_error(state, frame->line,
                                       name == NULL
                                           ? "dynamic property name must be a string"
                                           : "property assignment on non-object");
                } else {
                    pobject *object = (pobject *)object_value.as.gc;
                    const pproperty *property = pclass_find_property(
                        object->class_entry, ps_data(name), name->length);
                    if (property == NULL) {
                        pvalue arguments[2];
                        pvalue ignored = pv_null();
                        int invoked;
                        arguments[0] = name_value;
                        arguments[1] = value;
                        invoked = invoke_magic(state, object, "__set", 5U,
                                               arguments, 2U, &ignored);
                        pv_release(ignored);
                        if (invoked > 0) {
                            (void)push(state, value);
                            value = pv_null();
                        } else if (invoked == 0) {
                            pphp_runtime_error(state, frame->line,
                                               "undefined property %.*s",
                                               (int)name->length, ps_data(name));
                        }
                    } else if (!pclass_member_visible(
                                   property->flags, property->owner,
                                   frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public property %.*s",
                                           (int)name->length, ps_data(name));
                    } else if ((property->flags & PC_READONLY) != 0U &&
                               (frame->called_scope != property->owner ||
                                pobject_property_written(object,
                                                         property->slot))) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot modify readonly property %.*s",
                                           (int)name->length, ps_data(name));
#if PPHP_TYPECHECK
                    } else if (!check_property_type(
                                   state, object->class_entry, property, value,
                                   frame->line)) {
#endif
                    } else {
                        pv_retain(value);
                        pv_release(object->slots[property->slot]);
                        object->slots[property->slot] = value;
                        pobject_mark_property_written(object, property->slot);
                        (void)push(state, value);
                        value = pv_null();
                    }
                }
                pv_release(value);
                pv_release(name_value);
                pv_release(object_value);
                break;
            }
            case OP_MCALL:
            case OP_MCALL_ARRAY: {
                uint16_t name_index = read_u16(state, frame);
                uint8_t argument_count;
                const pstring *name = constant_string(state, frame, name_index);
                if (opcode == OP_MCALL_ARRAY) {
                    if (!expand_argument_array(state, frame->line,
                                               &argument_count)) {
                        break;
                    }
                } else {
                    argument_count = read_u8(state, frame);
                }
                (void)invoke_named_method(state, frame, name, argument_count);
                break;
            }
            case OP_MCALL_DYNAMIC:
            case OP_MCALL_DYNAMIC_ARRAY: {
                uint8_t argument_count;
                size_t name_position;
                pvalue name_value;
                const pstring *name;
                if (opcode == OP_MCALL_DYNAMIC_ARRAY) {
                    if (!expand_argument_array(state, frame->line,
                                               &argument_count)) break;
                } else {
                    argument_count = read_u8(state, frame);
                }
                if (state->stack_count < (size_t)argument_count + 2U) {
                    pphp_runtime_error(state, frame->line,
                                       "invalid dynamic method operands");
                    break;
                }
                name_position = state->stack_count - argument_count - 1U;
                name_value = state->stack[name_position];
                if (name_value.type != PT_STRING) {
                    pphp_runtime_error(state, frame->line,
                                       "dynamic method name must be a string");
                    break;
                }
                name = (const pstring *)name_value.as.gc;
                memmove(state->stack + name_position,
                        state->stack + name_position + 1U,
                        (size_t)argument_count * sizeof(*state->stack));
                state->stack_count--;
                (void)invoke_named_method(state, frame, name, argument_count);
                pv_release(name_value);
                break;
            }
            case OP_SCALL:
            case OP_SCALL_ARRAY: {
                uint16_t class_index = read_u16(state, frame);
                uint16_t method_index = read_u16(state, frame);
                const pstring *class_name = constant_string(state, frame,
                                                            class_index);
                const pstring *method_name = constant_string(state, frame,
                                                             method_index);
                uint8_t argument_count;
                if (opcode == OP_SCALL_ARRAY) {
                    if (!expand_argument_array(state, frame->line,
                                               &argument_count)) break;
                } else {
                    argument_count = read_u8(state, frame);
                }
                (void)invoke_static_method(state, frame, class_name,
                                           method_name, argument_count);
                break;
            }
            case OP_SPROP_GET:
            case OP_SPROP_SET:
            case OP_CLSCONST: {
                uint16_t class_index = read_u16(state, frame);
                uint16_t member_index = read_u16(state, frame);
                const pstring *class_name = constant_string(state, frame,
                                                            class_index);
                const pstring *member_name = constant_string(state, frame,
                                                             member_index);
                pclass *class_entry = class_name == NULL
                                          ? NULL
                                          : resolve_class_name(state, frame,
                                                               class_name);
                if (class_entry == NULL || member_name == NULL) {
                    pphp_runtime_error(state, frame->line,
                                       "invalid static member access");
                    break;
                }
                if (opcode == OP_SPROP_GET) {
                    pvalue value = pv_null();
                    const pproperty *property = pclass_find_static_property(
                        class_entry, ps_data(member_name), member_name->length);
                    if (property == NULL) {
                        pphp_runtime_error(state, frame->line,
                                           "undefined static property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
                    } else if (!pclass_member_visible(
                                   property->flags, property->owner,
                                   frame->called_scope)) {
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public static "
                                           "property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
#if PPHP_TYPECHECK
                    } else if (property->type.count != 0U &&
                               !property->initialized) {
                        pphp_runtime_error(
                            state, frame->line,
                            "Typed static property %.*s::$%.*s must not be accessed before initialization",
                            property->owner == NULL
                                ? 0 : (int)property->owner->name->length,
                            property->owner == NULL
                                ? "" : ps_data(property->owner->name),
                            (int)property->name->length,
                            ps_data(property->name));
#endif
                    } else if (!pclass_get_static_property(
                                   class_entry, ps_data(member_name),
                                   member_name->length, &value)) {
                        pphp_runtime_error(state, frame->line,
                                           "undefined static property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
                    } else {
                        (void)push(state, value);
                    }
                } else if (opcode == OP_SPROP_SET) {
                    pvalue value = pop(state);
                    const pproperty *property = pclass_find_static_property(
                        class_entry, ps_data(member_name), member_name->length);
                    if (property == NULL) {
                        pv_release(value);
                        pphp_runtime_error(state, frame->line,
                                           "undefined static property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
                    } else if (!pclass_member_visible(
                                   property->flags, property->owner,
                                   frame->called_scope)) {
                        pv_release(value);
                        pphp_runtime_error(state, frame->line,
                                           "cannot access non-public static "
                                           "property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
                    } else if ((property->flags & PC_READONLY) != 0U) {
                        pv_release(value);
                        pphp_runtime_error(state, frame->line,
                                           "cannot modify readonly static property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
#if PPHP_TYPECHECK
                    } else if (!check_property_type(
                                   state, class_entry, property, value,
                                   frame->line)) {
                        pv_release(value);
#endif
                    } else if (!pclass_set_static_property(
                                   class_entry, ps_data(member_name),
                                   member_name->length, value)) {
                        pv_release(value);
                        pphp_runtime_error(state, frame->line,
                                           "undefined static property %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
                    } else {
                        (void)push(state, value);
                    }
                } else if (ps_equal_bytes(member_name, "class", 5U)) {
                    pstring *name_copy = ps_new(ps_data(class_entry->name),
                                                class_entry->name->length);
                    if (name_copy == NULL) {
                        pphp_runtime_error(state, frame->line,
                                           "out of memory reading class name");
                    } else {
                        (void)push(state,
                            pv_heap(PT_STRING, &name_copy->header));
                    }
                } else {
                    pvalue value = pv_null();
                    if (!pclass_get_constant(class_entry, ps_data(member_name),
                                             member_name->length, &value)) {
                        pphp_runtime_error(state, frame->line,
                                           "undefined class constant %.*s",
                                           (int)member_name->length,
                                           ps_data(member_name));
                    } else {
                        (void)push(state, value);
                    }
                }
                break;
            }
            case OP_INSTANCEOF: {
                uint16_t name_index = read_u16(state, frame);
                const pstring *name = constant_string(state, frame, name_index);
                pvalue value = pop(state);
                int matches = 0;
                if (name != NULL && value.type == PT_OBJECT) {
                    pclass *expected = resolve_class_name(state, frame, name);
                    matches = expected != NULL &&
                              pclass_is_a(((pobject *)value.as.gc)->class_entry, expected);
                }
                pv_release(value);
                (void)push(state, pv_bool(matches));
                break;
            }
            case OP_INSTANCEOF_DYNAMIC: {
                pvalue class_value = pop(state);
                pvalue value = pop(state);
                int matches = 0;
                if (class_value.type == PT_STRING && value.type == PT_OBJECT) {
                    const pstring *name =
                        (const pstring *)class_value.as.gc;
                    pclass *expected = pphp_find_class(
                        state, ps_data(name), name->length);
                    matches = expected != NULL &&
                              pclass_is_a(
                                  ((pobject *)value.as.gc)->class_entry,
                                  expected);
                }
                pv_release(class_value);
                pv_release(value);
                (void)push(state, pv_bool(matches));
                break;
            }
            case OP_CLOSURE: {
                uint16_t proto_index = read_u16(state, frame);
                uint8_t capture_count = read_u8(state, frame);
                pvalue captures[UINT8_MAX];
                size_t capture;
                int valid = frame->module != NULL &&
                            proto_index < frame->module->count;
                pclosure *closure;
                for (capture = 0U; capture < capture_count; capture++) {
                    uint8_t kind = read_u8(state, frame);
                    uint8_t slot = read_u8(state, frame);
                    if (kind != 0U || slot >= frame->proto->n_locals) {
                        valid = 0;
                        captures[capture] = pv_null();
                    } else if (!read_local_value(state, frame, slot, 0,
                                                 &captures[capture])) {
                        valid = 0;
                    }
                }
                if (!valid) {
                    while (capture > 0U) {
                        capture--;
                        pv_release(captures[capture]);
                    }
                    pphp_runtime_error(state, frame->line,
                                       "invalid CLOSURE instruction");
                    break;
                }
                closure = pclosure_new(frame->module->protos[proto_index],
                                       frame->module, frame->called_scope,
                                       frame->called_class, captures,
                                       capture_count);
                for (capture = 0U; capture < capture_count; capture++) {
                    pv_release(captures[capture]);
                }
                if (closure == NULL) {
                    pphp_runtime_error(state, frame->line,
                                       "out of memory creating closure");
                } else {
                    (void)push(state, pv_heap(PT_CLOSURE, &closure->header));
                }
                break;
            }
            case OP_THROW: {
                pvalue exception = pop(state);
                if (exception.type != PT_OBJECT ||
                    !pphp_object_is_throwable(state, (pobject *)exception.as.gc)) {
                    pv_release(exception);
                    pphp_runtime_error(state, frame->line,
                                       "Can only throw objects implementing Throwable");
                } else {
                    (void)throw_exception(state, exception, instruction_pc);
                    exception_processed = 1;
                }
                break;
            }
            case OP_CLONE: {
                pvalue source = pop(state);
                pobject *copy;
                const pmethod *clone_method;
                if (source.type != PT_OBJECT) {
                    pv_release(source);
                    pphp_runtime_error(state, frame->line,
                                       "__clone method called on non-object");
                    break;
                }
                copy = pobject_clone((const pobject *)source.as.gc);
                pv_release(source);
                if (copy == NULL) {
                    pphp_runtime_error(state, frame->line,
                                       "out of memory cloning object");
                    break;
                }
                (void)push(state, pv_heap(PT_OBJECT, &copy->header));
                clone_method = pclass_find_method(copy->class_entry,
                                                  "__clone", 7U);
                if (clone_method != NULL) {
                    (void)enter_method(state, frame, clone_method, 0U, 1);
                }
                break;
            }
            case OP_LINE:
                frame->line = read_u16(state, frame);
                break;
            default:
                pphp_runtime_error(state, frame->line, "unknown opcode 0x%02x", opcode);
                break;
        }
        if (state->error[0] != '\0' && !exception_processed) {
            convert_runtime_error(state, instruction_pc);
        }
    }
    if (state->error[0] == '\0' && !state->capture_halt_result) {
        (void)validate_class_signatures(state, 0U, 1);
    }
    if (state->error[0] != '\0') {
        if (state->building_class != NULL) {
            pclass_destroy(state->building_class);
            state->building_class = NULL;
        }
        release_range(state, 0U, state->stack_count);
        state->stack_count = 0U;
        release_all_frames(state);
        return PPHP_E_RUNTIME;
    }
    if (state->exit_requested) {
        release_range(state, 0U, state->stack_count);
        state->stack_count = 0U;
        release_all_frames(state);
    }
    return PPHP_OK;
}

int pphp_vm_invoke(pphp_state *state, pvalue callable,
                   const pvalue *arguments, size_t count, pvalue *result) {
    pphp_state child;
    pmodule module;
    pproto *entry;
    pmodule **modules = NULL;
    size_t module_count;
    size_t i;
    uint16_t constant;
    int status = PPHP_E_RUNTIME;
    if (state == NULL || result == NULL || count > 31U) return 0;
    *result = pv_null();
    if (!pmodule_init(&module)) return 0;
    entry = pproto_new("{callback}", 10U);
    if (entry == NULL || !pmodule_add(&module, entry)) {
        pproto_destroy(entry);
        return 0;
    }
    if (!pproto_add_constant(entry, callable, &constant) ||
        !pproto_emit_u8(entry, OP_LOAD_CONST) ||
        !pproto_emit_u16(entry, constant)) goto done;
    for (i = 0U; i < count; i++) {
        if (!pproto_add_constant(entry, arguments[i], &constant) ||
            !pproto_emit_u8(entry, OP_LOAD_CONST) ||
            !pproto_emit_u16(entry, constant)) goto done;
    }
    if (!pproto_emit_u8(entry, OP_CALL_VALUE) ||
        !pproto_emit_u8(entry, (uint8_t)count) ||
        !pproto_emit_u8(entry, OP_HALT)) goto done;
    entry->max_stack = (uint16_t)(count + 2U);
    module_count = state->repl_module_count +
                   (state->module == NULL ? 0U : 1U);
    if (module_count != 0U) {
        modules = pphp_alloc(module_count * sizeof(*modules));
        if (modules == NULL) goto done;
        for (i = 0U; i < state->repl_module_count; i++) {
            modules[i] = state->repl_modules[i];
        }
        if (state->module != NULL) {
            modules[module_count - 1U] = (pmodule *)state->module;
        }
    }
    child = *state;
    child.stack_count = 0U;
    child.frame_count = 0U;
    child.module = NULL;
    child.root_state = state->root_state == NULL ? state : state->root_state;
    child.repl_modules = modules;
    child.repl_module_count = module_count;
    child.repl_module_capacity = module_count;
    child.repl_mode = 1;
    child.capture_halt_result = 1;
    child.halt_result = result;
    child.exit_requested = 0;
    child.exit_status = 0;
    child.error[0] = '\0';
    child.error_line = 0U;
    status = pphp_vm_execute(&child, &module);
    if (status != PPHP_OK) {
        (void)snprintf(state->error, sizeof(state->error), "%s", child.error);
        state->error_line = child.error_line;
        pv_release(*result);
        *result = pv_null();
    }
    if (child.exit_requested) {
        state->exit_requested = 1;
        state->exit_status = child.exit_status;
    }
    state->random_state = child.random_state;
    if (state->root_state != NULL) {
        state->root_state->classes = child.classes;
        state->root_state->class_count = child.class_count;
        state->root_state->class_capacity = child.class_capacity;
        state->root_state->random_state = child.random_state;
    }
done:
    pphp_free(modules);
    pmodule_destroy(&module);
    return status == PPHP_OK;
}
