#include "float_format.h"

#include <string.h>

#ifndef PPHP_DEVICE_FLOAT_FORMAT
#if defined(PPHP_HOST)
#define PPHP_DEVICE_FLOAT_FORMAT 0
#else
#define PPHP_DEVICE_FLOAT_FORMAT 1
#endif
#endif

#if !PPHP_DEVICE_FLOAT_FORMAT
#include <stdio.h>
#endif

#define FLOAT_FORMAT_MAX_PRECISION 64

#if PPHP_DEVICE_FLOAT_FORMAT

#define BIG_INTEGER_BASE 10000U
#define BIG_INTEGER_DIGITS 4U
#if PPHP_USE_DOUBLE
#define BIG_INTEGER_LIMBS 200U
#define EXACT_DECIMAL_CAPACITY 800U
typedef uint64_t float_significand;
#else
#define BIG_INTEGER_LIMBS 32U
#define EXACT_DECIMAL_CAPACITY 128U
typedef uint32_t float_significand;
#endif

typedef struct big_integer {
    uint32_t limbs[BIG_INTEGER_LIMBS];
    size_t used;
} big_integer;

typedef struct exact_decimal {
    char bytes[EXACT_DECIMAL_CAPACITY];
    size_t length;
    int exponent;
} exact_decimal;

typedef struct decimal_digits {
    char bytes[PPHP_FLOAT_FORMAT_BUFFER_SIZE];
    size_t length;
    int exponent;
} decimal_digits;

typedef struct float_output {
    char *data;
    size_t capacity;
    size_t length;
} float_output;

static int output_character(float_output *output, char byte) {
    if (output->length + 1U >= output->capacity) return 0;
    output->data[output->length++] = byte;
    return 1;
}

static int output_bytes(float_output *output, const char *bytes,
                        size_t length) {
    if (length >= output->capacity - output->length) return 0;
    if (length != 0U) memcpy(output->data + output->length, bytes, length);
    output->length += length;
    return 1;
}

static void big_integer_init(big_integer *integer,
                             float_significand value) {
    integer->used = 0U;
    while (value != 0U) {
        integer->limbs[integer->used++] =
            (uint32_t)(value % (float_significand)BIG_INTEGER_BASE);
        value /= (float_significand)BIG_INTEGER_BASE;
    }
}

static int big_integer_multiply(big_integer *integer, uint32_t factor) {
    uint32_t carry = 0U;
    size_t i;
    for (i = 0U; i < integer->used; i++) {
        uint32_t product = integer->limbs[i] * factor + carry;
        integer->limbs[i] = product % BIG_INTEGER_BASE;
        carry = product / BIG_INTEGER_BASE;
    }
    if (carry != 0U) {
        if (integer->used == BIG_INTEGER_LIMBS) return 0;
        integer->limbs[integer->used++] = carry;
    }
    return 1;
}

