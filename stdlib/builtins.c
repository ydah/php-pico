#include "builtins.h"

#if PPHP_ENABLE_FLOAT
#include "float_format.h"
#include "float_math.h"
#endif
#include "value_ops.h"
#include "parray.h"
#include "pclass.h"
#include "strings.h"
#include "arrays.h"
#include "formatting.h"
#include "json.h"
#include "system.h"
#include "files.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

int pphp_builtin_exists(const pstring *name) {
    static const char names[] =
        "abs\0acos\0array_combine\0array_fill\0array_fill_keys\0array_flip\0"
        "array_filter\0array_is_list\0array_key_exists\0array_keys\0array_map\0array_merge\0array_pop\0array_product\0array_push\0array_reduce\0"
        "array_reverse\0array_search\0array_shift\0array_slice\0array_sum\0array_unshift\0array_values\0arsort\0asort\0"
        "asin\0atan\0atan2\0bin2hex\0bindec\0boolval\0ceil\0chr\0class_exists\0"
        "constant\0cos\0count\0date\0decbin\0dechex\0decoct\0define\0defined\0die\0error_log\0exit\0exp\0"
        "explode\0fdiv\0floatval\0floor\0fmod\0function_exists\0get_class\0"
        "gc_collect_cycles\0gettype\0hex2bin\0hexdec\0hrtime\0implode\0in_array\0intdiv\0intval\0is_array\0"
        "is_bool\0is_callable\0is_float\0is_int\0is_null\0is_numeric\0is_object\0"
        "is_string\0join\0json_decode\0json_encode\0key_exists\0lcfirst\0log\0log10\0ltrim\0max\0"
        "krsort\0ksort\0memory_get_usage\0method_exists\0microtime\0min\0mt_rand\0octdec\0ord\0pi\0pow\0print_r\0printf\0rand\0random_int\0range\0rsort\0"
        "round\0rtrim\0sin\0sqrt\0str_contains\0str_ends_with\0str_pad\0"
        "str_repeat\0str_replace\0str_split\0str_starts_with\0strcasecmp\0strcmp\0"
        "strlen\0strncmp\0strpos\0strrev\0strrpos\0strtolower\0strtoupper\0"
        "sort\0srand\0strval\0substr\0sprintf\0tan\0time\0trim\0uasort\0ucfirst\0uksort\0usleep\0usort\0var_dump\0sleep\0";
    const char *candidate = names;
#if !PPHP_ENABLE_FLOAT
    static const char float_names[] =
        "acos\0asin\0atan\0atan2\0ceil\0cos\0exp\0fdiv\0floatval\0floor\0"
        "fmod\0is_float\0log\0log10\0pi\0pow\0round\0sin\0sqrt\0tan\0";
    const char *float_candidate = float_names;
    while (*float_candidate != '\0') {
        if (name_is(name, float_candidate)) return 0;
        float_candidate += strlen(float_candidate) + 1U;
    }
#endif
    while (*candidate != '\0') {
        if (name_is(name, candidate)) return 1;
        candidate += strlen(candidate) + 1U;
    }
    return pphp_file_builtin_exists(name);
}

