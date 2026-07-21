#include "builtins.h"

#include "value_ops.h"
#include "parray.h"
#include "pclass.h"
#include "strings.h"
#include "arrays.h"
#include "formatting.h"
#include "json.h"
#include "system.h"
#include "files.h"

#include <math.h>
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
                               (int)name->length, name->data);
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
                               ((const pstring *)arguments[0].as.gc)->data);
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
        *result = pv_bool(pphp_find_class(state, subject->data,
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
            class_entry = pphp_find_class(state, subject->data, subject->length);
        }
        method_name = (const pstring *)arguments[1].as.gc;
        *result = pv_bool(class_entry != NULL &&
                          pclass_find_method(class_entry, method_name->data,
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
        copy = ps_new(class_name->data, class_name->length);
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
    int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
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
                    pphp_output(state, string->data, string->length);
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
    uint32_t value = 0U;
    int digits = 0;
    if (base < 2U || base > 36U) return 0;
    while (i < string->length && (string->data[i] == ' ' ||
           string->data[i] == '\t' || string->data[i] == '\n' ||
           string->data[i] == '\r')) i++;
    if (i < string->length && (string->data[i] == '+' || string->data[i] == '-')) {
        sign = string->data[i++] == '-' ? -1 : 1;
    }
    if (i + 1U < string->length && string->data[i] == '0') {
        char prefix = string->data[i + 1U];
        if ((base == 16U && (prefix == 'x' || prefix == 'X')) ||
            (base == 2U && (prefix == 'b' || prefix == 'B'))) i += 2U;
    }
    while (i < string->length) {
        unsigned digit;
        unsigned char c = (unsigned char)string->data[i++];
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'z') digit = (unsigned)(c - 'a') + 10U;
        else if (c >= 'A' && c <= 'Z') digit = (unsigned)(c - 'A') + 10U;
        else break;
        if (digit >= base) break;
        value = value * base + digit;
        digits++;
    }
    if (digits == 0) {
        *result = 0;
    } else if (sign < 0) {
        *result = (pphp_int)(-(int64_t)value);
    } else {
        *result = (pphp_int)value;
    }
    return 1;
}

static int call_conversion_builtin(pphp_state *state, const pstring *name,
                                   const pvalue *arguments, size_t count,
                                   pvalue *result) {
    if (name_is(name, "intval")) {
        pphp_float number;
        int integer;
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
            (void)integer_from_string((const pstring *)arguments[0].as.gc,
                                      base, &converted);
            *result = pv_int(converted);
        } else if (pv_to_number(arguments[0], &number, &integer)) {
            *result = pv_int((pphp_int)number);
        } else {
            *result = pv_int(arguments[0].type == PT_ARRAY &&
                             pa_count((const parray *)arguments[0].as.gc) != 0U);
        }
        return 1;
    }
    if (name_is(name, "floatval")) {
        pphp_float number;
        int integer;
        if (!require_count(state, "floatval", count, 1U, 1U)) return -1;
        if (!pv_to_number(arguments[0], &number, &integer)) number = 0;
        *result = pv_float(number);
        return 1;
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
    else if (name_is(name, "is_float")) matches = arguments[0].type == PT_FLOAT;
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
        matches = (arguments[0].type == PT_INT || arguments[0].type == PT_FLOAT ||
                   arguments[0].type == PT_STRING) &&
                  pv_to_number(arguments[0], &number, &integer);
    } else {
        matches = arguments[0].type == PT_CLOSURE ||
                  arguments[0].type == PT_STRING;
        if (arguments[0].type == PT_OBJECT) {
            matches = pclass_find_method(
                ((pobject *)arguments[0].as.gc)->class_entry,
                "__invoke", 8U) != NULL;
        } else if (arguments[0].type == PT_ARRAY) {
            pvalue target = pv_null();
            pvalue method = pv_null();
            matches = pa_get((const parray *)arguments[0].as.gc,
                             pv_int(0), &target) &&
                      pa_get((const parray *)arguments[0].as.gc,
                             pv_int(1), &method) &&
                      target.type == PT_OBJECT && method.type == PT_STRING &&
                      pclass_find_method(
                          ((pobject *)target.as.gc)->class_entry,
                          ((pstring *)method.as.gc)->data,
                          ((pstring *)method.as.gc)->length) != NULL;
            pv_release(target);
            pv_release(method);
        }
    }
    *result = pv_bool(matches);
    return 1;
}

