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

typedef struct pphp_numeric {
    pphp_float number;
    pphp_int integer;
    int is_integer;
    int integer_exact;
    int integer_out_of_range;
    int string_status;
} pphp_numeric;

enum {
    PPHP_NUMERIC_STRING_EXACT = 0,
    PPHP_NUMERIC_STRING_TRAILING = 1,
    PPHP_NUMERIC_STRING_INVALID = 2
};

int pv_to_number(pvalue value, pphp_float *number, int *is_integer);
int pv_to_number_prefix(pvalue value, pphp_float *number, int *is_integer);
int pv_to_numeric(pvalue value, int require_complete, pphp_numeric *numeric);
int pv_binary_operation(pv_operation operation, pvalue left, pvalue right,
                        pvalue *result, const char **error,
                        unsigned *non_numeric_operands);
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
int pphp_numeric_to_integer(const pphp_numeric *numeric, int exact,
                            pphp_int *result);
#if PPHP_ENABLE_FLOAT
pphp_float pphp_integer_digits_to_float(const char *digits, size_t length,
                                        unsigned base);
#endif

#endif
