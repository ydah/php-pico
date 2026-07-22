#include "formatting.h"

#if PPHP_ENABLE_FLOAT
#include "float_format.h"
#endif
#include "parray.h"
#include "value_ops.h"

#include <stdio.h>
#include <string.h>

typedef struct format_buffer {
    char *data;
    size_t length;
    size_t capacity;
} format_buffer;

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int buffer_reserve(format_buffer *buffer, size_t extra) {
    size_t needed;
    size_t capacity;
    char *resized;
    if (extra > PPHP_STR_MAX - buffer->length) return 0;
    needed = buffer->length + extra + 1U;
    if (needed <= buffer->capacity) return 1;
    capacity = buffer->capacity == 0U ? 64U : buffer->capacity;
    while (capacity < needed) {
        size_t next = capacity * 2U;
        if (next <= capacity || next > (size_t)PPHP_STR_MAX + 1U) {
            capacity = (size_t)PPHP_STR_MAX + 1U;
            break;
        }
        capacity = next;
    }
    if (capacity < needed) return 0;
    resized = pphp_realloc(buffer->data, capacity);
    if (resized == NULL) return 0;
    buffer->data = resized;
    buffer->capacity = capacity;
    return 1;
}

static int buffer_append(format_buffer *buffer, const char *bytes,
                         size_t length) {
    if (!buffer_reserve(buffer, length)) return 0;
    if (length != 0U) memcpy(buffer->data + buffer->length, bytes, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int buffer_character(format_buffer *buffer, char value) {
    return buffer_append(buffer, &value, 1U);
}

static int buffer_printf_integer(format_buffer *buffer, const char *format,
                                 long long value) {
    int length = snprintf(NULL, 0U, format, value);
    if (length < 0 || !buffer_reserve(buffer, (size_t)length)) return 0;
    (void)snprintf(buffer->data + buffer->length,
                   buffer->capacity - buffer->length, format, value);
    buffer->length += (size_t)length;
    return 1;
}

static int buffer_printf_unsigned(format_buffer *buffer, const char *format,
                                  unsigned long long value) {
    int length = snprintf(NULL, 0U, format, value);
    if (length < 0 || !buffer_reserve(buffer, (size_t)length)) return 0;
    (void)snprintf(buffer->data + buffer->length,
                   buffer->capacity - buffer->length, format, value);
    buffer->length += (size_t)length;
    return 1;
}

static int buffer_printf_character(format_buffer *buffer, const char *format,
                                   int value) {
    int length = snprintf(NULL, 0U, format, value);
    if (length < 0 || !buffer_reserve(buffer, (size_t)length)) return 0;
    (void)snprintf(buffer->data + buffer->length,
                   buffer->capacity - buffer->length, format, value);
    buffer->length += (size_t)length;
    return 1;
}

static int append_binary(format_buffer *buffer, uint64_t value, unsigned width,
                         int zero_pad, int left_align) {
    char digits[64];
    size_t length = 0U;
    size_t padding;
    do {
        digits[length++] = (char)('0' + (value & 1U));
        value >>= 1U;
    } while (value != 0U);
    padding = width > length ? width - length : 0U;
    if (!left_align) {
        while (padding-- != 0U) {
            if (!buffer_character(buffer, zero_pad ? '0' : ' ')) return 0;
        }
    }
    while (length != 0U) {
        if (!buffer_character(buffer, digits[--length])) return 0;
    }
    if (left_align) {
        while (padding-- != 0U) {
            if (!buffer_character(buffer, ' ')) return 0;
        }
    }
    return 1;
}

static uint64_t unsigned_integer_value(pphp_int value) {
#if PPHP_INT64
    return (uint64_t)value;
#else
    return (uint32_t)value;
#endif
}

#if PPHP_ENABLE_FLOAT
static size_t normalized_float_length(char *bytes, size_t length) {
    size_t position;
    for (position = 0U; position + 3U < length; position++) {
        if (bytes[position] == 'e' &&
            (bytes[position + 1U] == '+' || bytes[position + 1U] == '-')) {
            size_t zero = position + 2U;
            while (zero + 1U < length && bytes[zero] == '0' &&
                   bytes[zero + 1U] >= '0' && bytes[zero + 1U] <= '9') {
                memmove(bytes + zero, bytes + zero + 1U, length - zero);
                length--;
            }
            bytes[length] = '\0';
            break;
        }
    }
    return length;
}

static int append_formatted_float(format_buffer *buffer, pphp_float value,
                                  char conversion, int precision,
                                  unsigned width, int zero_pad,
                                  int left_align) {
    char number[PPHP_FLOAT_FORMAT_BUFFER_SIZE];
    int formatted = pphp_format_float(number, sizeof(number), value,
                                      conversion, precision);
    size_t length;
    size_t padding;
    size_t start = 0U;
    size_t value_start;
    int special;
    if (formatted < 0) return 0;
    length = normalized_float_length(number, (size_t)formatted);
    value_start = length != 0U && number[0] == '-' ? 1U : 0U;
    special = length - value_start == 3U &&
              (memcmp(number + value_start, "inf", 3U) == 0 ||
               memcmp(number + value_start, "nan", 3U) == 0);
    if (special) zero_pad = 0;
    padding = width > length ? (size_t)width - length : 0U;
    if (!left_align && zero_pad && length != 0U && number[0] == '-') {
        if (!buffer_character(buffer, '-')) return 0;
        start = 1U;
        while (padding-- != 0U) {
            if (!buffer_character(buffer, '0')) return 0;
        }
    } else if (!left_align) {
        while (padding-- != 0U) {
            if (!buffer_character(buffer, zero_pad ? '0' : ' ')) return 0;
        }
    }
    if (!buffer_append(buffer, number + start, length - start)) return 0;
    if (left_align) {
        while (padding-- != 0U) {
            if (!buffer_character(buffer, ' ')) return 0;
        }
    }
    return 1;
}
#endif

static int append_formatted(pphp_state *state, const pstring *format,
                            const pvalue *arguments, size_t count,
                            format_buffer *buffer) {
    size_t position = 0U;
    size_t argument = 1U;
    while (position < format->length) {
        size_t start = position;
        char conversion;
        int left_align = 0;
        int zero_pad = 0;
        unsigned width = 0U;
        int precision = -1;
        char native[32];
        size_t native_length = 0U;
        pphp_numeric numeric;
        pphp_int integer_value = 0;
        int integer_conversion;
        if (format->data[position] != '%') {
            while (position < format->length && format->data[position] != '%') {
                position++;
            }
            if (!buffer_append(buffer, format->data + start, position - start)) {
                return 0;
            }
            continue;
        }
        position++;
        if (position < format->length && format->data[position] == '%') {
            if (!buffer_character(buffer, '%')) return 0;
            position++;
            continue;
        }
        while (position < format->length &&
               (format->data[position] == '-' || format->data[position] == '0')) {
            if (format->data[position] == '-') left_align = 1;
            else zero_pad = 1;
            position++;
        }
        while (position < format->length && format->data[position] >= '0' &&
               format->data[position] <= '9') {
            width = width * 10U + (unsigned)(format->data[position] - '0');
            if (width > PPHP_STR_MAX) return 0;
            position++;
        }
        if (position < format->length && format->data[position] == '.') {
            precision = 0;
            position++;
            while (position < format->length && format->data[position] >= '0' &&
                   format->data[position] <= '9') {
                precision = precision * 10 + (format->data[position] - '0');
                if (precision > 64) return 0;
                position++;
            }
        }
        if (position >= format->length || argument >= count) return 0;
        conversion = format->data[position++];
        if (conversion == 's') {
            pstring *string = pv_to_string(arguments[argument++]);
            size_t output_length;
            size_t padding;
            if (string == NULL) return 0;
            output_length = precision >= 0 && (size_t)precision < string->length
                                ? (size_t)precision
                                : string->length;
            padding = width > output_length ? width - output_length : 0U;
            while (!left_align && padding-- != 0U) {
                if (!buffer_character(buffer, ' ')) {
                    ps_destroy(string);
                    return 0;
                }
            }
            if (!buffer_append(buffer, string->data, output_length)) {
                ps_destroy(string);
                return 0;
            }
            if (left_align) {
                padding = width > output_length ? width - output_length : 0U;
                while (padding-- != 0U) {
                    if (!buffer_character(buffer, ' ')) {
                        ps_destroy(string);
                        return 0;
                    }
                }
            }
            ps_destroy(string);
            continue;
        }
        if (!pv_to_numeric(arguments[argument], 1, &numeric)) return 0;
        integer_conversion = pphp_numeric_to_integer(&numeric, 0,
                                                     &integer_value);
        if (!integer_conversion && conversion != 'f' && conversion != 'e' &&
            conversion != 'g') return 0;
        argument++;
        if (conversion == 'b') {
            if (!append_binary(buffer, unsigned_integer_value(integer_value),
                               width, zero_pad, left_align)) return 0;
            continue;
        }
        native[native_length++] = '%';
        if (left_align) native[native_length++] = '-';
        if (zero_pad && !left_align) native[native_length++] = '0';
        if (width != 0U) {
            int written = snprintf(native + native_length,
                                   sizeof(native) - native_length, "%u", width);
            if (written < 0 || (size_t)written >= sizeof(native) - native_length) {
                return 0;
            }
            native_length += (size_t)written;
        }
        if (precision >= 0) {
            int written;
            native[native_length++] = '.';
            written = snprintf(native + native_length,
                               sizeof(native) - native_length, "%d", precision);
            if (written < 0 || (size_t)written >= sizeof(native) - native_length) {
                return 0;
            }
            native_length += (size_t)written;
        }
        if (conversion == 'd') {
            native[native_length++] = 'l';
            native[native_length++] = 'l';
            native[native_length++] = 'd';
            native[native_length] = '\0';
            if (!buffer_printf_integer(buffer, native,
                                       (long long)integer_value)) return 0;
        } else if (conversion == 'u' || conversion == 'x' ||
                   conversion == 'X' || conversion == 'o') {
            native[native_length++] = 'l';
            native[native_length++] = 'l';
            native[native_length++] = conversion;
            native[native_length] = '\0';
            if (!buffer_printf_unsigned(buffer, native,
                                        (unsigned long long)
                                            unsigned_integer_value(
                                                integer_value))) {
                return 0;
            }
        } else if (conversion == 'c') {
            native[native_length++] = 'c';
            native[native_length] = '\0';
            if (!buffer_printf_character(buffer, native,
                                         (int)(unsigned char)integer_value)) {
                return 0;
            }
        }
#if PPHP_ENABLE_FLOAT
        else if (conversion == 'f' || conversion == 'e' ||
                 conversion == 'g') {
            if (!append_formatted_float(buffer, numeric.number, conversion,
                                        precision, width, zero_pad,
                                        left_align)) return 0;
        }
#endif
        else {
            return 0;
        }
    }
    (void)state;
    return 1;
}

static int append_print_value(format_buffer *buffer, pvalue value,
                              unsigned depth) {
    char number[64];
    int length;
    if (depth > 32U) return buffer_append(buffer, "*RECURSION*", 11U);
    switch ((pvalue_type)value.type) {
        case PT_NULL:
        case PT_FALSE:
            return 1;
        case PT_TRUE:
            return buffer_character(buffer, '1');
        case PT_INT:
            length = snprintf(number, sizeof(number), "%lld", (long long)value.as.i);
            return length >= 0 && buffer_append(buffer, number, (size_t)length);
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT:
            length = pphp_format_float(number, sizeof(number), value.as.f,
                                       'g', 14);
            return length >= 0 && buffer_append(buffer, number, (size_t)length);
#endif
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return buffer_append(buffer, string->data, string->length);
        }
        case PT_ARRAY: {
            const parray *array = (const parray *)value.as.gc;
            size_t position = 0U;
            unsigned indent;
            if (!buffer_append(buffer, "Array\n", 6U)) return 0;
            for (indent = 0U; indent < depth; indent++) {
                if (!buffer_append(buffer, "    ", 4U)) return 0;
            }
            if (!buffer_append(buffer, "(\n", 2U)) return 0;
            while (position < array->used) {
                pvalue key;
                pvalue item;
                size_t next;
                if (!pa_entry_at(array, position, &key, &item, &next)) break;
                for (indent = 0U; indent < depth + 1U; indent++) {
                    if (!buffer_append(buffer, "    ", 4U)) return 0;
                }
                if (!buffer_character(buffer, '[')) return 0;
                if (key.type == PT_INT) {
                    length = snprintf(number, sizeof(number), "%lld",
                                      (long long)key.as.i);
                    if (length < 0 || !buffer_append(buffer, number, (size_t)length)) {
                        return 0;
                    }
                } else {
                    const pstring *string = (const pstring *)key.as.gc;
                    if (!buffer_append(buffer, string->data, string->length)) return 0;
                }
                if (!buffer_append(buffer, "] => ", 5U) ||
                    !append_print_value(buffer, item, depth + 1U) ||
                    !buffer_character(buffer, '\n')) return 0;
                pv_release(key);
                pv_release(item);
                position = next;
            }
            for (indent = 0U; indent < depth; indent++) {
                if (!buffer_append(buffer, "    ", 4U)) return 0;
            }
            return buffer_append(buffer, ")\n", 2U);
        }
        default:
            return buffer_append(buffer, pv_type_name((pvalue_type)value.type),
                                 strlen(pv_type_name((pvalue_type)value.type)));
    }
}

int pphp_call_formatting_builtin(pphp_state *state, const pstring *name,
                                 const pvalue *arguments, size_t count,
                                 pvalue *result) {
    format_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    if (name_is(name, "sprintf") || name_is(name, "printf")) {
        pstring *format;
        pstring *string;
        if (count == 0U) {
            pphp_runtime_error(state, 0U, "%.*s() expects a format string",
                               (int)name->length, name->data);
            return -1;
        }
        format = pv_to_string(arguments[0]);
        if (format == NULL ||
            !append_formatted(state, format, arguments, count, &buffer)) {
            ps_destroy(format);
            pphp_free(buffer.data);
            pphp_runtime_error(state, 0U, "invalid or incomplete format string");
            return -1;
        }
        ps_destroy(format);
        if (name_is(name, "printf")) {
            pphp_output(state, buffer.data, buffer.length);
            *result = pv_int((pphp_int)buffer.length);
            pphp_free(buffer.data);
            return 1;
        }
        string = ps_new(buffer.data == NULL ? "" : buffer.data, buffer.length);
        pphp_free(buffer.data);
        if (string == NULL) {
            pphp_runtime_error(state, 0U, "out of memory formatting string");
            return -1;
        }
        *result = pv_heap(PT_STRING, &string->header);
        return 1;
    }
    if (name_is(name, "print_r")) {
        int return_string;
        pstring *string;
        if (count < 1U || count > 2U) {
            pphp_runtime_error(state, 0U, "print_r() expects one or two arguments");
            return -1;
        }
        return_string = count == 2U && pv_is_truthy(arguments[1]);
        if (!append_print_value(&buffer, arguments[0], 0U)) {
            pphp_free(buffer.data);
            pphp_runtime_error(state, 0U, "out of memory formatting value");
            return -1;
        }
        if (!return_string) {
            pphp_output(state, buffer.data, buffer.length);
            *result = pv_bool(1);
            pphp_free(buffer.data);
            return 1;
        }
        string = ps_new(buffer.data == NULL ? "" : buffer.data, buffer.length);
        pphp_free(buffer.data);
        if (string == NULL) {
            pphp_runtime_error(state, 0U, "out of memory formatting value");
            return -1;
        }
        *result = pv_heap(PT_STRING, &string->header);
        return 1;
    }
    return 0;
}