static int call_reflection_builtin(pphp_state *state, const pstring *name,
                                   const pvalue *arguments, size_t count,
                                   pvalue *result) {
    const pstring *subject;
    if (name_is(name, "define")) {
        pvalue existing = pv_null();
        if (count < 2U || count > 3U || arguments[0].type != PT_STRING) {
            pphp_runtime_error(state, 0U,
                               "define() expects a string name and a value");
            return -1;
        }
        subject = (const pstring *)arguments[0].as.gc;
        if (pa_get(state->constants, arguments[0], &existing)) {
            pv_release(existing);
            *result = pv_bool(0);
            return 1;
        }
        if (!pa_set(state->constants, arguments[0], arguments[1])) {
            pphp_runtime_error(state, 0U, "out of memory defining constant");
            return -1;
        }
        (void)subject;
        *result = pv_bool(1);
        return 1;
    }
    if (name_is(name, "defined") || name_is(name, "constant")) {
        pvalue value = pv_null();
        int found;
        if (count != 1U || arguments[0].type != PT_STRING) {
            pphp_runtime_error(state, 0U,
                               "%.*s() expects one string name",
                               (int)name->length, ps_data(name));
            return -1;
        }
        found = pa_get(state->constants, arguments[0], &value);
        if (name_is(name, "defined")) {
            pv_release(value);
            *result = pv_bool(found);
            return 1;
        }
        if (!found) {
            pphp_runtime_error(state, 0U, "undefined constant %.*s",
                               (int)((const pstring *)arguments[0].as.gc)->length,
                               ps_data((const pstring *)arguments[0].as.gc));
            return -1;
        }
        *result = value;
        return 1;
    }
    if (name_is(name, "function_exists")) {
        pstring *function_name;
        if (count != 1U || arguments[0].type != PT_STRING) {
            pphp_runtime_error(state, 0U,
                               "function_exists() expects one string name");
            return -1;
        }
        function_name = (pstring *)arguments[0].as.gc;
        *result = pv_bool(pphp_builtin_exists(function_name) ||
                          pphp_native_function_exists(state, function_name) ||
                          pphp_find_function(state, function_name, NULL) != NULL);
        return 1;
    }
    if (name_is(name, "class_exists")) {
        if (count < 1U || count > 2U || arguments[0].type != PT_STRING) {
            pphp_runtime_error(state, 0U,
                               "class_exists() expects a string class name");
            return -1;
        }
        subject = (const pstring *)arguments[0].as.gc;
        *result = pv_bool(pphp_find_class(state, ps_data(subject),
                                          subject->length) != NULL);
        return 1;
    }
    if (name_is(name, "method_exists")) {
        pclass *class_entry = NULL;
        const pstring *method_name;
        if (count != 2U || arguments[1].type != PT_STRING) {
            pphp_runtime_error(state, 0U,
                               "method_exists() expects object/class and method name");
            return -1;
        }
        if (arguments[0].type == PT_OBJECT) {
            class_entry = ((pobject *)arguments[0].as.gc)->class_entry;
        } else if (arguments[0].type == PT_STRING) {
            subject = (const pstring *)arguments[0].as.gc;
            class_entry = pphp_find_class(state, ps_data(subject), subject->length);
        }
        method_name = (const pstring *)arguments[1].as.gc;
        *result = pv_bool(class_entry != NULL &&
                          pclass_find_method(class_entry, ps_data(method_name),
                                             method_name->length) != NULL);
        return 1;
    }
    if (name_is(name, "get_class")) {
        const pstring *class_name;
        pstring *copy;
        if (count != 1U || arguments[0].type != PT_OBJECT) {
            pphp_runtime_error(state, 0U, "get_class() expects one object");
            return -1;
        }
        class_name = ((pobject *)arguments[0].as.gc)->class_entry->name;
        copy = ps_new(ps_data(class_name), class_name->length);
        if (copy == NULL) {
            pphp_runtime_error(state, 0U, "out of memory returning class name");
            return -1;
        }
        *result = pv_heap(PT_STRING, &copy->header);
        return 1;
    }
    return 0;
}

static void output_integer(pphp_state *state, pphp_int value) {
    char buffer[32];
    int length = pphp_format_integer(buffer, sizeof(buffer), value);
    if (length > 0) {
        pphp_output(state, buffer, (size_t)length);
    }
}

static void dump_value_depth(pphp_state *state, pvalue value, unsigned depth);

static void output_indent(pphp_state *state, unsigned depth) {
    while (depth-- != 0U) pphp_output(state, "  ", 2U);
}

