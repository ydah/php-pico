#include "builtins.h"

#include "value_ops.h"

#include <stdio.h>
#include <string.h>

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static void output_integer(pphp_state *state, pphp_int value) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    if (length > 0) {
        pphp_output(state, buffer, (size_t)length);
    }
}

static void dump_value(pphp_state *state, pvalue value) {
    switch ((pvalue_type)value.type) {
        case PT_NULL:
            pphp_output(state, "NULL\n", 5U);
            break;
        case PT_FALSE:
            pphp_output(state, "bool(false)\n", 12U);
            break;
        case PT_TRUE:
            pphp_output(state, "bool(true)\n", 11U);
            break;
        case PT_INT:
            pphp_output(state, "int(", 4U);
            output_integer(state, value.as.i);
            pphp_output(state, ")\n", 2U);
            break;
        case PT_FLOAT: {
            char buffer[64];
            int length = snprintf(buffer, sizeof(buffer), "float(%.14g)\n",
                                  (double)value.as.f);
            if (length > 0) {
                pphp_output(state, buffer, (size_t)length);
            }
            break;
        }
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            pphp_output(state, "string(", 7U);
            output_integer(state, (pphp_int)string->length);
            pphp_output(state, ") \"", 3U);
            pphp_output(state, string->data, string->length);
            pphp_output(state, "\"\n", 2U);
            break;
        }
        default:
            pphp_output(state, pv_type_name((pvalue_type)value.type),
                        strlen(pv_type_name((pvalue_type)value.type)));
            pphp_output(state, "\n", 1U);
            break;
    }
}

int pphp_call_builtin(pphp_state *state, const pstring *name,
                      const pvalue *arguments, size_t count, pvalue *result) {
    if (name_is(name, "strlen")) {
        pstring *string;
        if (count != 1U) {
            pphp_runtime_error(state, 0U, "strlen() expects exactly 1 argument, %zu given", count);
            return -1;
        }
        string = pv_to_string(arguments[0]);
        if (string == NULL) {
            pphp_runtime_error(state, 0U, "strlen(): argument must be string-compatible");
            return -1;
        }
        *result = pv_int((pphp_int)string->length);
        ps_destroy(string);
        return 1;
    }
    if (name_is(name, "var_dump")) {
        size_t i;
        for (i = 0U; i < count; i++) {
            dump_value(state, arguments[i]);
        }
        *result = pv_null();
        return 1;
    }
    if (name_is(name, "gettype")) {
        const char *type;
        pstring *string;
        if (count != 1U) {
            pphp_runtime_error(state, 0U, "gettype() expects exactly 1 argument, %zu given", count);
            return -1;
        }
        type = pv_type_name((pvalue_type)arguments[0].type);
        string = ps_new(type, strlen(type));
        if (string == NULL) {
            pphp_runtime_error(state, 0U, "out of memory in gettype()");
            return -1;
        }
        *result = pv_heap(PT_STRING, &string->header);
        return 1;
    }
    if (name_is(name, "memory_get_usage")) {
        *result = pv_int((pphp_int)pphp_pool_get_stats().used);
        return 1;
    }
    if (name_is(name, "abs")) {
        pphp_float number;
        int integer;
        if (count != 1U || !pv_to_number(arguments[0], &number, &integer)) {
            pphp_runtime_error(state, 0U, "abs() expects one numeric argument");
            return -1;
        }
        if (number < 0) {
            number = -number;
        }
        *result = integer ? pv_int((pphp_int)number) : pv_float(number);
        return 1;
    }
    return 0;
}

