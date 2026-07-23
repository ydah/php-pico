#include "state.h"

#include "pclass.h"
#include "value_ops.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int name_equal_ci(const pstring *name, const char *other) {
    size_t length = strlen(other);
    size_t i;
    if (name->length != length) return 0;
    for (i = 0U; i < length; i++) {
        unsigned char left = (unsigned char)ps_data(name)[i];
        unsigned char right = (unsigned char)other[i];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + 32U);
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + 32U);
        if (left != right) return 0;
    }
    return 1;
}

static pnative_function *find_native(pphp_state *state,
                                     const pstring *name) {
    size_t i;
    if (state == NULL || name == NULL) return NULL;
    for (i = 0U; i < state->native_function_count; i++) {
        if (name_equal_ci(name, ps_data(state->native_functions[i].name))) {
            return &state->native_functions[i];
        }
    }
    return NULL;
}

void pphp_def_func(pphp_state *state, const char *name, pphp_cfunc function,
                   int minimum_arguments, int maximum_arguments) {
    pnative_function *resized;
    pnative_function *entry;
    size_t capacity;
    pstring *name_string;
    if (state == NULL || name == NULL || function == NULL ||
        minimum_arguments < 0 ||
        (maximum_arguments >= 0 && maximum_arguments < minimum_arguments)) {
        return;
    }
    name_string = ps_new(name, strlen(name));
    if (name_string == NULL || find_native(state, name_string) != NULL) {
        ps_destroy(name_string);
        return;
    }
    if (state->native_function_count == state->native_function_capacity) {
        capacity = state->native_function_capacity == 0U
                       ? 8U : state->native_function_capacity * 2U;
        resized = pphp_realloc(state->native_functions,
                               capacity * sizeof(*resized));
        if (resized == NULL) {
            ps_destroy(name_string);
            return;
        }
        state->native_functions = resized;
        state->native_function_capacity = capacity;
    }
    entry = &state->native_functions[state->native_function_count++];
    entry->name = name_string;
    entry->function = function;
    entry->minimum_arguments = minimum_arguments;
    entry->maximum_arguments = maximum_arguments;
}

pclass *pphp_def_class(pphp_state *state, const char *name,
                       const char *parent_name) {
    pclass *parent = NULL;
    pclass *class_entry;
    if (state == NULL || name == NULL) return NULL;
    if (parent_name != NULL) {
        parent = pphp_find_class(state, parent_name, strlen(parent_name));
        if (parent == NULL) return NULL;
    }
    class_entry = pclass_new(name, strlen(name), parent, 0U);
    if (class_entry == NULL || !pphp_register_class(state, class_entry)) {
        pclass_destroy(class_entry);
        return NULL;
    }
    class_entry->persistent = 1U;
    return class_entry;
}

void pphp_def_method(pclass *class_entry, const char *name,
                     pphp_cfunc function, uint8_t flags) {
    if (class_entry == NULL || name == NULL || function == NULL) return;
    if ((flags & (PPHP_PUBLIC | PPHP_PROTECTED | PPHP_PRIVATE)) == 0U) {
        flags |= PPHP_PUBLIC;
    }
    (void)pclass_add_native_method(class_entry, name, strlen(name), flags,
                                   function);
}

void pphp_def_cconst_int(pclass *class_entry, const char *name,
                         pphp_int value) {
    if (class_entry != NULL && name != NULL) {
        (void)pclass_add_constant(class_entry, name, strlen(name),
                                  pv_int(value));
    }
}

static int invoke_context(pphp_state *state, pphp_cfunc function,
                          pobject *this_object, const pvalue *arguments,
                          size_t count, pvalue *result) {
    pphp_ctx context;
    size_t i;
    int status;
    if (state == NULL || function == NULL || result == NULL || count > 31U) {
        return -1;
    }
    memset(&context, 0, sizeof(context));
    context.state = state;
    context.this_object = this_object;
    context.arguments = arguments;
    context.argument_count = count;
    context.result = pv_null();
    status = function(&context);
    for (i = 0U; i < context.temporary_count; i++) {
        ps_destroy(context.temporaries[i]);
    }
    if (status < 0 || context.failed || state->error[0] != '\0') {
        pv_release(context.result);
        *result = pv_null();
        return -1;
    }
    *result = context.result;
    return 1;
}

int pphp_native_function_exists(const pphp_state *state,
                                const pstring *name) {
    return find_native((pphp_state *)state, name) != NULL;
}

int pphp_call_native_function(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result) {
    pnative_function *entry = find_native(state, name);
    if (entry == NULL) return 0;
    if (count < (size_t)entry->minimum_arguments ||
        (entry->maximum_arguments >= 0 &&
         count > (size_t)entry->maximum_arguments)) {
        pphp_runtime_error(state, 0U,
                           "%.*s() expects between %d and %d arguments",
                           (int)name->length, ps_data(name),
                           entry->minimum_arguments,
                           entry->maximum_arguments);
        return -1;
    }
    return invoke_context(state, entry->function, NULL, arguments, count,
                          result);
}