static int numeric_argument(pphp_state *state, const char *name, pvalue value,
                            pphp_float *number, int *integer) {
    if (pv_to_number(value, number, integer)) return 1;
    pphp_runtime_error(state, 0U, "%s() expects numeric arguments", name);
    return 0;
}

static int call_math_builtin(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    pphp_float a;
    pphp_float b;
    int ai;
    int bi;
    if (name_is(name, "pi")) {
        if (!require_count(state, "pi", count, 0U, 0U)) return -1;
        *result = pv_float((pphp_float)3.14159265358979323846);
        return 1;
    }
    if (name_is(name, "intdiv") || name_is(name, "fdiv") ||
        name_is(name, "fmod") || name_is(name, "pow") ||
        name_is(name, "atan2")) {
        if (!require_count(state, "binary math", count, 2U, 2U) ||
            !numeric_argument(state, "binary math", arguments[0], &a, &ai) ||
            !numeric_argument(state, "binary math", arguments[1], &b, &bi)) return -1;
        if (name_is(name, "intdiv")) {
            if ((pphp_int)b == 0) {
                pphp_runtime_error(state, 0U, "Division by zero");
                return -1;
            }
            *result = pv_int((pphp_int)a / (pphp_int)b);
        } else if (name_is(name, "fdiv")) {
            *result = pv_float(a / b);
        } else if (name_is(name, "fmod")) {
            *result = pv_float((pphp_float)fmod((double)a, (double)b));
        } else if (name_is(name, "pow")) {
            *result = pv_float((pphp_float)pow((double)a, (double)b));
        } else {
            *result = pv_float((pphp_float)atan2((double)a, (double)b));
        }
        return 1;
    }
    if (name_is(name, "floor") || name_is(name, "ceil") ||
        name_is(name, "sqrt") || name_is(name, "exp") ||
        name_is(name, "log10") || name_is(name, "sin") ||
        name_is(name, "cos") || name_is(name, "tan") ||
        name_is(name, "asin") || name_is(name, "acos") ||
        name_is(name, "atan")) {
        double value;
        if (!require_count(state, "unary math", count, 1U, 1U) ||
            !numeric_argument(state, "unary math", arguments[0], &a, &ai)) return -1;
        if (name_is(name, "floor")) value = floor((double)a);
        else if (name_is(name, "ceil")) value = ceil((double)a);
        else if (name_is(name, "sqrt")) value = sqrt((double)a);
        else if (name_is(name, "exp")) value = exp((double)a);
        else if (name_is(name, "log10")) value = log10((double)a);
        else if (name_is(name, "sin")) value = sin((double)a);
        else if (name_is(name, "cos")) value = cos((double)a);
        else if (name_is(name, "tan")) value = tan((double)a);
        else if (name_is(name, "asin")) value = asin((double)a);
        else if (name_is(name, "acos")) value = acos((double)a);
        else value = atan((double)a);
        *result = pv_float((pphp_float)value);
        return 1;
    }
    if (name_is(name, "log")) {
        if (!require_count(state, "log", count, 1U, 2U) ||
            !numeric_argument(state, "log", arguments[0], &a, &ai)) return -1;
        if (count == 2U) {
            if (!numeric_argument(state, "log", arguments[1], &b, &bi)) return -1;
            *result = pv_float((pphp_float)(log((double)a) / log((double)b)));
        } else {
            *result = pv_float((pphp_float)log((double)a));
        }
        return 1;
    }
    if (name_is(name, "round")) {
        int precision = 0;
        double scale;
        if (!require_count(state, "round", count, 1U, 2U) ||
            !numeric_argument(state, "round", arguments[0], &a, &ai)) return -1;
        if (count == 2U && arguments[1].type == PT_INT) precision = arguments[1].as.i;
        scale = pow(10.0, (double)precision);
        *result = pv_float((pphp_float)(round((double)a * scale) / scale));
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
        if (count != 1U || arguments[0].type != PT_ARRAY) {
            pphp_runtime_error(state, 0U, "array_sum() expects one array");
            return -1;
        }
        array = (const parray *)arguments[0].as.gc;
        while (position < array->used) {
            pvalue key;
            pvalue item;
            size_t next;
            pphp_float number;
            int integer;
            if (!pa_entry_at(array, position, &key, &item, &next)) break;
            if (pv_to_number(item, &number, &integer)) {
                sum += number;
                all_integer = all_integer && integer;
            }
            pv_release(key);
            pv_release(item);
            position = next;
        }
        *result = all_integer ? pv_int((pphp_int)sum) : pv_float(sum);
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