static void dump_value_depth(pphp_state *state, pvalue value, unsigned depth) {
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
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT: {
            char buffer[64];
            int length = pphp_format_float(buffer, sizeof(buffer), value.as.f,
                                           'g', 14);
            if (length >= 0) {
                pphp_output(state, "float(", 6U);
                pphp_output(state, buffer, (size_t)length);
                pphp_output(state, ")\n", 2U);
            }
            break;
        }
#endif
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            pphp_output(state, "string(", 7U);
            output_integer(state, (pphp_int)string->length);
            pphp_output(state, ") \"", 3U);
            pphp_output(state, ps_data(string), string->length);
            pphp_output(state, "\"\n", 2U);
            break;
        }
        case PT_ARRAY: {
            const parray *array = (const parray *)value.as.gc;
            size_t position = 0U;
            pphp_output(state, "array(", 6U);
            output_integer(state, (pphp_int)array->size);
            pphp_output(state, ") {\n", 4U);
            while (position < array->used) {
                pvalue key;
                pvalue item;
                size_t next;
                if (!pa_entry_at(array, position, &key, &item, &next)) break;
                output_indent(state, depth + 1U);
                pphp_output(state, "[", 1U);
                if (key.type == PT_INT) {
                    output_integer(state, key.as.i);
                } else {
                    const pstring *string = (const pstring *)key.as.gc;
                    pphp_output(state, "\"", 1U);
                    pphp_output(state, ps_data(string), string->length);
                    pphp_output(state, "\"", 1U);
                }
                pphp_output(state, "]=>\n", 4U);
                output_indent(state, depth + 1U);
                dump_value_depth(state, item, depth + 1U);
                pv_release(key);
                pv_release(item);
                position = next;
            }
            output_indent(state, depth);
            pphp_output(state, "}\n", 2U);
            break;
        }
        default:
            pphp_output(state, pv_type_name((pvalue_type)value.type),
                        strlen(pv_type_name((pvalue_type)value.type)));
            pphp_output(state, "\n", 1U);
            break;
    }
}

static void dump_value(pphp_state *state, pvalue value) {
    dump_value_depth(state, value, 0U);
}

static int require_count(pphp_state *state, const char *name, size_t count,
                         size_t minimum, size_t maximum) {
    if (count >= minimum && count <= maximum) return 1;
    pphp_runtime_error(state, 0U,
                       "%s() expects %zu to %zu arguments, %zu given",
                       name, minimum, maximum, count);
    return 0;
}

static int integer_from_string(const pstring *string, unsigned base,
                               pphp_int *result) {
    size_t i = 0U;
    int sign = 1;
    uint64_t value = 0U;
    uint64_t limit;
    int digits = 0;
    if (base < 2U || base > 36U) return 0;
    while (i < string->length && (ps_data(string)[i] == ' ' ||
           ps_data(string)[i] == '\t' || ps_data(string)[i] == '\n' ||
           ps_data(string)[i] == '\r')) i++;
    if (i < string->length && (ps_data(string)[i] == '+' || ps_data(string)[i] == '-')) {
        sign = ps_data(string)[i++] == '-' ? -1 : 1;
    }
    limit = (uint64_t)PPHP_INT_MAXIMUM + (sign < 0 ? 1U : 0U);
    if (i + 1U < string->length && ps_data(string)[i] == '0') {
        char prefix = ps_data(string)[i + 1U];
        if ((base == 16U && (prefix == 'x' || prefix == 'X')) ||
            (base == 2U && (prefix == 'b' || prefix == 'B'))) i += 2U;
    }
    while (i < string->length) {
        unsigned digit;
        unsigned char c = (unsigned char)ps_data(string)[i++];
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'z') digit = (unsigned)(c - 'a') + 10U;
        else if (c >= 'A' && c <= 'Z') digit = (unsigned)(c - 'A') + 10U;
        else break;
        if (digit >= base) break;
        if (value > (limit - digit) / base) {
            *result = sign < 0 ? PPHP_INT_MINIMUM : PPHP_INT_MAXIMUM;
            return -1;
        }
        value = value * base + digit;
        digits++;
    }
    if (digits == 0) {
        *result = 0;
    } else if (sign < 0 && value == (uint64_t)PPHP_INT_MAXIMUM + 1U) {
        *result = PPHP_INT_MINIMUM;
    } else if (sign < 0) {
        *result = -(pphp_int)value;
    } else {
        *result = (pphp_int)value;
    }
    return 1;
}

