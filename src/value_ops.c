#include "value_ops.h"

#if PPHP_ENABLE_FLOAT
#include "float_format.h"
#endif
#include "pstring.h"
#include "parray.h"
#include "pclass.h"
#include "pphp/pphp.h"

#if PPHP_ENABLE_FLOAT
#include <math.h>
#endif
#include <limits.h>
#include <stdio.h>
#include <string.h>

int pphp_integer_add(pphp_int left, pphp_int right, pphp_int *result) {
    if ((right > 0 && left > PPHP_INT_MAXIMUM - right) ||
        (right < 0 && left < PPHP_INT_MINIMUM - right)) {
        return 0;
    }
    *result = left + right;
    return 1;
}

int pphp_integer_subtract(pphp_int left, pphp_int right, pphp_int *result) {
    if ((right > 0 && left < PPHP_INT_MINIMUM + right) ||
        (right < 0 && left > PPHP_INT_MAXIMUM + right)) {
        return 0;
    }
    *result = left - right;
    return 1;
}

int pphp_integer_multiply(pphp_int left, pphp_int right, pphp_int *result) {
    if (left > 0) {
        if ((right > 0 && left > PPHP_INT_MAXIMUM / right) ||
            (right < 0 && right < PPHP_INT_MINIMUM / left)) return 0;
    } else if (left < 0) {
        if ((right > 0 && left < PPHP_INT_MINIMUM / right) ||
            (right < 0 && left < PPHP_INT_MAXIMUM / right)) return 0;
    }
    *result = left * right;
    return 1;
}

int pphp_integer_negate(pphp_int value, pphp_int *result) {
    if (value == PPHP_INT_MINIMUM) return 0;
    *result = -value;
    return 1;
}

int pphp_integer_division_overflows(pphp_int left, pphp_int right) {
    return left == PPHP_INT_MINIMUM && right == (pphp_int)-1;
}

int pphp_integer_power(pphp_int base, pphp_int exponent, pphp_int *result) {
    pphp_int value = 1;
    if (exponent < 0) return 0;
    while (exponent != 0) {
        if ((exponent & 1) != 0 &&
            !pphp_integer_multiply(value, base, &value)) return 0;
        exponent >>= 1;
        if (exponent != 0 &&
            !pphp_integer_multiply(base, base, &base)) return 0;
    }
    *result = value;
    return 1;
}

static pphp_int integer_shift_left(pphp_int value, pphp_int distance) {
    unsigned width = (unsigned)(sizeof(pphp_int) * CHAR_BIT);
    unsigned shift = (unsigned)((uint64_t)distance & (uint64_t)(width - 1U));
#if PPHP_INT64
    uint64_t bits = (uint64_t)value << shift;
#else
    uint32_t bits = (uint32_t)value << shift;
#endif
    pphp_int shifted;
    memcpy(&shifted, &bits, sizeof(shifted));
    return shifted;
}

static pphp_int integer_shift_right(pphp_int value, pphp_int distance) {
    unsigned width = (unsigned)(sizeof(pphp_int) * CHAR_BIT);
    unsigned shift = (unsigned)((uint64_t)distance & (uint64_t)(width - 1U));
    uint64_t magnitude;
    uint64_t quotient;
    uint64_t remainder_mask;
    if (shift == 0U) return value;
    if (value >= 0) {
        return (pphp_int)((uint64_t)value >> shift);
    }
    magnitude = UINT64_C(0) - (uint64_t)value;
    quotient = magnitude >> shift;
    remainder_mask = (UINT64_C(1) << shift) - 1U;
    if ((magnitude & remainder_mask) != 0U) quotient++;
    if (quotient == (uint64_t)PPHP_INT_MAXIMUM + 1U) {
        return PPHP_INT_MINIMUM;
    }
    return -(pphp_int)quotient;
}

