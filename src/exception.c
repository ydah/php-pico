#include "pclass.h"

#include "state.h"

#include <stdio.h>
#include <string.h>

static pclass *register_builtin_class(pphp_state *state, const char *name,
                                      pclass *parent, uint8_t flags) {
    pclass *class_entry = pclass_new(name, strlen(name), parent, flags);
    if (class_entry == NULL || !pphp_register_class(state, class_entry)) {
        pclass_destroy(class_entry);
        return NULL;
    }
    return class_entry;
}

static int add_property(pclass *class_entry, const char *name, size_t length,
                        pvalue default_value) {
    int added;
    added = pclass_add_property(class_entry, name, length, PC_PROTECTED,
                                default_value);
    pv_release(default_value);
    return added;
}

static int add_string_property(pclass *class_entry, const char *name,
                               size_t length) {
    pstring *string = ps_new("", 0U);
    return string != NULL &&
           add_property(class_entry, name, length,
                        pv_heap(PT_STRING, &string->header));
}

static int add_exception_properties(pclass *class_entry) {
    return add_string_property(class_entry, "message", 7U) &&
           add_property(class_entry, "code", 4U, pv_int(0)) &&
           add_string_property(class_entry, "file", 4U) &&
           add_property(class_entry, "line", 4U, pv_int(0)) &&
           add_string_property(class_entry, "trace", 5U);
}

int pphp_register_exception_classes(pphp_state *state) {
    pclass *throwable;
    pclass *exception;
    pclass *runtime;
    pclass *error;
    pclass *arithmetic;
    throwable = register_builtin_class(state, "Throwable", NULL, PC_INTERFACE);
    exception = register_builtin_class(state, "Exception", throwable, 0U);
    if (throwable == NULL || exception == NULL ||
        !add_exception_properties(exception)) return 0;
    runtime = register_builtin_class(state, "RuntimeException", exception, 0U);
    if (runtime == NULL ||
        register_builtin_class(state, "InvalidArgumentException", runtime, 0U) == NULL) {
        return 0;
    }
    error = register_builtin_class(state, "Error", throwable, 0U);
    if (error == NULL || !add_exception_properties(error)) return 0;
    if (register_builtin_class(state, "TypeError", error, 0U) == NULL ||
        register_builtin_class(state, "ValueError", error, 0U) == NULL ||
        register_builtin_class(state, "ArgumentCountError", error, 0U) == NULL ||
        register_builtin_class(state, "UnhandledMatchError", error, 0U) == NULL ||
        register_builtin_class(state, "OutOfMemoryError", error, 0U) == NULL) {
        return 0;
    }
    arithmetic = register_builtin_class(state, "ArithmeticError", error, 0U);
    if (arithmetic == NULL ||
        register_builtin_class(state, "DivisionByZeroError", arithmetic, 0U) == NULL) {
        return 0;
    }
    state->oom_exception = pphp_exception_new(state, "OutOfMemoryError",
                                              "Out of memory");
    return state->oom_exception != NULL;
}

pobject *pphp_exception_new(pphp_state *state, const char *class_name,
                            const char *message) {
    pclass *class_entry = pphp_find_class(state, class_name, strlen(class_name));
    pobject *object;
    const pproperty *property;
    pstring *string;
    if (class_entry == NULL) return NULL;
    object = pobject_new(state, class_entry);
    if (object == NULL) return NULL;
    property = pclass_find_property(class_entry, "message", 7U);
    string = ps_new(message == NULL ? "" : message,
                    message == NULL ? 0U : strlen(message));
    if (property == NULL || string == NULL) {
        ps_destroy(string);
        pobject_destroy(object);
        return NULL;
    }
    pv_release(object->slots[property->slot]);
    object->slots[property->slot] = pv_heap(PT_STRING, &string->header);
    pphp_exception_set_location(object,
                                state->chunk_name == NULL ? "<source>" : state->chunk_name,
                                state->error_line);
    return object;
}

int pphp_object_is_throwable(const pphp_state *state, const pobject *object) {
    pclass *throwable = pphp_find_class(state, "Throwable", 9U);
    return object != NULL && throwable != NULL &&
           pclass_is_a(object->class_entry, throwable);
}

const pstring *pphp_exception_message(const pobject *object) {
    const pvalue *value = pphp_exception_field(object, "message", 7U);
    return value != NULL && value->type == PT_STRING
               ? (const pstring *)value->as.gc
               : NULL;
}

const pvalue *pphp_exception_field(const pobject *object, const char *name,
                                   size_t length) {
    const pproperty *property;
    if (object == NULL) return NULL;
    property = pclass_find_property(object->class_entry, name, length);
    return property == NULL ? NULL : &object->slots[property->slot];
}

static void set_field(pobject *object, const char *name, size_t length,
                      pvalue value) {
    const pproperty *property = pclass_find_property(object->class_entry,
                                                     name, length);
    if (property == NULL) {
        pv_release(value);
        return;
    }
    pv_release(object->slots[property->slot]);
    object->slots[property->slot] = value;
}

void pphp_exception_set_location(pobject *object, const char *file,
                                 uint32_t line) {
    pstring *file_string;
    if (object == NULL) return;
    if (file == NULL) file = "<source>";
    file_string = ps_new(file, strlen(file));
    if (file_string != NULL) {
        set_field(object, "file", 4U,
                  pv_heap(PT_STRING, &file_string->header));
    }
    set_field(object, "line", 4U, pv_int((pphp_int)line));
}

void pphp_exception_set_code(pobject *object, pphp_int code) {
    if (object != NULL) set_field(object, "code", 4U, pv_int(code));
}

void pphp_exception_capture_trace(pphp_state *state, pobject *object) {
    const pvalue *existing = pphp_exception_field(object, "trace", 5U);
    char buffer[512];
    size_t length = 0U;
    size_t depth;
    pstring *trace;
    if (state == NULL || object == NULL ||
        (existing != NULL && existing->type == PT_STRING &&
         ((const pstring *)existing->as.gc)->length != 0U)) return;
    for (depth = state->frame_count; depth > 0U && length < sizeof(buffer); depth--) {
        const pframe *frame = &state->frames[depth - 1U];
        int written = snprintf(buffer + length, sizeof(buffer) - length,
                               "#%zu %.*s:%u %.*s()\n",
                               state->frame_count - depth,
                               (int)strlen(state->chunk_name == NULL
                                               ? "<source>" : state->chunk_name),
                               state->chunk_name == NULL ? "<source>" : state->chunk_name,
                               frame->line, (int)frame->proto->name->length,
                               frame->proto->name->data);
        if (written < 0) break;
        if ((size_t)written >= sizeof(buffer) - length) {
            length = sizeof(buffer) - 1U;
            break;
        }
        length += (size_t)written;
    }
    trace = ps_new(buffer, length);
    if (trace != NULL) {
        set_field(object, "trace", 5U, pv_heap(PT_STRING, &trace->header));
    }
}