static int call_conversion_builtin(pphp_state *state, const pstring *name,
                                   const pvalue *arguments, size_t count,
                                   pvalue *result) {
    if (name_is(name, "intval")) {
        unsigned base = 10U;
        if (!require_count(state, "intval", count, 1U, 2U)) return -1;
        if (count == 2U) {
            if (arguments[1].type != PT_INT || arguments[1].as.i < 2 ||
                arguments[1].as.i > 36) {
                pphp_runtime_error(state, 0U, "intval() base must be between 2 and 36");
                return -1;
            }
            base = (unsigned)arguments[1].as.i;
        }
        if (arguments[0].type == PT_STRING) {
            pphp_int converted;
            if (integer_from_string((const pstring *)arguments[0].as.gc,
                                    base, &converted) < 0) {
#if PPHP_ENABLE_FLOAT
                *result = pv_int(converted);
                return 1;
#else
                pphp_runtime_error(state, 0U,
                                   "integer overflow requires float support");
                return -1;
#endif
            }
            *result = pv_int(converted);
        } else {
            pphp_numeric numeric;
            pphp_int converted;
            if (pv_to_numeric(arguments[0], 1, &numeric)) {
                if (!pphp_numeric_to_integer(&numeric, 0, &converted)) {
                    pphp_runtime_error(state, 0U,
                                       "integer conversion is out of range");
                    return -1;
                }
                *result = pv_int(converted);
            } else {
                *result = pv_int(
                    arguments[0].type == PT_ARRAY &&
                    pa_count((const parray *)arguments[0].as.gc) != 0U);
            }
        }
        return 1;
    }
    if (name_is(name, "floatval")) {
#if PPHP_ENABLE_FLOAT
        pphp_float number;
        int integer;
        if (!require_count(state, "floatval", count, 1U, 1U)) return -1;
        if (!pv_to_number(arguments[0], &number, &integer)) number = 0;
        *result = pv_float(number);
        return 1;
#else
        (void)arguments;
        (void)result;
        pphp_runtime_error(state, 0U, "float support disabled");
        return -1;
#endif
    }
    if (name_is(name, "strval")) {
        pstring *string;
        if (!require_count(state, "strval", count, 1U, 1U)) return -1;
        string = pv_to_string(arguments[0]);
        if (string == NULL) {
            pphp_runtime_error(state, 0U, "strval() value cannot be converted to string");
            return -1;
        }
        *result = pv_heap(PT_STRING, &string->header);
        return 1;
    }
    if (name_is(name, "boolval")) {
        if (!require_count(state, "boolval", count, 1U, 1U)) return -1;
        *result = pv_bool(pv_is_truthy(arguments[0]));
        return 1;
    }
    return 0;
}