int pphp_call_native_method(pphp_state *state, pphp_cfunc function,
                            pobject *this_object, const pvalue *arguments,
                            size_t count, pvalue *result) {
    return invoke_context(state, function, this_object, arguments, count,
                          result);
}

int pphp_argc(pphp_ctx *context) {
    return context == NULL ? 0 : (int)context->argument_count;
}

pvalue pphp_arg(pphp_ctx *context, int index) {
    if (context == NULL || index < 0 ||
        (size_t)index >= context->argument_count) return pv_null();
    return context->arguments[index];
}

pphp_int pphp_arg_int(pphp_ctx *context, int index) {
    pphp_numeric numeric;
    pphp_int converted = 0;
    if (context == NULL || index < 0 ||
        (size_t)index >= context->argument_count ||
        !pv_to_numeric(context->arguments[index], 1, &numeric) ||
        !pphp_numeric_to_integer(&numeric, 0, &converted)) {
        if (context != NULL) {
            (void)pphp_raise(context, "TypeError",
                             "argument %d must be integer-compatible",
                             index + 1);
        }
        return 0;
    }
    return converted;
}

const char *pphp_arg_str(pphp_ctx *context, int index, size_t *length) {
    pstring *string;
    if (length != NULL) *length = 0U;
    if (context == NULL || index < 0 ||
        (size_t)index >= context->argument_count ||
        context->temporary_count >= 31U) {
        if (context != NULL) {
            (void)pphp_raise(context, "TypeError",
                             "argument %d must be string-compatible",
                             index + 1);
        }
        return NULL;
    }
    string = pv_to_string(context->arguments[index]);
    if (string == NULL) {
        (void)pphp_raise(context, "TypeError",
                         "argument %d must be string-compatible", index + 1);
        return NULL;
    }
    context->temporaries[context->temporary_count++] = string;
    if (length != NULL) *length = string->length;
    return ps_data(string);
}

pobject *pphp_this(pphp_ctx *context) {
    return context == NULL ? NULL : context->this_object;
}

static void set_result(pphp_ctx *context, pvalue value) {
    if (context == NULL) {
        pv_release(value);
        return;
    }
    pv_release(context->result);
    context->result = value;
}

void pphp_ret_null(pphp_ctx *context) { set_result(context, pv_null()); }
void pphp_ret_int(pphp_ctx *context, pphp_int value) {
    set_result(context, pv_int(value));
}
#if PPHP_ENABLE_FLOAT
void pphp_ret_float(pphp_ctx *context, pphp_float value) {
    set_result(context, pv_float(value));
}
#endif
void pphp_ret_bool(pphp_ctx *context, int value) {
    set_result(context, pv_bool(value));
}
void pphp_ret_strn(pphp_ctx *context, const char *bytes, size_t length) {
    pstring *string;
    if (context == NULL || bytes == NULL || length > PPHP_STR_MAX) return;
    string = ps_new(bytes, length);
    if (string == NULL) {
        (void)pphp_raise(context, "OutOfMemoryError",
                         "out of memory returning native string");
        return;
    }
    set_result(context, pv_heap(PT_STRING, &string->header));
}
void pphp_ret_value(pphp_ctx *context, pvalue value) {
    set_result(context, value);
}
void pphp_ret_object(pphp_ctx *context, pobject *object) {
    set_result(context, object == NULL
                            ? pv_null()
                            : pv_heap(PT_OBJECT, &object->header));
}

int pphp_raise(pphp_ctx *context, const char *class_name,
               const char *format, ...) {
    va_list arguments;
    if (context == NULL || context->state == NULL || format == NULL) return -1;
    va_start(arguments, format);
    (void)vsnprintf(context->state->error, sizeof(context->state->error),
                    format, arguments);
    va_end(arguments);
    (void)snprintf(context->state->raised_class,
                   sizeof(context->state->raised_class), "%s",
                   class_name == NULL ? "Error" : class_name);
    context->failed = 1;
    return -1;
}

pobject *pphp_obj_new_with(pphp_ctx *context, pclass *class_entry,
                           size_t extra_bytes, void (*finalizer)(void *)) {
    pobject *object;
    if (context == NULL || context->state == NULL || class_entry == NULL) {
        return NULL;
    }
    object = pobject_new(context->state, class_entry);
    if (object == NULL) return NULL;
    if (extra_bytes != 0U) {
        object->native_data = pphp_alloc(extra_bytes);
        if (object->native_data == NULL) {
            pobject_destroy(object);
            (void)pphp_raise(context, "OutOfMemoryError",
                             "out of memory allocating native object data");
            return NULL;
        }
        memset(object->native_data, 0, extra_bytes);
    }
    object->native_finalizer = finalizer;
    return object;
}

void *pphp_obj_data(pobject *object) {
    return object == NULL ? NULL : object->native_data;
}

const void *pphp_obj_const_data(const pobject *object) {
    return object == NULL ? NULL : object->native_data;
}

#if PPHP_RC_DEBUG
void pphp_obj_set_rc_visitor(pobject *object,
                             pphp_native_rc_visit_fn visitor) {
    if (object != NULL) object->native_rc_visitor = visitor;
}
#endif