#if PPHP_ENABLE_FLOAT
int pphp_number_to_integer(pphp_float number, int exact, pphp_int *result) {
    const pphp_float upper_exclusive = -(pphp_float)PPHP_INT_MINIMUM;
    pphp_int integer;
    if (!(number >= (pphp_float)PPHP_INT_MINIMUM &&
          number < upper_exclusive)) return 0;
    integer = (pphp_int)number;
    if (exact && number != (pphp_float)integer) return 0;
    *result = integer;
    return 1;
}

static int integer_binary_float(pv_operation operation, pphp_int left,
                                pphp_int right, pvalue *result,
                                const char **error) {
    pphp_int integer;
    switch (operation) {
        case PV_ADD:
            *result = pphp_integer_add(left, right, &integer)
                          ? pv_int(integer)
                          : pv_float((pphp_float)left + (pphp_float)right);
            return 1;
        case PV_SUB:
            *result = pphp_integer_subtract(left, right, &integer)
                          ? pv_int(integer)
                          : pv_float((pphp_float)left - (pphp_float)right);
            return 1;
        case PV_MUL:
            *result = pphp_integer_multiply(left, right, &integer)
                          ? pv_int(integer)
                          : pv_float((pphp_float)left * (pphp_float)right);
            return 1;
        case PV_DIV:
            if (right == 0) {
                *error = "Division by zero";
                return 0;
            }
            if (!pphp_integer_division_overflows(left, right) &&
                left % right == 0) {
                *result = pv_int(left / right);
            } else {
                *result = pv_float((pphp_float)left / (pphp_float)right);
            }
            return 1;
        case PV_MOD:
            if (right == 0) {
                *error = "Modulo by zero";
                return 0;
            }
            *result = pv_int(pphp_integer_division_overflows(left, right)
                                 ? 0
                                 : left % right);
            return 1;
        case PV_POW:
            if (right >= 0 && pphp_integer_power(left, right, &integer)) {
                *result = pv_int(integer);
            } else {
                *result = pv_float((pphp_float)pow((double)left,
                                                   (double)right));
            }
            return 1;
        case PV_BAND: *result = pv_int(left & right); return 1;
        case PV_BOR: *result = pv_int(left | right); return 1;
        case PV_BXOR: *result = pv_int(left ^ right); return 1;
        case PV_SHL: *result = pv_int(integer_shift_left(left, right)); return 1;
        case PV_SHR: *result = pv_int(integer_shift_right(left, right)); return 1;
        case PV_CONCAT: break;
    }
    return 0;
}
#endif

#if !PPHP_ENABLE_FLOAT
static int string_number(const char *text, size_t length, pphp_float *number,
                         int *is_integer, int require_complete) {
    size_t position = 0U;
    int negative = 0;
    int digits = 0;
    uint64_t magnitude = 0U;
    uint64_t limit;
    while (position < length &&
           (text[position] == ' ' || text[position] == '\t' ||
            text[position] == '\n' || text[position] == '\r')) position++;
    if (position < length &&
        (text[position] == '+' || text[position] == '-')) {
        negative = text[position++] == '-';
    }
    limit = (uint64_t)PPHP_INT_MAXIMUM + (negative ? 1U : 0U);
    while (position < length && text[position] >= '0' &&
           text[position] <= '9') {
        unsigned digit = (unsigned)(text[position++] - '0');
        if (magnitude > (limit - digit) / 10U) {
            *is_integer = -1;
            return 0;
        }
        magnitude = magnitude * 10U + digit;
        digits++;
    }
    if (digits == 0 ||
        (position < length &&
         (text[position] == '.' || text[position] == 'e' ||
          text[position] == 'E'))) return 0;
    if (require_complete) {
        while (position < length &&
               (text[position] == ' ' || text[position] == '\t' ||
                text[position] == '\n' || text[position] == '\r')) position++;
        if (position != length) return 0;
    }
    if (negative && magnitude == (uint64_t)PPHP_INT_MAXIMUM + 1U) {
        *number = PPHP_INT_MINIMUM;
    } else {
        pphp_int integer = (pphp_int)magnitude;
        *number = negative ? -integer : integer;
    }
    *is_integer = 1;
    return 1;
}
#else
static int string_number(const char *text, size_t length, pphp_float *number,
                         int *is_integer, int require_complete) {
    size_t i = 0U;
    int sign = 1;
    pphp_float value = 0;
#if PPHP_ENABLE_FLOAT
    pphp_float scale = 1;
#endif
    int digits = 0;
    int fraction = 0;
    int exponent = 0;
#if PPHP_ENABLE_FLOAT
    int exponent_sign = 1;
#endif
    while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' ||
                          text[i] == '\r')) {
        i++;
    }
    if (i < length && (text[i] == '+' || text[i] == '-')) {
        sign = text[i++] == '-' ? -1 : 1;
    }
    while (i < length && text[i] >= '0' && text[i] <= '9') {
        value = value * (pphp_float)10 + (pphp_float)(text[i++] - '0');
        digits++;
    }
    if (i < length && text[i] == '.') {
#if PPHP_ENABLE_FLOAT
        fraction = 1;
        i++;
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            scale *= (pphp_float)0.1;
            value += (pphp_float)(text[i++] - '0') * scale;
            digits++;
        }