static int call_type_builtin(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    int matches;
    if (!(name_is(name, "is_int") || name_is(name, "is_float") ||
          name_is(name, "is_string") || name_is(name, "is_bool") ||
          name_is(name, "is_array") || name_is(name, "is_object") ||
          name_is(name, "is_null") || name_is(name, "is_numeric") ||
          name_is(name, "is_callable"))) return 0;
    if (!require_count(state, "type predicate", count, 1U, 1U)) return -1;
    if (name_is(name, "is_int")) matches = arguments[0].type == PT_INT;
    else if (name_is(name, "is_float")) {
#if PPHP_ENABLE_FLOAT
        matches = arguments[0].type == PT_FLOAT;
#else
        matches = 0;
#endif
    }
    else if (name_is(name, "is_string")) matches = arguments[0].type == PT_STRING;
    else if (name_is(name, "is_bool")) {
        matches = arguments[0].type == PT_TRUE || arguments[0].type == PT_FALSE;
    } else if (name_is(name, "is_array")) matches = arguments[0].type == PT_ARRAY;
    else if (name_is(name, "is_object")) {
        matches = arguments[0].type == PT_OBJECT || arguments[0].type == PT_CLOSURE;
    } else if (name_is(name, "is_null")) matches = arguments[0].type == PT_NULL;
    else if (name_is(name, "is_numeric")) {
        pphp_float number;
        int integer;
        matches = (arguments[0].type == PT_INT ||
#if PPHP_ENABLE_FLOAT
                   arguments[0].type == PT_FLOAT ||
#endif
                   arguments[0].type == PT_STRING) &&
                  pv_to_number(arguments[0], &number, &integer);
    } else {
        const pclass *scope = state->frame_count == 0U
                                  ? NULL
                                  : state->frames[state->frame_count - 1U]
                                        .called_scope;
        matches = arguments[0].type == PT_CLOSURE;
        if (arguments[0].type == PT_STRING) {
            const pstring *function_name = (const pstring *)arguments[0].as.gc;
            matches = pphp_builtin_exists(function_name) ||
                      pphp_native_function_exists(state, function_name) ||
                      pphp_find_function(state, function_name, NULL) != NULL;
        } else if (arguments[0].type == PT_OBJECT) {
            const pmethod *invoke = pclass_find_method(
                ((pobject *)arguments[0].as.gc)->class_entry,
                "__invoke", 8U);
            matches = invoke != NULL &&
                      pclass_member_visible(invoke->flags, invoke->owner,
                                            scope);
        } else if (arguments[0].type == PT_ARRAY) {
            pvalue target = pv_null();
            pvalue method = pv_null();
            pclass *class_entry = NULL;
            const pmethod *method_entry = NULL;
            matches = pa_get((const parray *)arguments[0].as.gc,
                             pv_int(0), &target) &&
                      pa_get((const parray *)arguments[0].as.gc,
                             pv_int(1), &method) &&
                      method.type == PT_STRING;
            if (matches && target.type == PT_OBJECT) {
                class_entry = ((pobject *)target.as.gc)->class_entry;
            } else if (matches && target.type == PT_STRING) {
                pstring *class_name = (pstring *)target.as.gc;
                class_entry = pphp_find_class(state, ps_data(class_name),
                                               class_name->length);
            }
            if (class_entry != NULL) {
                pstring *method_name = (pstring *)method.as.gc;
                method_entry = pclass_find_method(class_entry,
                                                  ps_data(method_name),
                                                  method_name->length);
            }
            matches = method_entry != NULL &&
                      (target.type == PT_OBJECT ||
                       (method_entry->flags & PC_STATIC) != 0U) &&
                      pclass_member_visible(method_entry->flags,
                                            method_entry->owner, scope);
            pv_release(target);
            pv_release(method);
        }
    }
    *result = pv_bool(matches);
    return 1;
}

static int numeric_value_argument(pphp_state *state, const char *name,
                                  pvalue value, pphp_numeric *numeric) {
    if (pv_to_numeric(value, 1, numeric)) return 1;
    if (numeric->is_integer < 0) {
        pphp_runtime_error(state, 0U,
                           "integer overflow requires float support");
    } else {
        pphp_runtime_error(state, 0U, "%s() expects numeric arguments", name);
    }
    return 0;
}

#if PPHP_ENABLE_FLOAT
static int numeric_argument(pphp_state *state, const char *name,
                            pvalue value, pphp_float *number,
                            int *integer) {
    pphp_numeric numeric;
    if (!numeric_value_argument(state, name, value, &numeric)) return 0;
    *number = numeric.number;
    *integer = numeric.is_integer;
    return 1;
}
#endif

