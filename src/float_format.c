#include "float_format.h"

#include <math.h>
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

static double normalize_value(double value, int *exponent) {
    *exponent = 0;
    if (value == 0.0) return 0.0;
    while (value >= 10.0) {
        value *= 0.1;
        (*exponent)++;
    }
    while (value < 1.0) {
        value *= 10.0;
        (*exponent)--;
    }
    return value;
}

static int generate_digits(double value, size_t count,
                           decimal_digits *digits) {
    double normalized;
    size_t i;
    int round_digit;
    if (count == 0U || count + 1U > sizeof(digits->bytes)) return 0;
    normalized = normalize_value(value, &digits->exponent);
    for (i = 0U; i <= count; i++) {
        int digit = (int)normalized;
        if (digit < 0) digit = 0;
        if (digit > 9) digit = 9;
        digits->bytes[i] = (char)('0' + digit);
        normalized = (normalized - (double)digit) * 10.0;
        if (normalized < 0.0) normalized = 0.0;
    }
    round_digit = digits->bytes[count] - '0';
    digits->length = count;
    if (round_digit > 5 ||
        (round_digit == 5 &&
         (normalized > 0.0 ||
          ((digits->bytes[count - 1U] - '0') & 1) != 0))) {
        i = count;
        while (i != 0U && digits->bytes[i - 1U] == '9') {
            digits->bytes[--i] = '0';
        }
        if (i == 0U) {
            digits->bytes[0] = '1';
            for (i = 1U; i < count; i++) digits->bytes[i] = '0';
            digits->exponent++;
        } else {
            digits->bytes[i - 1U]++;
        }
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

static int format_fixed(float_output *output, double value, int precision) {
    decimal_digits digits;
    int exponent;
    size_t count;
    (void)normalize_value(value, &exponent);
    if (exponent + precision + 1 <= 0) {
        memset(&digits, 0, sizeof(digits));
        digits.exponent = exponent;
        if (exponent + precision + 1 == 0) {
            double normalized = normalize_value(value, &exponent);
            if (normalized > 5.0) {
                digits.bytes[0] = '1';
                digits.length = 1U;
                digits.exponent = -precision;
            }
        }
        return output_fixed(output, &digits, precision);
    }
    count = (size_t)(exponent + precision + 1);
    if (!generate_digits(value, count, &digits)) return 0;
    return output_fixed(output, &digits, precision);
}

static int format_exponential(float_output *output, double value,
                              int precision) {
    decimal_digits digits;
    if (!generate_digits(value, (size_t)precision + 1U, &digits)) return 0;
    return output_scientific(output, &digits, precision);
}

static int format_general(float_output *output, double value, int precision) {
    decimal_digits digits;
    int significant = precision == 0 ? 1 : precision;
    size_t used;
    int scientific;
    int fractional;
    int position;
    if (!generate_digits(value, (size_t)significant, &digits)) return 0;
    scientific = digits.exponent < -4 || digits.exponent >= significant;
    used = digits.length;
    while (used > 1U && digits.bytes[used - 1U] == '0') used--;
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

static int format_device(char *buffer, size_t capacity, double value,
                         char conversion, int precision) {
    float_output output;
    int negative;
    if (buffer == NULL || capacity == 0U) return -1;
    output.data = buffer;
    output.capacity = capacity;
    output.length = 0U;
    negative = signbit(value) != 0;
    if (negative && !output_character(&output, '-')) return -1;
    if (isnan(value)) {
        if (!output_bytes(&output, "nan", 3U)) return -1;
    } else if (isinf(value)) {
        if (!output_bytes(&output, "inf", 3U)) return -1;
    } else if (value == 0.0) {
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
        double magnitude = negative ? -value : value;
        int ok;
        if (conversion == 'f') {
            ok = format_fixed(&output, magnitude, precision);
        } else if (conversion == 'e') {
            ok = format_exponential(&output, magnitude, precision);
        } else {
            ok = format_general(&output, magnitude, precision);
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
    return format_device(buffer, capacity, (double)value, conversion,
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