static size_t append_unsigned_digits(char *output, size_t position,
                                     uint32_t value, unsigned minimum) {
    char reversed[10];
    size_t length = 0U;
    do {
        reversed[length++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (length < (size_t)minimum) reversed[length++] = '0';
    while (length != 0U) output[position++] = reversed[--length];
    return position;
}

static int big_integer_decimal(const big_integer *integer, char *output,
                               size_t capacity, size_t *length) {
    size_t position = 0U;
    size_t limb;
    if (integer->used == 0U) return 0;
    limb = integer->used - 1U;
    position = append_unsigned_digits(output, position,
                                      integer->limbs[limb], 1U);
    while (limb != 0U) {
        limb--;
        position = append_unsigned_digits(output, position,
                                          integer->limbs[limb],
                                          BIG_INTEGER_DIGITS);
    }
    if (position > capacity) return 0;
    *length = position;
    return 1;
}

/* Return -1 on failure, 0 for finite nonzero, 1 for zero, 2 for infinity,
 * and 3 for NaN. */
static int exact_decimal_from_float(pphp_float value, exact_decimal *decimal,
                                    int *negative) {
    float_significand significand;
    unsigned exponent_bits;
    int binary_exponent;
    int decimal_scale = 0;
    big_integer integer;
    size_t i;
#if PPHP_USE_DOUBLE
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    *negative = (bits >> 63U) != 0U;
    exponent_bits = (unsigned)((bits >> 52U) & UINT64_C(0x7ff));
    significand = bits & UINT64_C(0x000fffffffffffff);
    if (exponent_bits == 0x7ffU) return significand == 0U ? 2 : 3;
    if (exponent_bits == 0U) {
        if (significand == 0U) return 1;
        binary_exponent = -1074;
    } else {
        significand |= UINT64_C(0x0010000000000000);
        binary_exponent = (int)exponent_bits - 1023 - 52;
    }
#else
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    *negative = (bits >> 31U) != 0U;
    exponent_bits = (bits >> 23U) & UINT32_C(0xff);
    significand = bits & UINT32_C(0x007fffff);
    if (exponent_bits == 0xffU) return significand == 0U ? 2 : 3;
    if (exponent_bits == 0U) {
        if (significand == 0U) return 1;
        binary_exponent = -149;
    } else {
        significand |= UINT32_C(0x00800000);
        binary_exponent = (int)exponent_bits - 127 - 23;
    }
#endif
    big_integer_init(&integer, significand);
    if (binary_exponent >= 0) {
        for (i = 0U; i < (size_t)binary_exponent; i++) {
            if (!big_integer_multiply(&integer, 2U)) return -1;
        }
    } else {
        decimal_scale = -binary_exponent;
        for (i = 0U; i < (size_t)decimal_scale; i++) {
            if (!big_integer_multiply(&integer, 5U)) return -1;
        }
    }
    if (!big_integer_decimal(&integer, decimal->bytes,
                             sizeof(decimal->bytes), &decimal->length)) {
        return -1;
    }
    while (decimal_scale != 0 && decimal->length > 1U &&
           decimal->bytes[decimal->length - 1U] == '0') {
        decimal->length--;
        decimal_scale--;
    }
    decimal->exponent = (int)decimal->length - decimal_scale - 1;
    return 0;
}

static int should_round_up(const exact_decimal *exact, size_t retained) {
    int next;
    size_t i;
    if (retained >= exact->length) return 0;
    next = exact->bytes[retained] - '0';
    if (next > 5) return 1;
    if (next < 5) return 0;
    for (i = retained + 1U; i < exact->length; i++) {
        if (exact->bytes[i] != '0') return 1;
    }
    return retained != 0U &&
           ((exact->bytes[retained - 1U] - '0') & 1) != 0;
}

static int is_exact_half(const exact_decimal *exact, size_t retained) {
    size_t i;
    if (retained >= exact->length || exact->bytes[retained] != '5') return 0;
    for (i = retained + 1U; i < exact->length; i++) {
        if (exact->bytes[i] != '0') return 0;
    }
    return 1;
}

static int rounded_digits(const exact_decimal *exact, size_t count,
                          decimal_digits *digits) {
    size_t copied;
    size_t i;
    if (count == 0U || count > sizeof(digits->bytes)) return 0;
    copied = count < exact->length ? count : exact->length;
    memcpy(digits->bytes, exact->bytes, copied);
    if (count > copied) memset(digits->bytes + copied, '0', count - copied);
    digits->length = count;
    digits->exponent = exact->exponent;
    if (!should_round_up(exact, count)) return 1;
    i = count;
    while (i != 0U && digits->bytes[i - 1U] == '9') {
        digits->bytes[--i] = '0';
    }
    if (i == 0U) {
        digits->bytes[0] = '1';
        digits->exponent++;
    } else {
        digits->bytes[i - 1U]++;
    }
    return 1;
}

static char digit_at(const decimal_digits *digits, int position) {
    if (position < 0 || (size_t)position >= digits->length) return '0';
    return digits->bytes[(size_t)position];
}

static int output_exponent(float_output *output, int exponent) {
    char reversed[8];
    size_t length = 0U;
    unsigned magnitude;
    if (!output_character(output, exponent < 0 ? '-' : '+')) return 0;
    magnitude = exponent < 0 ? (unsigned)(-exponent) : (unsigned)exponent;
    do {
        reversed[length++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U);
    if (length == 1U) reversed[length++] = '0';
    while (length != 0U) {
        if (!output_character(output, reversed[--length])) return 0;
    }
    return 1;
}

static int output_fixed(float_output *output, const decimal_digits *digits,
                        int precision) {
    int position;
    int decimal;
    if (digits->exponent >= 0) {
        for (position = 0; position <= digits->exponent; position++) {
            if (!output_character(output, digit_at(digits, position))) return 0;
        }
    } else if (!output_character(output, '0')) {
        return 0;
    }
    if (precision == 0) return 1;
    if (!output_character(output, '.')) return 0;
    for (decimal = 1; decimal <= precision; decimal++) {
        position = digits->exponent + decimal;
        if (!output_character(output, digit_at(digits, position))) return 0;
    }
    return 1;
}

static int output_scientific(float_output *output,
                             const decimal_digits *digits, int precision) {
    int i;
    if (!output_character(output, digits->bytes[0])) return 0;
    if (precision != 0) {
        if (!output_character(output, '.')) return 0;
        for (i = 1; i <= precision; i++) {
            if (!output_character(output, digit_at(digits, i))) return 0;
        }
    }
    return output_character(output, 'e') &&
           output_exponent(output, digits->exponent);
}

static int format_fixed(float_output *output, const exact_decimal *exact,
                        int precision) {
    decimal_digits digits;
    int count = exact->exponent + precision + 1;
    if (count <= 0) {
        memset(&digits, 0, sizeof(digits));
        digits.bytes[0] = '0';
        digits.length = 1U;
        digits.exponent = exact->exponent;
        if (count == 0 && should_round_up(exact, 0U)) {
            digits.bytes[0] = '1';
            digits.exponent = -precision;
        }
        return output_fixed(output, &digits, precision);
    }
    if (!rounded_digits(exact, (size_t)count, &digits)) return 0;
    return output_fixed(output, &digits, precision);
}

static int format_exponential(float_output *output, const exact_decimal *exact,
                              int precision) {
    decimal_digits digits;
    if (!rounded_digits(exact, (size_t)precision + 1U, &digits)) return 0;
    return output_scientific(output, &digits, precision);
}

static int format_general(float_output *output, const exact_decimal *exact,
                          int precision) {
    decimal_digits digits;
    int significant = precision == 0 ? 1 : precision;
    size_t used;
    int scientific;
    int fractional;
    int position;
    int preserve_tie_zero;
    if (!rounded_digits(exact, (size_t)significant, &digits)) return 0;
    scientific = digits.exponent < -4 || digits.exponent >= significant;
    used = digits.length;
    preserve_tie_zero = is_exact_half(exact, (size_t)significant) &&
                        exact->bytes[(size_t)significant - 1U] == '0';
    while (!preserve_tie_zero && used > 1U &&
           digits.bytes[used - 1U] == '0') {
        used--;
    }
    digits.length = used;
    if (scientific) {
        return output_scientific(output, &digits, (int)used - 1);
    }
    if (digits.exponent >= 0) {
        for (position = 0; position <= digits.exponent; position++) {
            if (!output_character(output, digit_at(&digits, position))) return 0;
        }
        fractional = (int)used - digits.exponent - 1;
    } else {
        if (!output_character(output, '0')) return 0;
        fractional = -digits.exponent - 1 + (int)used;
    }
    if (fractional <= 0) return 1;
    if (!output_character(output, '.')) return 0;
    for (position = 1; position <= fractional; position++) {
        int index = digits.exponent + position;
        if (!output_character(output, digit_at(&digits, index))) return 0;
    }
    return 1;
}

static int format_device(char *buffer, size_t capacity, pphp_float value,
                         char conversion, int precision) {
    float_output output;
    exact_decimal exact;
    int negative;
    int kind;
    if (buffer == NULL || capacity == 0U) return -1;
    output.data = buffer;
    output.capacity = capacity;
    output.length = 0U;
    kind = exact_decimal_from_float(value, &exact, &negative);
    if (kind < 0) return -1;
    if (negative && !output_character(&output, '-')) return -1;
    if (kind == 3) {
        if (!output_bytes(&output, "nan", 3U)) return -1;
    } else if (kind == 2) {
        if (!output_bytes(&output, "inf", 3U)) return -1;
    } else if (kind == 1) {
        decimal_digits zero;
        memset(&zero, 0, sizeof(zero));
        zero.bytes[0] = '0';
        zero.length = 1U;
        if (conversion == 'f') {
            if (!output_fixed(&output, &zero, precision)) return -1;
        } else if (conversion == 'e') {
            if (!output_scientific(&output, &zero, precision)) return -1;
        } else if (!output_character(&output, '0')) {
            return -1;
        }
    } else {
        int ok;
        if (conversion == 'f') {
            ok = format_fixed(&output, &exact, precision);
        } else if (conversion == 'e') {
            ok = format_exponential(&output, &exact, precision);
        } else {
            ok = format_general(&output, &exact, precision);
        }
        if (!ok) return -1;
    }
    output.data[output.length] = '\0';
    return (int)output.length;
}

#endif

int pphp_format_float(char *buffer, size_t capacity, pphp_float value,
                      char conversion, int precision) {
    int selected_precision = precision < 0 ? 6 : precision;
    if (buffer == NULL || capacity == 0U || selected_precision < 0 ||
        selected_precision > FLOAT_FORMAT_MAX_PRECISION ||
        (conversion != 'f' && conversion != 'e' && conversion != 'g')) {
        return -1;
    }
#if PPHP_DEVICE_FLOAT_FORMAT
    return format_device(buffer, capacity, value, conversion,
                         selected_precision);
#else
    {
        char format[16];
        int format_length;
        int length;
        format_length = snprintf(format, sizeof(format), "%%.%d%c",
                                 selected_precision, conversion);
        if (format_length < 0 || (size_t)format_length >= sizeof(format)) {
            return -1;
        }
        length = snprintf(buffer, capacity, format, (double)value);
        if (length < 0 || (size_t)length >= capacity) return -1;
        return length;
    }
#endif
}
