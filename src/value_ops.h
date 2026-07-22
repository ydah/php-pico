#ifndef PPHP_VALUE_OPS_H
#define PPHP_VALUE_OPS_H

#include <stddef.h>

#include "pstring.h"
#include "value.h"

typedef enum pv_operation {
    PV_ADD,
    PV_SUB,
    PV_MUL,
    PV_DIV,
    PV_MOD,
    PV_POW,
    PV_CONCAT,
    PV_BAND,
    PV_BOR,
    PV_BXOR,
    PV_SHL,
    PV_SHR
} pv_operation;

int pv_to_number(pvalue value, pphp_float *number, int *is_integer);
int pv_to_number_prefix(pvalue value, pphp_float *number, int *is_integer);
int pv_binary_operation(pv_operation operation, pvalue left, pvalue right,
                        pvalue *result, const char **error);
int pv_compare(pvalue left, pvalue right, int strict, int *result,
               const char **error);
pstring *pv_to_string(pvalue value);

int pphp_integer_add(pphp_int left, pphp_int right, pphp_int *result);
int pphp_integer_subtract(pphp_int left, pphp_int right, pphp_int *result);
int pphp_integer_multiply(pphp_int left, pphp_int right, pphp_int *result);
int pphp_integer_negate(pphp_int value, pphp_int *result);
int pphp_integer_division_overflows(pphp_int left, pphp_int right);
int pphp_integer_power(pphp_int base, pphp_int exponent, pphp_int *result);
int pphp_number_to_integer(pphp_float number, int exact, pphp_int *result);

#endif