#else
        return 0;
#endif
    }
    if (digits == 0) {
        return 0;
    }
    if (i < length && (text[i] == 'e' || text[i] == 'E')) {
#if PPHP_ENABLE_FLOAT
        size_t exponent_start = i++;
        int exponent_digits = 0;
        if (i < length && (text[i] == '+' || text[i] == '-')) {
            exponent_sign = text[i++] == '-' ? -1 : 1;
        }
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            if (exponent < 10000) {
                exponent = exponent * 10 + (text[i] - '0');
                if (exponent > 10000) exponent = 10000;
            }
            i++;
            exponent_digits++;
        }
        if (exponent_digits == 0) {
            i = exponent_start;
        } else {
            fraction = 1;
        }
#else
        return 0;
#endif
    }
    if (require_complete) {
        while (i < length &&
               (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' ||
                text[i] == '\r')) {
            i++;
        }
        if (i != length) return 0;
    }
    if (exponent != 0) {
#if PPHP_ENABLE_FLOAT
        value *= (pphp_float)pow(10.0, (double)(exponent * exponent_sign));
#else
        return 0;
#endif
    }
    *number = sign < 0 ? -value : value;
    *is_integer = !fraction;
    return 1;
}
#endif

int pv_to_number(pvalue value, pphp_float *number, int *is_integer) {
    *is_integer = 0;
    switch ((pvalue_type)value.type) {
        case PT_INT:
            *number = (pphp_float)value.as.i;
            *is_integer = 1;
            return 1;
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT:
            *number = value.as.f;
            *is_integer = 0;
            return 1;
#endif
        case PT_TRUE:
            *number = (pphp_float)1;
            *is_integer = 1;
            return 1;
        case PT_FALSE:
        case PT_NULL:
            *number = (pphp_float)0;
            *is_integer = 1;
            return 1;
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return string_number(string->data, string->length, number,
                                 is_integer, 1);
        }
        default:
            return 0;
    }
}

int pv_to_number_prefix(pvalue value, pphp_float *number, int *is_integer) {
    *is_integer = 0;
    if (value.type == PT_STRING) {
        const pstring *string = (const pstring *)value.as.gc;
        return string_number(string->data, string->length, number,
                             is_integer, 0);
    }
    return pv_to_number(value, number, is_integer);
}

#if PPHP_ENABLE_FLOAT
static pvalue numeric_result(pphp_float number, int integers) {
    pphp_int integer;
    if (integers && pphp_number_to_integer(number, 1, &integer)) return pv_int(integer);
    return pv_float(number);
}
#endif

