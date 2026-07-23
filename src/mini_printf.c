#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

typedef struct pphp_format_output {
    char *buffer;
    size_t capacity;
    size_t length;
    int console;
} pphp_format_output;

int stdio_putchar(int byte);

static void format_byte(pphp_format_output *output, char byte) {
    if (output->console) {
        (void)stdio_putchar((unsigned char)byte);
    } else if (output->capacity != 0U &&
               output->length + 1U < output->capacity) {
        output->buffer[output->length] = byte;
    }
    output->length++;
}

static void format_repeat(pphp_format_output *output, char byte, size_t count) {
    while (count-- != 0U) format_byte(output, byte);
}

static void format_bytes(pphp_format_output *output, const char *bytes,
                         size_t length) {
    while (length-- != 0U) format_byte(output, *bytes++);
}

static void format_field(pphp_format_output *output, const char *bytes,
                         size_t length, size_t width, int left_align) {
    size_t padding = width > length ? width - length : 0U;
    if (!left_align) format_repeat(output, ' ', padding);
    format_bytes(output, bytes, length);
    if (left_align) format_repeat(output, ' ', padding);
}

static void format_unsigned(pphp_format_output *output, unsigned long value,
                            unsigned base, int uppercase, int negative,
                            size_t width, int precision, int zero_pad,
                            int left_align) {
    char digits[sizeof(unsigned long) * CHAR_BIT];
    size_t count = 0U;
    size_t zeroes;
    size_t padding;
    if (value != 0UL || precision != 0) {
        do {
            unsigned digit = (unsigned)(value % base);
            digits[count++] = (char)(digit < 10U
                ? (unsigned)'0' + digit
                : (unsigned)(uppercase ? 'A' : 'a') + digit - 10U);
            value /= base;
        } while (value != 0UL);
    }
    zeroes = precision > 0 && (size_t)precision > count
        ? (size_t)precision - count : 0U;
    padding = width > count + zeroes + (negative ? 1U : 0U)
        ? width - count - zeroes - (negative ? 1U : 0U) : 0U;
    if (!left_align && (!zero_pad || precision >= 0)) {
        format_repeat(output, ' ', padding);
        padding = 0U;
    }
    if (negative) format_byte(output, '-');
    if (!left_align && zero_pad) format_repeat(output, '0', padding);
    format_repeat(output, '0', zeroes);
    while (count != 0U) format_byte(output, digits[--count]);
    if (left_align) format_repeat(output, ' ', padding);
}

static int format_variadic(pphp_format_output *output, const char *format,
                           va_list arguments) {
    while (*format != '\0') {
        int left_align;
        int zero_pad;
        size_t width = 0U;
        int precision = -1;
        int length = 0;
        char conversion;
        if (*format != '%') {
            format_byte(output, *format++);
            continue;
        }
        format++;
        if (*format == '%') {
            format_byte(output, *format++);
            continue;
        }
        left_align = *format == '-';
        if (left_align) format++;
        zero_pad = *format == '0';
        if (zero_pad) format++;
        if (*format == '*') {
            int requested = va_arg(arguments, int);
            format++;
            if (requested < 0) {
                left_align = 1;
                width = (size_t)(-(requested + 1)) + 1U;
            } else {
                width = (size_t)requested;
            }
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10U + (size_t)(*format++ - '0');
            }
        }
        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(arguments, int);
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format++ - '0');
                }
            }
            if (precision < 0) precision = -1;
        }
        if (*format == 'z') {
            length = 1;
            format++;
        } else if (*format == 'l') {
            length = 2;
            format++;
            if (*format == 'l') {
#if defined(PPHP_INT64) && PPHP_INT64
                length = 3;
#endif
                format++;
            }
        }
        conversion = *format;
        if (conversion == '\0') break;
        format++;
        if (conversion == 's') {
            const char *string = va_arg(arguments, const char *);
            size_t string_length;
            if (string == NULL) string = "(null)";
            string_length = strlen(string);
            if (precision >= 0 && (size_t)precision < string_length) {
                string_length = (size_t)precision;
            }
            format_field(output, string, string_length, width, left_align);
        } else if (conversion == 'c') {
            char byte = (char)va_arg(arguments, int);
            format_field(output, &byte, 1U, width, left_align);
        } else if (conversion == 'd' || conversion == 'i') {
            long value = length == 0 ? (long)va_arg(arguments, int)
                       : length == 1 ? (long)va_arg(arguments, ptrdiff_t)
                                     : va_arg(arguments, long);
            int negative = value < 0L;
            unsigned long magnitude = negative
                ? (unsigned long)(-(value + 1L)) + 1UL
                : (unsigned long)value;
            format_unsigned(output, magnitude, 10U, 0, negative, width,
                            precision, zero_pad, left_align);
        } else if (conversion == 'u' || conversion == 'x' ||
                   conversion == 'X' || conversion == 'o') {
            unsigned long value =
                length == 0 ? (unsigned long)va_arg(arguments, unsigned int)
              : length == 1 ? (unsigned long)va_arg(arguments, size_t)
                            : va_arg(arguments, unsigned long);
            unsigned base = conversion == 'o' ? 8U
                          : conversion == 'u' ? 10U : 16U;
            format_unsigned(output, value, base, conversion == 'X', 0,
                            width, precision, zero_pad, left_align);
        } else {
            format_byte(output, '%');
            format_byte(output, conversion);
        }
    }
    return output->length <= (size_t)INT_MAX ? (int)output->length : -1;
}

int pphp_vsnprintf(char *buffer, size_t capacity, const char *format,
                   va_list arguments) {
    pphp_format_output output = {buffer, capacity, 0U, 0};
    int result = format_variadic(&output, format, arguments);
    if (capacity != 0U) {
        size_t end = output.length < capacity ? output.length : capacity - 1U;
        buffer[end] = '\0';
    }
    return result;
}

int pphp_snprintf(char *buffer, size_t capacity, const char *format, ...) {
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = pphp_vsnprintf(buffer, capacity, format, arguments);
    va_end(arguments);
    return result;
}

int vprintf(const char *format, va_list arguments) {
    pphp_format_output output = {NULL, 0U, 0U, 1};
    return format_variadic(&output, format, arguments);
}
