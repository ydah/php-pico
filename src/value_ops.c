#include "value_ops.h"

#if PPHP_ENABLE_FLOAT
#include "float_format.h"
#include "float_math.h"
#endif
#include "pstring.h"
#include "parray.h"
#include "pclass.h"
#include "pphp/pphp.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#if PPHP_ENABLE_FLOAT
#if PPHP_USE_DOUBLE
#define PPHP_INTEGER_FLOAT_LIMBS 34U
#define PPHP_INTEGER_FLOAT_PRECISION 53U
#define PPHP_INTEGER_FLOAT_MAX_EXPONENT 1023
#else
#define PPHP_INTEGER_FLOAT_LIMBS 5U
#define PPHP_INTEGER_FLOAT_PRECISION 24U
#define PPHP_INTEGER_FLOAT_MAX_EXPONENT 127
#endif

typedef struct pphp_integer_float_big {
    uint32_t limbs[PPHP_INTEGER_FLOAT_LIMBS];
    size_t used;
} pphp_integer_float_big;

static unsigned integer_float_digit(char value) {
    if (value >= '0' && value <= '9') {
        return (unsigned)(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return (unsigned)(value - 'a') + 10U;
    }
    return (unsigned)(value - 'A') + 10U;
}

static int integer_float_append(pphp_integer_float_big *integer,
                                unsigned base, unsigned digit) {
    uint64_t carry = digit;
    size_t i;
    for (i = 0U; i < integer->used; i++) {
        uint64_t product = (uint64_t)integer->limbs[i] * base + carry;
        integer->limbs[i] = (uint32_t)product;
        carry = product >> 32U;
    }
    if (carry != 0U) {
        if (integer->used == PPHP_INTEGER_FLOAT_LIMBS) return 0;
        integer->limbs[integer->used++] = (uint32_t)carry;
    }
    return 1;
}

static size_t integer_float_bit_length(
    const pphp_integer_float_big *integer) {
    uint32_t top;
    size_t length;
    if (integer->used == 0U) return 0U;
    top = integer->limbs[integer->used - 1U];
    length = (integer->used - 1U) * 32U;
    while (top != 0U) {
        length++;
        top >>= 1U;
    }
    return length;
}

static int integer_float_bit(const pphp_integer_float_big *integer,
                             size_t position) {
    size_t limb = position / 32U;
    if (limb >= integer->used) return 0;
    return (integer->limbs[limb] &
            (UINT32_C(1) << (unsigned)(position % 32U))) != 0U;
}

static int integer_float_any_bits_below(
    const pphp_integer_float_big *integer, size_t limit) {
    size_t whole = limit / 32U;
    size_t i;
    for (i = 0U; i < whole && i < integer->used; i++) {
        if (integer->limbs[i] != 0U) return 1;
    }
    if (whole < integer->used && limit % 32U != 0U) {
        uint32_t mask =
            (UINT32_C(1) << (unsigned)(limit % 32U)) - 1U;
        if ((integer->limbs[whole] & mask) != 0U) return 1;
    }
    return 0;
}

static uint64_t integer_float_top_bits(
    const pphp_integer_float_big *integer, size_t bit_length, size_t count) {
    uint64_t value = 0U;
    size_t position;
    size_t first = bit_length - count;
    for (position = bit_length; position > first; position--) {
        value = (value << 1U) |
                (uint64_t)integer_float_bit(integer, position - 1U);
    }
    return value;
}

static pphp_float integer_float_infinity(void) {
#if PPHP_USE_DOUBLE
    uint64_t bits = UINT64_C(0x7ff0000000000000);
    double number;
#else
    uint32_t bits = UINT32_C(0x7f800000);
    float number;
#endif
    memcpy(&number, &bits, sizeof(number));
    return (pphp_float)number;
}

pphp_float pphp_integer_digits_to_float(const char *digits, size_t length,
                                        unsigned base) {
    pphp_integer_float_big integer;
    size_t i;
    size_t bit_length;
    size_t retained;
    size_t discarded;
    uint64_t significand;
    int exponent;
    memset(&integer, 0, sizeof(integer));
    for (i = 0U; i < length; i++) {
        if (digits[i] != '_' &&
            !integer_float_append(&integer, base,
                                  integer_float_digit(digits[i]))) {
            return integer_float_infinity();
        }
    }
    bit_length = integer_float_bit_length(&integer);
    if (bit_length == 0U) return (pphp_float)0;
    retained = bit_length < PPHP_INTEGER_FLOAT_PRECISION
                   ? bit_length : PPHP_INTEGER_FLOAT_PRECISION;
    discarded = bit_length - retained;
    significand = integer_float_top_bits(&integer, bit_length, retained);
    if (retained < PPHP_INTEGER_FLOAT_PRECISION) {
        significand <<= PPHP_INTEGER_FLOAT_PRECISION - retained;
    } else if (discarded != 0U &&
               integer_float_bit(&integer, discarded - 1U) &&
               (integer_float_any_bits_below(&integer, discarded - 1U) ||
                (significand & 1U) != 0U)) {
        significand++;
    }
    exponent = (int)bit_length - 1;
    if (significand ==
        (UINT64_C(1) << PPHP_INTEGER_FLOAT_PRECISION)) {
        significand >>= 1U;
        exponent++;
    }
    if (exponent > PPHP_INTEGER_FLOAT_MAX_EXPONENT) {
        return integer_float_infinity();
    }
#if PPHP_USE_DOUBLE
    {
        uint64_t bits = ((uint64_t)(exponent + 1023) << 52U) |
                        (significand & UINT64_C(0x000fffffffffffff));
        double number;
        memcpy(&number, &bits, sizeof(number));
        return (pphp_float)number;
    }
#else
    {
        uint32_t bits = ((uint32_t)(exponent + 127) << 23U) |
                        ((uint32_t)significand & UINT32_C(0x007fffff));
        float number;
        memcpy(&number, &bits, sizeof(number));
        return (pphp_float)number;
    }
#endif
}

#if PPHP_USE_DOUBLE
pphp_float pphp_decimal_to_float(const char *text, size_t length) {
    size_t i = 0U;
    pphp_float value = 0;
    pphp_float fraction = (pphp_float)0.1;
    int after_dot = 0;
    int negative = 0;
    int exponent = 0;
    int exponent_sign = 1;
    if (i < length && (text[i] == '+' || text[i] == '-')) {
        negative = text[i++] == '-';
    }
    while (i < length && text[i] != 'e' && text[i] != 'E') {
        char byte = text[i++];
        if (byte == '_') continue;
        if (byte == '.') {
            after_dot = 1;
        } else if (after_dot) {
            value += (pphp_float)(byte - '0') * fraction;
            fraction *= (pphp_float)0.1;
        } else {
            value = value * (pphp_float)10 + (pphp_float)(byte - '0');
        }
    }
    if (i < length) {
        i++;
        if (i < length && (text[i] == '+' || text[i] == '-')) {
            exponent_sign = text[i++] == '-' ? -1 : 1;
        }
        while (i < length) {
            char byte = text[i++];
            if (byte != '_' && exponent < 10000) {
                exponent = exponent * 10 + (byte - '0');
                if (exponent > 10000) exponent = 10000;
            }
        }
    }
    if (exponent != 0 && value != (pphp_float)0) {
        value *= pphp_float_power(
            (pphp_float)10, (pphp_float)(exponent_sign * exponent));
    }
    return negative ? -value : value;
}
#else
#define PPHP_DECIMAL_SIGNIFICANT_LIMIT 150U
#define PPHP_DECIMAL_MIDPOINT_DIGITS 120U

typedef struct pphp_decimal_midpoint {
    uint8_t digits[PPHP_DECIMAL_MIDPOINT_DIGITS];
    size_t used;
} pphp_decimal_midpoint;

static void decimal_midpoint_init(pphp_decimal_midpoint *midpoint,
                                  uint32_t value) {
    midpoint->used = 0U;
    do {
        uint32_t quotient = value / 10U;
        midpoint->digits[midpoint->used++] =
            (uint8_t)(value - quotient * 10U);
        value = quotient;
    } while (value != 0U);
}

static void decimal_midpoint_multiply(pphp_decimal_midpoint *midpoint,
                                      unsigned factor) {
    unsigned carry = 0U;
    size_t i;
    for (i = 0U; i < midpoint->used; i++) {
        unsigned product = (unsigned)midpoint->digits[i] * factor + carry;
        unsigned quotient = product / 10U;
        midpoint->digits[i] = (uint8_t)(product - quotient * 10U);
        carry = quotient;
    }
    while (carry != 0U) {
        unsigned quotient = carry / 10U;
        midpoint->digits[midpoint->used++] =
            (uint8_t)(carry - quotient * 10U);
        carry = quotient;
    }
}

static int saturated_decimal_add(int value, int addend) {
    if (addend > 0 && value > INT_MAX - addend) return INT_MAX;
    if (addend < 0 && value < INT_MIN - addend) return INT_MIN;
    return value + addend;
}

static int decimal_compare_midpoint(const uint8_t *digits,
                                    size_t digit_count, int magnitude,
                                    int sticky, uint32_t lower) {
    pphp_decimal_midpoint midpoint;
    uint32_t exponent_field = lower >> 23U;
    uint32_t significand =
        (lower & UINT32_C(0x007fffff)) |
        (exponent_field != 0U ? UINT32_C(0x00800000) : 0U);
    int binary_exponent =
        (exponent_field != 0U ? (int)exponent_field - 1 : 0) - 150;
    int scale = binary_exponent < 0 ? -binary_exponent : 0;
    int midpoint_magnitude;
    size_t position;
    decimal_midpoint_init(&midpoint, significand * 2U + 1U);
    while (binary_exponent < 0) {
        decimal_midpoint_multiply(&midpoint, 5U);
        binary_exponent++;
    }
    while (binary_exponent > 0) {
        decimal_midpoint_multiply(&midpoint, 2U);
        binary_exponent--;
    }
    midpoint_magnitude = (int)midpoint.used - scale - 1;
    if (magnitude != midpoint_magnitude) {
        return magnitude > midpoint_magnitude ? 1 : -1;
    }
    for (position = 0U;
         position < digit_count || position < midpoint.used; position++) {
        unsigned left = position < digit_count ? digits[position] : 0U;
        unsigned right = position < midpoint.used
            ? midpoint.digits[midpoint.used - position - 1U] : 0U;
        if (left != right) return left > right ? 1 : -1;
    }
    return sticky ? 1 : 0;
}

pphp_float pphp_decimal_to_float(const char *text, size_t length) {
    uint8_t digits[PPHP_DECIMAL_SIGNIFICANT_LIMIT];
    size_t i = 0U;
    size_t digit_count = 0U;
    int after_dot = 0;
    int significant = 0;
    int sticky = 0;
    int negative = 0;
    int explicit_exponent = 0;
    int exponent_sign = 1;
    int magnitude = 0;
    uint32_t lower = 0U;
    uint32_t upper = UINT32_C(0x7f800000);
    uint32_t result_bits;
    float result;
    if (i < length && (text[i] == '+' || text[i] == '-')) {
        negative = text[i++] == '-';
    }
    while (i < length && text[i] != 'e' && text[i] != 'E') {
        char byte = text[i++];
        unsigned digit;
        if (byte == '_') continue;
        if (byte == '.') {
            after_dot = 1;
            continue;
        }
        digit = (unsigned)(byte - '0');
        if (!significant) {
            if (after_dot) {
                magnitude = saturated_decimal_add(magnitude, -1);
            }
            if (digit == 0U) continue;
            significant = 1;
        } else if (!after_dot) {
            magnitude = saturated_decimal_add(magnitude, 1);
        }
        if (digit_count < PPHP_DECIMAL_SIGNIFICANT_LIMIT) {
            digits[digit_count++] = (uint8_t)digit;
        } else {
            if (digit != 0U) sticky = 1;
        }
    }
    if (!significant) {
        result_bits = negative ? UINT32_C(0x80000000) : 0U;
        memcpy(&result, &result_bits, sizeof(result));
        return (pphp_float)result;
    }
    if (i < length) {
        i++;
        if (i < length && (text[i] == '+' || text[i] == '-')) {
            exponent_sign = text[i++] == '-' ? -1 : 1;
        }
        while (i < length) {
            char byte = text[i++];
            if (byte != '_' && explicit_exponent < INT_MAX) {
                int digit = byte - '0';
                if (explicit_exponent > (INT_MAX - digit) / 10) {
                    explicit_exponent = INT_MAX;
                } else {
                    explicit_exponent = explicit_exponent * 10 + digit;
                }
            }
        }
    }
    magnitude = saturated_decimal_add(
        magnitude, exponent_sign * explicit_exponent);
    if (magnitude > 38) {
        result_bits = UINT32_C(0x7f800000);
        goto signed_result;
    }
    if (magnitude < -46) {
        result_bits = 0U;
        goto signed_result;
    }
    while (lower < upper) {
        uint32_t middle = lower + (upper - lower) / 2U;
        int comparison = decimal_compare_midpoint(
            digits, digit_count, magnitude, sticky, middle);
        if (comparison < 0 ||
            (comparison == 0 && (middle & 1U) == 0U)) {
            upper = middle;
        } else {
            lower = middle + 1U;
        }
    }
    result_bits = lower;
signed_result:
    if (negative) result_bits |= UINT32_C(0x80000000);
    memcpy(&result, &result_bits, sizeof(result));
    return (pphp_float)result;
}
#endif

#if !PPHP_USE_DOUBLE
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this software is freely
 * granted, provided that this notice is preserved.
 * ====================================================
 *
 * Float-only core derived from FreeBSD msun's e_powf.c (Sun fdlibm).
 * The split high/low terms keep y*log2(x) accurate without binary64.
 */
static float pphp_float_power_positive(float x, float y) {
    static const float bp[2] = {1.0f, 1.5f};
    static const float dp_h[2] = {0.0f, 5.84960938e-01f};
    static const float dp_l[2] = {0.0f, 1.56322085e-06f};
    const float half = 0.5f;
    const float third = 3.33333343e-01f;
    const float quarter = 0.25f;
    const float ivln2 = 1.4426950216e+00f;
    const float ivln2_h = 1.4426879883e+00f;
    const float ivln2_l = 7.0526075433e-06f;
    const float cp = 9.6179670095e-01f;
    const float cp_h = 9.6191406250e-01f;
    const float cp_l = -1.1736857402e-04f;
    const float lg2 = 6.9314718246e-01f;
    const float lg2_h = 6.93145752e-01f;
    const float lg2_l = 1.42860654e-06f;
    const float ovt = 4.2995665694e-08f;
    const float l1 = 6.0000002384e-01f;
    const float l2 = 4.2857143283e-01f;
    const float l3 = 3.3333334327e-01f;
    const float l4 = 2.7272811532e-01f;
    const float l5 = 2.3066075146e-01f;
    const float l6 = 2.0697501302e-01f;
    const float p1 = 1.6666667163e-01f;
    const float p2 = -2.7777778450e-03f;
    const float p3 = 6.6137559770e-05f;
    const float p4 = -1.6533901999e-06f;
    const float p5 = 4.1381369442e-08f;
    uint32_t x_bits;
    uint32_t y_bits;
    uint32_t word;
    int exponent_adjustment;
    int interval;
    float t1;
    float t2;
    float high;
    float low;
    float z;
    memcpy(&x_bits, &x, sizeof(x_bits));
    memcpy(&y_bits, &y, sizeof(y_bits));

    if ((y_bits & UINT32_C(0x7fffffff)) > UINT32_C(0x4d000000) ||
        ((y_bits & UINT32_C(0x7fffffff)) >= UINT32_C(0x4b800000) &&
         x_bits >= UINT32_C(0x3f7ffff6) &&
         x_bits <= UINT32_C(0x3f800007))) {
        if ((y_bits & UINT32_C(0x7fffffff)) > UINT32_C(0x4d000000)) {
            if (x_bits < UINT32_C(0x3f7ffff6)) {
                return (y_bits & UINT32_C(0x80000000)) != 0U
                           ? integer_float_infinity() : 0.0f;
            }
            if (x_bits > UINT32_C(0x3f800007)) {
                return (y_bits & UINT32_C(0x80000000)) == 0U
                           ? integer_float_infinity() : 0.0f;
            }
        }
        {
            float difference = x - 1.0f;
            float correction = difference * difference *
                (half - difference * (third - difference * quarter));
            float first = ivln2_h * difference;
            float second = difference * ivln2_l - correction * ivln2;
            t1 = first + second;
            memcpy(&word, &t1, sizeof(word));
            word &= UINT32_C(0xfffff000);
            memcpy(&t1, &word, sizeof(t1));
            t2 = second - (t1 - first);
        }
    } else {
        uint32_t normalized_bits = x_bits;
        uint32_t fraction;
        float normalized = x;
        float s;
        float s_high;
        float s_low;
        float t_high;
        float t_low;
        float polynomial;
        float squared;
        float first;
        float second;
        exponent_adjustment = 0;
        if (normalized_bits < UINT32_C(0x00800000)) {
            normalized *= 16777216.0f;
            exponent_adjustment -= 24;
            memcpy(&normalized_bits, &normalized, sizeof(normalized_bits));
        }
        exponent_adjustment += (int)(normalized_bits >> 23U) - 127;
        fraction = normalized_bits & UINT32_C(0x007fffff);
        normalized_bits = fraction | UINT32_C(0x3f800000);
        if (fraction <= UINT32_C(0x1cc471)) {
            interval = 0;
        } else if (fraction < UINT32_C(0x5db3d7)) {
            interval = 1;
        } else {
            interval = 0;
            exponent_adjustment++;
            normalized_bits -= UINT32_C(0x00800000);
        }
        memcpy(&normalized, &normalized_bits, sizeof(normalized));
        first = normalized - bp[interval];
        second = 1.0f / (normalized + bp[interval]);
        s = first * second;
        s_high = s;
        memcpy(&word, &s_high, sizeof(word));
        word &= UINT32_C(0xfffff000);
        memcpy(&s_high, &word, sizeof(s_high));
        word = ((normalized_bits >> 1U) & UINT32_C(0xfffff000)) |
               UINT32_C(0x20000000);
        word += UINT32_C(0x00400000) + ((uint32_t)interval << 21U);
        memcpy(&t_high, &word, sizeof(t_high));
        t_low = normalized - (t_high - bp[interval]);
        s_low = second * ((first - s_high * t_high) - s_high * t_low);
        squared = s * s;
        polynomial = squared * squared *
            (l1 + squared * (l2 + squared * (l3 + squared *
             (l4 + squared * (l5 + squared * l6)))));
        polynomial += s_low * (s_high + s);
        squared = s_high * s_high;
        t_high = 3.0f + squared + polynomial;
        memcpy(&word, &t_high, sizeof(word));
        word &= UINT32_C(0xfffff000);
        memcpy(&t_high, &word, sizeof(t_high));
        t_low = polynomial - ((t_high - 3.0f) - squared);
        first = s_high * t_high;
        second = s_low * t_high + t_low * s;
        high = first + second;
        memcpy(&word, &high, sizeof(word));
        word &= UINT32_C(0xfffff000);
        memcpy(&high, &word, sizeof(high));
        low = second - (high - first);
        {
            float z_high = cp_h * high;
            float z_low = cp_l * high + low * cp + dp_l[interval];
            float adjustment = (float)exponent_adjustment;
            t1 = ((z_high + z_low) + dp_h[interval]) + adjustment;
            memcpy(&word, &t1, sizeof(word));
            word &= UINT32_C(0xfffff000);
            memcpy(&t1, &word, sizeof(t1));
            t2 = z_low - (((t1 - adjustment) - dp_h[interval]) - z_high);
        }
    }

    memcpy(&word, &y, sizeof(word));
    word &= UINT32_C(0xfffff000);
    memcpy(&high, &word, sizeof(high));
    low = (y - high) * t1 + y * t2;
    high *= t1;
    z = low + high;
    memcpy(&word, &z, sizeof(word));
    if ((int32_t)word > INT32_C(0x43000000) ||
        (word == UINT32_C(0x43000000) && low + ovt > z - high)) {
        return integer_float_infinity();
    }
    if ((word & UINT32_C(0x7fffffff)) > UINT32_C(0x43160000) ||
        (word == UINT32_C(0xc3160000) && low <= z - high)) {
        return 0.0f;
    }

    exponent_adjustment = 0;
    {
        uint32_t absolute_word = word & UINT32_C(0x7fffffff);
        int binary_exponent = (int)(absolute_word >> 23U) - 127;
        if (absolute_word > UINT32_C(0x3f000000)) {
            uint32_t rounded =
                word + (UINT32_C(0x00800000) >>
                        (unsigned)(binary_exponent + 1));
            binary_exponent =
                (int)((rounded & UINT32_C(0x7fffffff)) >> 23U) - 127;
            word = rounded & ~(UINT32_C(0x007fffff) >>
                               (unsigned)binary_exponent);
            memcpy(&z, &word, sizeof(z));
            exponent_adjustment = (int)(
                ((rounded & UINT32_C(0x007fffff)) |
                 UINT32_C(0x00800000)) >>
                (unsigned)(23 - binary_exponent));
            if ((int32_t)rounded < 0) exponent_adjustment = -exponent_adjustment;
            high -= z;
        }
    }
    z = low + high;
    memcpy(&word, &z, sizeof(word));
    word &= UINT32_C(0xffff8000);
    memcpy(&z, &word, sizeof(z));
    {
        float first = z * lg2_h;
        float second = (low - (z - high)) * lg2 + z * lg2_l;
        float sum = first + second;
        float tail = second - (sum - first);
        float squared = sum * sum;
        float approximation = sum - squared *
            (p1 + squared * (p2 + squared * (p3 + squared *
             (p4 + squared * p5))));
        float correction =
            (sum * approximation) / (approximation - 2.0f) -
            (tail + sum * tail);
        z = 1.0f - (correction - sum);
    }
    memcpy(&word, &z, sizeof(word));
    word += (uint32_t)exponent_adjustment << 23U;
    if (((int32_t)word >> 23) <= 0) {
        return PPHP_FLOAT_MATH(ldexp)(z, exponent_adjustment);
    }
    memcpy(&z, &word, sizeof(z));
    return z;
}
#endif

pphp_float pphp_float_power(pphp_float base, pphp_float exponent) {
#if PPHP_USE_DOUBLE
    return PPHP_FLOAT_MATH(pow)(base, exponent);
#else
    uint32_t base_bits;
    uint32_t exponent_bits;
    uint32_t absolute_base_bits;
    uint32_t absolute_exponent_bits;
    uint32_t exponent_field;
    int exponent_is_integer = 0;
    int exponent_is_odd = 0;
    float absolute_base;
    float result;
    memcpy(&base_bits, &base, sizeof(base_bits));
    memcpy(&exponent_bits, &exponent, sizeof(exponent_bits));
    absolute_base_bits = base_bits & UINT32_C(0x7fffffff);
    absolute_exponent_bits = exponent_bits & UINT32_C(0x7fffffff);

    /* pow(1, NaN) and pow(x, +/-0) are both exactly one. */
    if (absolute_exponent_bits == 0U || base_bits == UINT32_C(0x3f800000)) {
        return 1.0f;
    }
    if (absolute_base_bits > UINT32_C(0x7f800000) ||
        absolute_exponent_bits > UINT32_C(0x7f800000)) {
        uint32_t nan_bits = UINT32_C(0x7fc00000);
        memcpy(&result, &nan_bits, sizeof(result));
        return result;
    }
    if (absolute_base_bits == UINT32_C(0x3f800000) &&
        absolute_exponent_bits == UINT32_C(0x7f800000)) {
        return 1.0f;
    }

    exponent_field = absolute_exponent_bits >> 23U;
    if (exponent_field >= 127U && exponent_field < 255U) {
        if (exponent_field > 150U) {
            exponent_is_integer = 1;
        } else {
            unsigned shift = 150U - exponent_field;
            uint32_t significand =
                (absolute_exponent_bits & UINT32_C(0x007fffff)) |
                UINT32_C(0x00800000);
            uint32_t fractional_mask = (UINT32_C(1) << shift) - 1U;
            if ((significand & fractional_mask) == 0U) {
                exponent_is_integer = 1;
                exponent_is_odd =
                    (int)((significand >> shift) & UINT32_C(1));
            }
        }
    }

    if (absolute_exponent_bits == UINT32_C(0x7f800000)) {
        if (absolute_base_bits == UINT32_C(0x3f800000)) return 1.0f;
        if ((absolute_base_bits > UINT32_C(0x3f800000)) ==
            ((exponent_bits & UINT32_C(0x80000000)) == 0U)) {
            uint32_t infinity_bits = UINT32_C(0x7f800000);
            memcpy(&result, &infinity_bits, sizeof(result));
            return result;
        }
        return 0.0f;
    }

    if (absolute_base_bits == 0U ||
        absolute_base_bits == UINT32_C(0x7f800000)) {
        uint32_t result_bits;
        int reciprocal = (exponent_bits & UINT32_C(0x80000000)) != 0U;
        if ((absolute_base_bits == 0U) == reciprocal) {
            result_bits = UINT32_C(0x7f800000);
        } else {
            result_bits = 0U;
        }
        if ((base_bits & UINT32_C(0x80000000)) != 0U &&
            exponent_is_odd) {
            result_bits |= UINT32_C(0x80000000);
        }
        memcpy(&result, &result_bits, sizeof(result));
        return result;
    }

    if ((base_bits & UINT32_C(0x80000000)) != 0U &&
        !exponent_is_integer) {
        uint32_t nan_bits = UINT32_C(0x7fc00000);
        memcpy(&result, &nan_bits, sizeof(result));
        return result;
    }
    memcpy(&absolute_base, &absolute_base_bits, sizeof(absolute_base));
    result = pphp_float_power_positive(absolute_base, exponent);
    return ((base_bits & UINT32_C(0x80000000)) != 0U && exponent_is_odd)
               ? -result : result;
#endif
}
#endif

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

int pphp_format_integer(char *buffer, size_t capacity, pphp_int value) {
    char reversed[sizeof(pphp_int) * 3U + 1U];
#if PPHP_INT64
    uint64_t magnitude;
#else
    uint32_t magnitude;
#endif
    size_t length = 0U;
    size_t output = 0U;
    if (value < 0) {
#if PPHP_INT64
        magnitude = (uint64_t)(-(value + 1));
#else
        magnitude = (uint32_t)(-(value + 1));
#endif
        magnitude++;
    } else {
#if PPHP_INT64
        magnitude = (uint64_t)value;
#else
        magnitude = (uint32_t)value;
#endif
    }
    do {
        reversed[length++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U);
    if (value < 0) reversed[length++] = '-';
    if (capacity <= length) return -1;
    while (length != 0U) buffer[output++] = reversed[--length];
    buffer[output] = '\0';
    return (int)output;
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
                *result = pv_float(pphp_float_power(
                    (pphp_float)left, (pphp_float)right));
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

static int string_integer_exact(const char *text, size_t length,
                                int require_complete, pphp_int *result,
                                int *out_of_range, size_t *digits_start_out,
                                size_t *digits_length_out, int *negative_out,
                                size_t *consumed) {
    size_t position = 0U;
    size_t digits_start;
    int negative = 0;
    int digits = 0;
    int in_range = 1;
    int magnitude_valid = 1;
    uint64_t magnitude = 0U;
    uint64_t limit;
    *out_of_range = 0;
    *digits_start_out = 0U;
    *digits_length_out = 0U;
    *negative_out = 0;
    *consumed = 0U;
    while (position < length &&
           (text[position] == ' ' || text[position] == '\t' ||
            text[position] == '\n' || text[position] == '\r')) position++;
    if (position < length &&
        (text[position] == '+' || text[position] == '-')) {
        negative = text[position++] == '-';
    }
    digits_start = position;
    limit = (uint64_t)PPHP_INT_MAXIMUM + (negative ? 1U : 0U);
    while (position < length && text[position] >= '0' &&
           text[position] <= '9') {
        unsigned digit = (unsigned)(text[position++] - '0');
        if (magnitude_valid) {
            if (magnitude > (UINT64_MAX - digit) / 10U) {
                magnitude_valid = 0;
            } else {
                magnitude = magnitude * 10U + digit;
            }
        }
        if (in_range && (!magnitude_valid || magnitude > limit)) in_range = 0;
        digits++;
    }
    if (digits == 0 ||
        (position < length &&
         (text[position] == '.' || text[position] == 'e' ||
          text[position] == 'E'))) return 0;
    while (position < length &&
           (text[position] == ' ' || text[position] == '\t' ||
            text[position] == '\n' || text[position] == '\r')) position++;
    *consumed = position;
    if (require_complete && position != length) return 0;
    *digits_start_out = digits_start;
    *digits_length_out = (size_t)digits;
    *negative_out = negative;
    if (!in_range) {
        *out_of_range = 1;
        return 0;
    }
    if (negative && magnitude == (uint64_t)PPHP_INT_MAXIMUM + 1U) {
        *result = PPHP_INT_MINIMUM;
    } else {
        pphp_int integer = (pphp_int)magnitude;
        *result = negative ? -integer : integer;
    }
    return 1;
}

#if PPHP_ENABLE_FLOAT
static int floating_string_number(const char *text, size_t length,
                                  pphp_float *number, int *is_integer,
                                  int require_complete, size_t *consumed) {
    size_t i = 0U;
    size_t number_start;
    size_t number_end;
    int digits = 0;
    int fraction = 0;
    *consumed = 0U;
    while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' ||
                          text[i] == '\r')) {
        i++;
    }
    number_start = i;
    if (i < length && (text[i] == '+' || text[i] == '-')) {
        i++;
    }
    while (i < length && text[i] >= '0' && text[i] <= '9') {
        i++;
        digits++;
    }
    if (i < length && text[i] == '.') {
#if PPHP_ENABLE_FLOAT
        fraction = 1;
        i++;
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            i++;
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
            i++;
        }
        while (i < length && text[i] >= '0' && text[i] <= '9') {
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
    number_end = i;
    while (i < length &&
           (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' ||
            text[i] == '\r')) {
        i++;
    }
    *consumed = i;
    if (require_complete && i != length) return 0;
    *number = pphp_decimal_to_float(text + number_start,
                                    number_end - number_start);
    *is_integer = !fraction;
    return 1;
}
#endif

static int string_numeric(const char *text, size_t length,
                          int require_complete, pphp_numeric *numeric) {
    int out_of_range;
    size_t digits_start;
    size_t digits_length;
    int negative;
    size_t consumed = 0U;
    numeric->string_status = PPHP_NUMERIC_STRING_INVALID;
    if (string_integer_exact(text, length, require_complete,
                             &numeric->integer, &out_of_range, &digits_start,
                             &digits_length, &negative, &consumed)) {
        numeric->number = (pphp_float)numeric->integer;
        numeric->is_integer = 1;
        numeric->integer_exact = 1;
        numeric->string_status = consumed < length
                                     ? PPHP_NUMERIC_STRING_TRAILING
                                     : PPHP_NUMERIC_STRING_EXACT;
        return 1;
    }
#if PPHP_ENABLE_FLOAT
    if (out_of_range) {
        numeric->number = pphp_integer_digits_to_float(
            text + digits_start, digits_length, 10U);
        if (negative) numeric->number = -numeric->number;
        numeric->is_integer = 0;
        numeric->integer_out_of_range = 1;
        numeric->string_status = consumed < length
                                     ? PPHP_NUMERIC_STRING_TRAILING
                                     : PPHP_NUMERIC_STRING_EXACT;
        return 1;
    }
    if (!floating_string_number(text, length, &numeric->number,
                                &numeric->is_integer,
                                require_complete, &consumed)) return 0;
    if (out_of_range) {
        numeric->is_integer = 0;
        numeric->integer_out_of_range = 1;
    }
    numeric->integer_exact = 0;
    numeric->string_status = consumed < length
                                 ? PPHP_NUMERIC_STRING_TRAILING
                                 : PPHP_NUMERIC_STRING_EXACT;
    return 1;
#else
    if (out_of_range) {
        numeric->is_integer = -1;
        numeric->string_status = consumed < length
                                     ? PPHP_NUMERIC_STRING_TRAILING
                                     : PPHP_NUMERIC_STRING_EXACT;
    }
    return 0;
#endif
}

int pv_to_numeric(pvalue value, int require_complete, pphp_numeric *numeric) {
    memset(numeric, 0, sizeof(*numeric));
    switch ((pvalue_type)value.type) {
        case PT_INT:
            numeric->number = (pphp_float)value.as.i;
            numeric->integer = value.as.i;
            numeric->is_integer = 1;
            numeric->integer_exact = 1;
            return 1;
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT:
            numeric->number = value.as.f;
            return 1;
#endif
        case PT_TRUE:
            numeric->number = (pphp_float)1;
            numeric->integer = 1;
            numeric->is_integer = 1;
            numeric->integer_exact = 1;
            return 1;
        case PT_FALSE:
        case PT_NULL:
            numeric->is_integer = 1;
            numeric->integer_exact = 1;
            return 1;
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return string_numeric(ps_data(string), string->length,
                                  require_complete, numeric);
        }
        default:
            return 0;
    }
}

int pv_to_number(pvalue value, pphp_float *number, int *is_integer) {
    pphp_numeric numeric;
    int converted = pv_to_numeric(value, 1, &numeric);
    *number = numeric.number;
    *is_integer = numeric.is_integer;
    return converted;
}

int pv_to_number_prefix(pvalue value, pphp_float *number, int *is_integer) {
    pphp_numeric numeric;
    int converted = pv_to_numeric(value, value.type != PT_STRING, &numeric);
    *number = numeric.number;
    *is_integer = numeric.is_integer;
    return converted;
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
            length = pphp_format_integer(buffer, sizeof(buffer), value.as.i);
            return length < 0 ? NULL : ps_new(buffer, (size_t)length);
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT:
            length = pphp_format_float(buffer, sizeof(buffer), value.as.f,
                                       'g', 14);
            return length < 0 ? NULL : ps_new(buffer, (size_t)length);
#endif
        case PT_STRING:
            return ps_new(ps_data((const pstring *)value.as.gc),
                          ((const pstring *)value.as.gc)->length);
        case PT_ARRAY:
            return ps_new("Array", 5U);
        default:
            return NULL;
    }
}