pstring *pv_to_string(pvalue value) {
    char buffer[64];
    int length;
    switch ((pvalue_type)value.type) {
        case PT_NULL:
        case PT_FALSE:
            return ps_new("", 0U);
        case PT_TRUE:
            return ps_new("1", 1U);
        case PT_INT:
            length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value.as.i);
            return length < 0 ? NULL : ps_new(buffer, (size_t)length);
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT:
            length = pphp_format_float(buffer, sizeof(buffer), value.as.f,
                                       'g', 14);
            return length < 0 ? NULL : ps_new(buffer, (size_t)length);
#endif
        case PT_STRING:
            return ps_new(((const pstring *)value.as.gc)->data,
                          ((const pstring *)value.as.gc)->length);
        case PT_ARRAY:
            return ps_new("Array", 5U);
        default:
            return NULL;
    }
}

int pv_binary_operation(pv_operation operation, pvalue left, pvalue right,
                        pvalue *result, const char **error) {
    pphp_float a;
    pphp_float b;
    int ai = 0;
    int bi = 0;
    if (operation == PV_CONCAT) {
        pstring *left_string = pv_to_string(left);
        pstring *right_string = pv_to_string(right);
        pstring *joined;
        char *bytes;
        if (left_string == NULL || right_string == NULL ||
            (size_t)left_string->length + right_string->length > PPHP_STR_MAX) {
            if (left_string != NULL) ps_destroy(left_string);
            if (right_string != NULL) ps_destroy(right_string);
            *error = "value cannot be converted to string";
            return 0;
        }
        if (left_string->length == 0U && right_string->length == 0U) {
            joined = ps_new("", 0U);
            ps_destroy(left_string);
            ps_destroy(right_string);
            if (joined == NULL) {
                *error = "out of memory during string concatenation";
                return 0;
            }
            *result = pv_heap(PT_STRING, &joined->header);
            return 1;
        }
        bytes = pphp_alloc((size_t)left_string->length + right_string->length);
        if (bytes == NULL) {
            ps_destroy(left_string);
            ps_destroy(right_string);
            *error = "out of memory during string concatenation";
            return 0;
        }
        memcpy(bytes, left_string->data, left_string->length);
        memcpy(bytes + left_string->length, right_string->data, right_string->length);
        joined = ps_new(bytes, (size_t)left_string->length + right_string->length);
        pphp_free(bytes);
        ps_destroy(left_string);
        ps_destroy(right_string);
        if (joined == NULL) {
            *error = "out of memory during string concatenation";
            return 0;
        }
        *result = pv_heap(PT_STRING, &joined->header);
        return 1;
    }
#if PPHP_ENABLE_FLOAT
    if (left.type == PT_INT && right.type == PT_INT) {
        return integer_binary_float(operation, left.as.i, right.as.i,
                                    result, error);
    }
#endif
    if (!pv_to_number_prefix(left, &a, &ai) ||
        !pv_to_number_prefix(right, &b, &bi)) {
        *error = ai < 0 || bi < 0
                     ? "integer overflow requires float support"
                     : "unsupported operand types";
        return 0;
    }
    switch (operation) {
        case PV_ADD:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a + b, ai && bi);
#else
            if (!pphp_integer_add(a, b, &a)) goto integer_overflow;
            *result = pv_int(a);
#endif
            return 1;
        case PV_SUB:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a - b, ai && bi);
#else
            if (!pphp_integer_subtract(a, b, &a)) goto integer_overflow;
            *result = pv_int(a);
#endif
            return 1;
        case PV_MUL:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a * b, ai && bi);
#else
            if (!pphp_integer_multiply(a, b, &a)) goto integer_overflow;
            *result = pv_int(a);
#endif
            return 1;
        case PV_DIV:
            if (b == (pphp_float)0) {
                *error = "Division by zero";
                return 0;
            }
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a / b, ai && bi && fmod((double)a, (double)b) == 0.0);
#else
            if (pphp_integer_division_overflows(a, b)) goto integer_overflow;
            if (a % b != 0) {
                *error = "non-integral division requires float support";
                return 0;
            }
            *result = pv_int(a / b);