static int call_math_builtin(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
#if PPHP_ENABLE_FLOAT
    pphp_float a;
    pphp_float b;
    int ai;
    int bi;
#endif
#if !PPHP_ENABLE_FLOAT
    if (name_is(name, "pi") || name_is(name, "fdiv") ||
        name_is(name, "fmod") || name_is(name, "pow") ||
        name_is(name, "atan2") || name_is(name, "floor") ||
        name_is(name, "ceil") || name_is(name, "sqrt") ||
        name_is(name, "exp") || name_is(name, "log10") ||
        name_is(name, "sin") || name_is(name, "cos") ||
        name_is(name, "tan") || name_is(name, "asin") ||
        name_is(name, "acos") || name_is(name, "atan") ||
        name_is(name, "log") || name_is(name, "round")) {
        pphp_runtime_error(state, 0U, "float support disabled");
        return -1;
    }
#endif
#if PPHP_ENABLE_FLOAT
    if (name_is(name, "pi")) {
        if (!require_count(state, "pi", count, 0U, 0U)) return -1;
        *result = pv_float((pphp_float)3.14159265358979323846);
        return 1;
    }
    if (name_is(name, "fdiv") ||
        name_is(name, "fmod") || name_is(name, "pow") ||
        name_is(name, "atan2")) {
        if (!require_count(state, "binary math", count, 2U, 2U) ||
            !numeric_argument(state, "binary math", arguments[0], &a, &ai) ||
            !numeric_argument(state, "binary math", arguments[1], &b, &bi)) return -1;
        if (name_is(name, "fdiv")) {
            *result = pv_float(a / b);
        } else if (name_is(name, "fmod")) {
            *result = pv_float(PPHP_FLOAT_MATH(fmod)(a, b));
        } else if (name_is(name, "pow")) {
            *result = pv_float(pphp_float_power(a, b));
        } else {
            *result = pv_float(PPHP_FLOAT_MATH(atan2)(a, b));
        }
        return 1;
    }
    if (name_is(name, "floor") || name_is(name, "ceil") ||
        name_is(name, "sqrt") || name_is(name, "exp") ||
        name_is(name, "log10") || name_is(name, "sin") ||
        name_is(name, "cos") || name_is(name, "tan") ||
        name_is(name, "asin") || name_is(name, "acos") ||
        name_is(name, "atan")) {
        pphp_float value;
        if (!require_count(state, "unary math", count, 1U, 1U) ||
            !numeric_argument(state, "unary math", arguments[0], &a, &ai)) return -1;
        if (name_is(name, "floor")) value = PPHP_FLOAT_MATH(floor)(a);
        else if (name_is(name, "ceil")) value = PPHP_FLOAT_MATH(ceil)(a);
        else if (name_is(name, "sqrt")) value = PPHP_FLOAT_MATH(sqrt)(a);
        else if (name_is(name, "exp")) value = PPHP_FLOAT_MATH(exp)(a);
        else if (name_is(name, "log10")) value = PPHP_FLOAT_MATH(log10)(a);
        else if (name_is(name, "sin")) value = PPHP_FLOAT_MATH(sin)(a);
        else if (name_is(name, "cos")) value = PPHP_FLOAT_MATH(cos)(a);
        else if (name_is(name, "tan")) value = PPHP_FLOAT_MATH(tan)(a);
        else if (name_is(name, "asin")) value = PPHP_FLOAT_MATH(asin)(a);
        else if (name_is(name, "acos")) value = PPHP_FLOAT_MATH(acos)(a);
        else value = PPHP_FLOAT_MATH(atan)(a);
        *result = pv_float(value);
        return 1;
    }
    if (name_is(name, "log")) {
        if (!require_count(state, "log", count, 1U, 2U) ||
            !numeric_argument(state, "log", arguments[0], &a, &ai)) return -1;
        if (count == 2U) {
            if (!numeric_argument(state, "log", arguments[1], &b, &bi)) return -1;
            *result = pv_float(PPHP_FLOAT_MATH(log)(a) /
                               PPHP_FLOAT_MATH(log)(b));
        } else {
            *result = pv_float(PPHP_FLOAT_MATH(log)(a));
        }
        return 1;
    }
    if (name_is(name, "round")) {
        int precision = 0;
        pphp_float scale;
        if (!require_count(state, "round", count, 1U, 2U) ||
            !numeric_argument(state, "round", arguments[0], &a, &ai)) return -1;
        if (count == 2U && arguments[1].type == PT_INT) {
            if (arguments[1].as.i < INT_MIN ||
                arguments[1].as.i > INT_MAX) {
                pphp_runtime_error(state, 0U,
                                   "round() precision is out of range");
                return -1;
            }
            precision = (int)arguments[1].as.i;
        }
        scale = pphp_float_power((pphp_float)10,
                                 (pphp_float)precision);
        *result = pv_float(PPHP_FLOAT_MATH(round)(a * scale) / scale);
        return 1;
    }
#endif
    if (name_is(name, "intdiv")) {
        pphp_numeric left_numeric;
        pphp_numeric right_numeric;
        pphp_int left_integer;
        pphp_int right_integer;
        if (!require_count(state, "intdiv", count, 2U, 2U) ||
            !numeric_value_argument(state, "intdiv", arguments[0],
                                    &left_numeric) ||
            !numeric_value_argument(state, "intdiv", arguments[1],
                                    &right_numeric)) return -1;
        if (!pphp_numeric_to_integer(&left_numeric, 0, &left_integer)) {
            pphp_runtime_error(state, 0U,
                               "integer conversion is out of range");
            return -1;
        }
        if (!pphp_numeric_to_integer(&right_numeric, 0, &right_integer)) {
            pphp_runtime_error(state, 0U,
                               "integer conversion is out of range");
            return -1;
        }
        if (right_integer == 0) {
            pphp_runtime_error(state, 0U, "Division by zero");
            return -1;
        }
        if (pphp_integer_division_overflows(left_integer, right_integer)) {
            pphp_runtime_error(state, 0U,
                               "integer overflow requires float support");
            return -1;
        }
        *result = pv_int(left_integer / right_integer);
        return 1;
    }
    if (name_is(name, "max") || name_is(name, "min")) {
        pvalue best;
        size_t i;
        if (count == 0U) {
            pphp_runtime_error(state, 0U, "max/min expects at least one argument");
            return -1;
        }
        if (count == 1U && arguments[0].type == PT_ARRAY) {
            const parray *array = (const parray *)arguments[0].as.gc;
            size_t position = 0U;
            pvalue key;
            size_t next;
            if (!pa_entry_at(array, 0U, &key, &best, &position)) {
                pphp_runtime_error(state, 0U, "max/min array must not be empty");
                return -1;
            }
            pv_release(key);
            while (pa_entry_at(array, position, &key, result, &next)) {
                int compared = 0;
                const char *error = NULL;
                (void)pv_compare(*result, best, 0, &compared, &error);
                if ((name_is(name, "max") && compared > 0) ||
                    (name_is(name, "min") && compared < 0)) {
                    pv_release(best);
                    best = *result;
                } else {
                    pv_release(*result);
                }
                pv_release(key);
                position = next;
            }
        } else {
            best = arguments[0];
            pv_retain(best);
            for (i = 1U; i < count; i++) {
                int compared = 0;
                const char *error = NULL;
                (void)pv_compare(arguments[i], best, 0, &compared, &error);
                if ((name_is(name, "max") && compared > 0) ||
                    (name_is(name, "min") && compared < 0)) {
                    pv_release(best);
                    best = arguments[i];
                    pv_retain(best);
                }
            }
        }
        *result = best;
        return 1;
    }
    return 0;
}