int pv_binary_operation(pv_operation operation, pvalue left, pvalue right,
                        pvalue *result, const char **error,
                        unsigned *non_numeric_operands) {
    pphp_numeric left_numeric = {0};
    pphp_numeric right_numeric = {0};
    pphp_float a;
    pphp_float b;
    int left_converted;
    int right_converted;
    if (non_numeric_operands != NULL) *non_numeric_operands = 0U;
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
        memcpy(bytes, ps_data(left_string), left_string->length);
        memcpy(bytes + left_string->length, ps_data(right_string), right_string->length);
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
    left_converted = pv_to_numeric(left, left.type != PT_STRING,
                                   &left_numeric);
    right_converted = pv_to_numeric(right, right.type != PT_STRING,
                                    &right_numeric);
    if (non_numeric_operands != NULL) {
        if (left.type == PT_STRING &&
            left_numeric.string_status != PPHP_NUMERIC_STRING_EXACT) {
            *non_numeric_operands |= 1U;
        }
        if (right.type == PT_STRING &&
            right_numeric.string_status != PPHP_NUMERIC_STRING_EXACT) {
            *non_numeric_operands |= 2U;
        }
    }
    if (!left_converted || !right_converted) {
        *error = left_numeric.is_integer < 0 || right_numeric.is_integer < 0
                     ? "integer overflow requires float support"
                     : "unsupported operand types";
        return 0;
    }
    a = left_numeric.number;
    b = right_numeric.number;
#if PPHP_ENABLE_FLOAT
    if (left_numeric.integer_exact && right_numeric.integer_exact) {
        return integer_binary_float(operation, left_numeric.integer,
                                    right_numeric.integer, result, error);
    }
#endif
    switch (operation) {
        case PV_ADD:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a + b, left_numeric.is_integer &&
                                              right_numeric.is_integer);
#else
            if (!pphp_integer_add(a, b, &a)) goto integer_overflow;
            *result = pv_int(a);
#endif
            return 1;
        case PV_SUB:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a - b, left_numeric.is_integer &&
                                              right_numeric.is_integer);