#endif
            return 1;
        case PV_MOD:
            if (b == (pphp_float)0) {
                *error = "Modulo by zero";
                return 0;
            }
#if PPHP_ENABLE_FLOAT
            {
                pphp_int left_integer;
                pphp_int right_integer;
                if (!pphp_number_to_integer(a, 0, &left_integer) ||
                    !pphp_number_to_integer(b, 0, &right_integer)) {
                    *error = "integer conversion is out of range";
                    return 0;
                }
                if (right_integer == 0) {
                    *error = "Modulo by zero";
                    return 0;
                }
                *result = pv_int(pphp_integer_division_overflows(
                                     left_integer, right_integer)
                                     ? 0
                                     : left_integer % right_integer);
                return 1;
            }
#else
            if (pphp_integer_division_overflows((pphp_int)a,
                                                (pphp_int)b)) {
                goto integer_overflow;
            }
            *result = pv_int((pphp_int)a % (pphp_int)b);
            return 1;
#endif
        case PV_POW:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result((pphp_float)pow((double)a, (double)b), 0);
#else
            if (b < 0) {
                *error = "negative exponent requires float support";
                return 0;
            }
            if (!pphp_integer_power(a, b, &a)) goto integer_overflow;
            *result = pv_int(a);
#endif
            return 1;
        case PV_BAND: case PV_BOR: case PV_BXOR: case PV_SHL: case PV_SHR:
#if PPHP_ENABLE_FLOAT
        {
            pphp_int left_integer;
            pphp_int right_integer;
            if (!pphp_number_to_integer(a, 0, &left_integer) ||
                !pphp_number_to_integer(b, 0, &right_integer)) {
                *error = "integer conversion is out of range";
                return 0;
            }
            if (operation == PV_BAND) *result = pv_int(left_integer & right_integer);
            else if (operation == PV_BOR) *result = pv_int(left_integer | right_integer);
            else if (operation == PV_BXOR) *result = pv_int(left_integer ^ right_integer);
            else if (operation == PV_SHL) {
                *result = pv_int(integer_shift_left(left_integer, right_integer));
            } else {
                *result = pv_int(integer_shift_right(left_integer, right_integer));
            }
            return 1;
        }
#else
            if (operation == PV_BAND) *result = pv_int(a & b);
            else if (operation == PV_BOR) *result = pv_int(a | b);
            else if (operation == PV_BXOR) *result = pv_int(a ^ b);
            else if (operation == PV_SHL) *result = pv_int(integer_shift_left(a, b));
            else *result = pv_int(integer_shift_right(a, b));
            return 1;
#endif
        case PV_CONCAT: break;
    }
    *error = "invalid binary operation";
    return 0;
#if !PPHP_ENABLE_FLOAT
integer_overflow:
    *error = "integer overflow requires float support";
    return 0;
#endif
}

#if !PPHP_ENABLE_FLOAT
int pphp_number_to_integer(pphp_float number, int exact, pphp_int *result) {
    (void)exact;
    *result = number;
    return 1;
}
#endif

