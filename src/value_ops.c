#include "value_ops.h"

#include "pstring.h"
#include "pphp/pphp.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int string_number(const char *text, size_t length, pphp_float *number,
                         int *is_integer) {
    size_t i = 0U;
    int sign = 1;
    pphp_float value = 0;
    pphp_float scale = 1;
    int digits = 0;
    int fraction = 0;
    int exponent = 0;
    int exponent_sign = 1;
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
        fraction = 1;
        i++;
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            scale *= (pphp_float)0.1;
            value += (pphp_float)(text[i++] - '0') * scale;
            digits++;
        }
    }
    if (digits == 0) {
        return 0;
    }
    if (i < length && (text[i] == 'e' || text[i] == 'E')) {
        size_t exponent_start = i++;
        int exponent_digits = 0;
        if (i < length && (text[i] == '+' || text[i] == '-')) {
            exponent_sign = text[i++] == '-' ? -1 : 1;
        }
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            exponent = exponent * 10 + (text[i++] - '0');
            exponent_digits++;
        }
        if (exponent_digits == 0) {
            i = exponent_start;
        } else {
            fraction = 1;
        }
    }
    while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' ||
                          text[i] == '\r')) {
        i++;
    }
    if (i != length) {
        return 0;
    }
    if (exponent != 0) {
        value *= (pphp_float)pow(10.0, (double)(exponent * exponent_sign));
    }
    *number = sign < 0 ? -value : value;
    *is_integer = !fraction;
    return 1;
}

int pv_to_number(pvalue value, pphp_float *number, int *is_integer) {
    switch ((pvalue_type)value.type) {
        case PT_INT:
            *number = (pphp_float)value.as.i;
            *is_integer = 1;
            return 1;
        case PT_FLOAT:
            *number = value.as.f;
            *is_integer = 0;
            return 1;
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
            return string_number(string->data, string->length, number, is_integer);
        }
        default:
            return 0;
    }
}

static pvalue numeric_result(pphp_float number, int integers) {
    if (integers && number >= (pphp_float)INT32_MIN && number <= (pphp_float)INT32_MAX &&
        number == (pphp_float)(pphp_int)number) {
        return pv_int((pphp_int)number);
    }
    return pv_float(number);
}

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
        case PT_FLOAT:
            length = snprintf(buffer, sizeof(buffer), "%.14g", (double)value.as.f);
            return length < 0 ? NULL : ps_new(buffer, (size_t)length);
        case PT_STRING:
            return ps_new(((const pstring *)value.as.gc)->data,
                          ((const pstring *)value.as.gc)->length);
        default:
            return NULL;
    }
}

int pv_binary_operation(pv_operation operation, pvalue left, pvalue right,
                        pvalue *result, const char **error) {
    pphp_float a;
    pphp_float b;
    int ai;
    int bi;
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
    if (!pv_to_number(left, &a, &ai) || !pv_to_number(right, &b, &bi)) {
        *error = "unsupported operand types";
        return 0;
    }
    switch (operation) {
        case PV_ADD: *result = numeric_result(a + b, ai && bi); return 1;
        case PV_SUB: *result = numeric_result(a - b, ai && bi); return 1;
        case PV_MUL: *result = numeric_result(a * b, ai && bi); return 1;
        case PV_DIV:
            if (b == (pphp_float)0) {
                *error = "Division by zero";
                return 0;
            }
            *result = numeric_result(a / b, ai && bi && fmod((double)a, (double)b) == 0.0);
            return 1;
        case PV_MOD:
            if ((pphp_int)b == 0) {
                *error = "Modulo by zero";
                return 0;
            }
            *result = pv_int((pphp_int)a % (pphp_int)b);
            return 1;
        case PV_POW: *result = numeric_result((pphp_float)pow((double)a, (double)b), 0); return 1;
        case PV_BAND: *result = pv_int((pphp_int)a & (pphp_int)b); return 1;
        case PV_BOR: *result = pv_int((pphp_int)a | (pphp_int)b); return 1;
        case PV_BXOR: *result = pv_int((pphp_int)a ^ (pphp_int)b); return 1;
        case PV_SHL: *result = pv_int((pphp_int)((uint32_t)(pphp_int)a << ((unsigned)b & 31U))); return 1;
        case PV_SHR: *result = pv_int((pphp_int)a >> ((unsigned)b & 31U)); return 1;
        case PV_CONCAT: break;
    }
    *error = "invalid binary operation";
    return 0;
}

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