#else
            if (!pphp_integer_subtract(a, b, &a)) goto integer_overflow;
            *result = pv_int(a);
#endif
            return 1;
        case PV_MUL:
#if PPHP_ENABLE_FLOAT
            *result = numeric_result(a * b, left_numeric.is_integer &&
                                              right_numeric.is_integer);
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
            *result = numeric_result(
                a / b, left_numeric.is_integer && right_numeric.is_integer &&
                           PPHP_FLOAT_MATH(fmod)(a, b) == (pphp_float)0);
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
                if (!pphp_numeric_to_integer(&left_numeric, 0,
                                             &left_integer) ||
                    !pphp_numeric_to_integer(&right_numeric, 0,
                                             &right_integer)) {
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
            *result = numeric_result(pphp_float_power(a, b), 0);
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
            if (!pphp_numeric_to_integer(&left_numeric, 0,
                                         &left_integer) ||
                !pphp_numeric_to_integer(&right_numeric, 0,
                                         &right_integer)) {
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

int pphp_numeric_to_integer(const pphp_numeric *numeric, int exact,
                            pphp_int *result) {
    if (numeric->integer_out_of_range) return 0;
    if (numeric->integer_exact) {
        *result = numeric->integer;
        return 1;
    }
    return pphp_number_to_integer(numeric->number, exact, result);
}

int pv_compare(pvalue left, pvalue right, int strict, int *result,
               const char **error) {
    pphp_numeric left_numeric;
    pphp_numeric right_numeric;
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
    if (pv_to_numeric(left, 1, &left_numeric) &&
        pv_to_numeric(right, 1, &right_numeric)) {
        if (left_numeric.integer_exact && right_numeric.integer_exact) {
            *result = (left_numeric.integer > right_numeric.integer) -
                      (left_numeric.integer < right_numeric.integer);
        } else {
            *result = (left_numeric.number > right_numeric.number) -
                      (left_numeric.number < right_numeric.number);
        }
        return 1;
    }
    if (left.type == PT_STRING && right.type == PT_STRING) {
        const pstring *ls = (const pstring *)left.as.gc;
        const pstring *rs = (const pstring *)right.as.gc;
        size_t shortest = ls->length < rs->length ? ls->length : rs->length;
        int compared = memcmp(ps_data(ls), ps_data(rs), shortest);
        *result = compared != 0 ? (compared > 0 ? 1 : -1)
                                : ((ls->length > rs->length) - (ls->length < rs->length));
        return 1;
    }
    *result = (left.type > right.type) - (left.type < right.type);
    return 1;
}