int pv_compare(pvalue left, pvalue right, int strict, int *result,
               const char **error) {
    pphp_float a;
    pphp_float b;
    int ai;
    int bi;
    (void)error;
    if (strict && left.type != right.type) {
        *result = left.type < right.type ? -1 : 1;
        return 1;
    }
    if (strict && (left.type == PT_OBJECT || left.type == PT_CLOSURE ||
                   left.type == PT_RESOURCE)) {
        *result = left.as.gc == right.as.gc ? 0 : 1;
        return 1;
    }
    if (left.type == PT_ARRAY && right.type == PT_ARRAY) {
        const parray *left_array = (const parray *)left.as.gc;
        const parray *right_array = (const parray *)right.as.gc;
        size_t left_position = 0U;
        size_t right_position = 0U;
        if (left_array->size != right_array->size) {
            *result = left_array->size > right_array->size ? 1 : -1;
            return 1;
        }
        if (!strict) {
            while (left_position < left_array->used) {
                pvalue left_key;
                pvalue left_value;
                pvalue right_value = pv_null();
                size_t left_next;
                int value_result;
                if (!pa_entry_at(left_array, left_position, &left_key,
                                 &left_value, &left_next)) {
                    break;
                }
                if (!pa_get(right_array, left_key, &right_value)) {
                    pv_release(left_key);
                    pv_release(left_value);
                    *result = 1;
                    return 1;
                }
                (void)pv_compare(left_value, right_value, 0, &value_result,
                                 error);
                pv_release(left_key);
                pv_release(left_value);
                pv_release(right_value);
                if (value_result != 0) {
                    *result = value_result;
                    return 1;
                }
                left_position = left_next;
            }
            *result = 0;
            return 1;
        }
        while (left_position < left_array->used && right_position < right_array->used) {
            pvalue left_key;
            pvalue left_value;
            pvalue right_key;
            pvalue right_value;
            size_t left_next;
            size_t right_next;
            int key_result;
            int value_result;
            if (!pa_entry_at(left_array, left_position, &left_key, &left_value,
                             &left_next) ||
                !pa_entry_at(right_array, right_position, &right_key, &right_value,
                             &right_next)) {
                break;
            }
            (void)pv_compare(left_key, right_key, strict, &key_result, error);
            (void)pv_compare(left_value, right_value, strict, &value_result, error);
            pv_release(left_key);
            pv_release(left_value);
            pv_release(right_key);
            pv_release(right_value);
            if (key_result != 0 || value_result != 0) {
                *result = key_result != 0 ? key_result : value_result;
                return 1;
            }
            left_position = left_next;
            right_position = right_next;
        }
        *result = 0;
        return 1;
    }
    if (left.type == PT_OBJECT && right.type == PT_OBJECT) {
        const pobject *left_object = (const pobject *)left.as.gc;
        const pobject *right_object = (const pobject *)right.as.gc;
        size_t i;
        if (left_object->class_entry != right_object->class_entry) {
            *result = (uintptr_t)left_object->class_entry >
                              (uintptr_t)right_object->class_entry
                          ? 1
                          : -1;
            return 1;
        }
        for (i = 0U; i < left_object->class_entry->property_count; i++) {
            int property_result;
            (void)pv_compare(left_object->slots[i], right_object->slots[i],
                             0, &property_result, error);
            if (property_result != 0) {
                *result = property_result;
                return 1;
            }
        }
        *result = 0;
        return 1;
    }
    if (left.type == PT_TRUE || left.type == PT_FALSE || right.type == PT_TRUE ||
        right.type == PT_FALSE) {
        int lt = pv_is_truthy(left);
        int rt = pv_is_truthy(right);
        *result = (lt > rt) - (lt < rt);
        return 1;
    }
    if (left.type == PT_NULL || right.type == PT_NULL) {
        int lt = pv_is_truthy(left);
        int rt = pv_is_truthy(right);
        *result = (lt > rt) - (lt < rt);
        return 1;
    }
    if (pv_to_number(left, &a, &ai) && pv_to_number(right, &b, &bi)) {
        *result = (a > b) - (a < b);
        return 1;
    }
    if (left.type == PT_STRING && right.type == PT_STRING) {
        const pstring *ls = (const pstring *)left.as.gc;
        const pstring *rs = (const pstring *)right.as.gc;
        size_t shortest = ls->length < rs->length ? ls->length : rs->length;
        int compared = memcmp(ls->data, rs->data, shortest);
        *result = compared != 0 ? (compared > 0 ? 1 : -1)
                                : ((ls->length > rs->length) - (ls->length < rs->length));
        return 1;
    }
    *result = (left.type > right.type) - (left.type < right.type);
    return 1;
}