int pphp_call_builtin(pphp_state *state, const pstring *name,
                      const pvalue *arguments, size_t count, pvalue *result) {
    int handled = call_reflection_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = pphp_call_formatting_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = pphp_call_json_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = pphp_call_system_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = pphp_call_file_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = pphp_call_array_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = pphp_call_string_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = call_conversion_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = call_type_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = call_math_builtin(state, name, arguments, count, result);
    if (handled != 0) return handled;
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
    if (name_is(name, "count")) {
        if (count < 1U || count > 2U || arguments[0].type != PT_ARRAY) {
            pphp_runtime_error(state, 0U, "count() expects an array");
            return -1;
        }
        *result = pv_int((pphp_int)pa_count((const parray *)arguments[0].as.gc));
        return 1;
    }
    if (name_is(name, "array_sum")) {
        const parray *array;
        size_t position = 0U;
        pphp_float sum = 0;
        int all_integer = 1;
#if PPHP_ENABLE_FLOAT
        pphp_int integer_sum = 0;
#endif
        if (count != 1U || arguments[0].type != PT_ARRAY) {
            pphp_runtime_error(state, 0U, "array_sum() expects one array");
            return -1;
        }
        array = (const parray *)arguments[0].as.gc;
        while (position < array->used) {
            pvalue key;
            pvalue item;
            size_t next;
            pphp_numeric numeric;
            if (!pa_entry_at(array, position, &key, &item, &next)) break;
            if (pv_to_numeric(item, 1, &numeric)) {
#if !PPHP_ENABLE_FLOAT
                if (!pphp_integer_add(sum, numeric.integer, &sum)) {
                    pv_release(key);
                    pv_release(item);
                    pphp_runtime_error(
                        state, 0U,
                        "integer overflow requires float support");
                    return -1;
                }
#else
                sum += numeric.number;
                if (all_integer) {
                    pphp_int added;
                    if (!numeric.integer_exact) {
                        all_integer = 0;
                    } else if (!pphp_integer_add(
                                   integer_sum, numeric.integer, &added)) {
                        all_integer = 0;
                    } else {
                        integer_sum = added;
                    }
                }
#endif
#if !PPHP_ENABLE_FLOAT
                all_integer = all_integer && numeric.is_integer;
#endif
            } else if (numeric.is_integer < 0) {
                pv_release(key);
                pv_release(item);
                pphp_runtime_error(state, 0U,
                                   "integer overflow requires float support");
                return -1;
            }
            pv_release(key);
            pv_release(item);
            position = next;
        }
#if PPHP_ENABLE_FLOAT
        *result = all_integer ? pv_int(integer_sum) : pv_float(sum);
#else
        (void)all_integer;
        *result = pv_int(sum);
#endif
        return 1;
    }
    if (name_is(name, "abs")) {
        pphp_numeric numeric = {0};
#if PPHP_ENABLE_FLOAT
        pphp_int integer_value = 0;
#endif
        if (count != 1U || !pv_to_numeric(arguments[0], 1, &numeric)) {
            pphp_runtime_error(
                state, 0U, "%s",
                numeric.is_integer < 0
                    ? "integer overflow requires float support"
                    : "abs() expects one numeric argument");
            return -1;
        }
#if PPHP_ENABLE_FLOAT
        if (numeric.integer_exact) integer_value = numeric.integer;
#endif
        if (numeric.number < 0) {
#if PPHP_ENABLE_FLOAT
            if (numeric.integer_exact) {
                pphp_int negated;
                if (pphp_integer_negate(integer_value, &negated)) {
                    integer_value = negated;
                    numeric.number = (pphp_float)negated;
                } else {
                    numeric.number = -numeric.number;
                    numeric.integer_exact = 0;
                }
            } else {
                numeric.number = -numeric.number;
            }
#else
            if (!pphp_integer_negate(numeric.integer, &numeric.integer)) {
                pphp_runtime_error(state, 0U,
                                   "integer overflow requires float support");
                return -1;
            }
#endif
        }
#if PPHP_ENABLE_FLOAT
        *result = numeric.integer_exact ? pv_int(integer_value)
                                        : pv_float(numeric.number);
#else
        *result = pv_int(numeric.integer);
#endif
        return 1;
    }
    return 0;
}
