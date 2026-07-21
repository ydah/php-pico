#include "json.h"

#include "parray.h"
#include "pclass.h"
#include "value_ops.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct json_buffer {
    char *data;
    size_t length;
    size_t capacity;
} json_buffer;

typedef struct json_parser {
    const char *bytes;
    size_t length;
    size_t position;
    unsigned depth;
    int failed;
} json_parser;

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int reserve(json_buffer *buffer, size_t extra) {
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

static int append(json_buffer *buffer, const char *bytes, size_t length) {
    if (!reserve(buffer, length)) return 0;
    if (length != 0U) memcpy(buffer->data + buffer->length, bytes, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int character(json_buffer *buffer, char byte) {
    return append(buffer, &byte, 1U);
}

static int indentation(json_buffer *buffer, unsigned depth) {
    while (depth-- != 0U) {
        if (!append(buffer, "    ", 4U)) return 0;
    }
    return 1;
}

static int encode_string(json_buffer *buffer, const char *bytes,
                         size_t length) {
    size_t i;
    static const char hex[] = "0123456789abcdef";
    if (!character(buffer, '"')) return 0;
    for (i = 0U; i < length; i++) {
        unsigned char byte = (unsigned char)bytes[i];
        const char *escape = NULL;
        if (byte == '"') escape = "\\\"";
        else if (byte == '\\') escape = "\\\\";
        else if (byte == '/') escape = "\\/";
        else if (byte == '\b') escape = "\\b";
        else if (byte == '\f') escape = "\\f";
        else if (byte == '\n') escape = "\\n";
        else if (byte == '\r') escape = "\\r";
        else if (byte == '\t') escape = "\\t";
        if (escape != NULL) {
            if (!append(buffer, escape, 2U)) return 0;
        } else if (byte < 0x20U) {
            char encoded[6] = {'\\', 'u', '0', '0', hex[byte >> 4U],
                               hex[byte & 0x0fU]};
            if (!append(buffer, encoded, sizeof(encoded))) return 0;
        } else if (!character(buffer, (char)byte)) {
            return 0;
        }
    }
    return character(buffer, '"');
}

static int array_is_list(const parray *array) {
    size_t position = 0U;
    pphp_int expected = 0;
    while (position < array->used) {
        pvalue key;
        pvalue value;
        size_t next;
        if (!pa_entry_at(array, position, &key, &value, &next)) break;
        if (key.type != PT_INT || key.as.i != expected++) {
            pv_release(key);
            pv_release(value);
            return 0;
        }
        pv_release(key);
        pv_release(value);
        position = next;
    }
    return 1;
}

static int encode_value(json_buffer *buffer, pvalue value, int pretty,
                        unsigned depth);

static int encode_array(json_buffer *buffer, const parray *array, int pretty,
                        unsigned depth) {
    int list = array_is_list(array);
    size_t position = 0U;
    size_t emitted = 0U;
    if (depth >= 32U || !character(buffer, list ? '[' : '{')) return 0;
    while (position < array->used) {
        pvalue key;
        pvalue item;
        size_t next;
        if (!pa_entry_at(array, position, &key, &item, &next)) break;
        if (emitted != 0U && !character(buffer, ',')) goto failed;
        if (pretty && (!character(buffer, '\n') ||
                       !indentation(buffer, depth + 1U))) goto failed;
        if (!list) {
            if (key.type == PT_STRING) {
                const pstring *string = (const pstring *)key.as.gc;
                if (!encode_string(buffer, string->data, string->length)) goto failed;
            } else {
                char number[32];
                int length = snprintf(number, sizeof(number), "%lld",
                                      (long long)key.as.i);
                if (length < 0 || !encode_string(buffer, number, (size_t)length)) {
                    goto failed;
                }
            }
            if (!character(buffer, ':') ||
                (pretty && !character(buffer, ' '))) goto failed;
        }
        if (!encode_value(buffer, item, pretty, depth + 1U)) goto failed;
        pv_release(key);
        pv_release(item);
        emitted++;
        position = next;
        continue;
failed:
        pv_release(key);
        pv_release(item);
        return 0;
    }
    if (pretty && emitted != 0U &&
        (!character(buffer, '\n') || !indentation(buffer, depth))) return 0;
    return character(buffer, list ? ']' : '}');
}

static int encode_object(json_buffer *buffer, const pobject *object,
                         int pretty, unsigned depth) {
    const pclass *class_entry = object->class_entry;
    size_t i;
    size_t emitted = 0U;
    if (depth >= 32U || !character(buffer, '{')) return 0;
    for (i = 0U; i < class_entry->property_count; i++) {
        const pproperty *property = &class_entry->properties[i];
        if ((property->flags & PC_STATIC) != 0U) continue;
        if (emitted != 0U && !character(buffer, ',')) return 0;
        if (pretty && (!character(buffer, '\n') ||
                       !indentation(buffer, depth + 1U))) return 0;
        if (!encode_string(buffer, property->name->data, property->name->length) ||
            !character(buffer, ':') || (pretty && !character(buffer, ' ')) ||
            !encode_value(buffer, object->slots[property->slot], pretty,
                          depth + 1U)) return 0;
        emitted++;
    }
    if (pretty && emitted != 0U &&
        (!character(buffer, '\n') || !indentation(buffer, depth))) return 0;
    return character(buffer, '}');
}

static int encode_value(json_buffer *buffer, pvalue value, int pretty,
                        unsigned depth) {
    char number[64];
    int length;
    switch ((pvalue_type)value.type) {
        case PT_NULL: return append(buffer, "null", 4U);
        case PT_FALSE: return append(buffer, "false", 5U);
        case PT_TRUE: return append(buffer, "true", 4U);
        case PT_INT:
            length = snprintf(number, sizeof(number), "%lld", (long long)value.as.i);
            return length >= 0 && append(buffer, number, (size_t)length);
        case PT_FLOAT:
            if (!isfinite((double)value.as.f)) return append(buffer, "null", 4U);
            length = snprintf(number, sizeof(number),
                              PPHP_USE_DOUBLE ? "%.17g" : "%.9g",
                              (double)value.as.f);
            return length >= 0 && append(buffer, number, (size_t)length);
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return encode_string(buffer, string->data, string->length);
        }
        case PT_ARRAY:
            return encode_array(buffer, (const parray *)value.as.gc, pretty,
                                depth);
        case PT_OBJECT:
            return encode_object(buffer, (const pobject *)value.as.gc, pretty,
                                 depth);
        default:
            return append(buffer, "null", 4U);
    }
}

static void skip_space(json_parser *parser) {
    while (parser->position < parser->length) {
        char byte = parser->bytes[parser->position];
        if (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r') break;
        parser->position++;
    }
}

static int hex_digit(char byte) {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}

static int read_hex4(json_parser *parser, uint32_t *value) {
    unsigned i;
    *value = 0U;
    if (parser->position + 4U > parser->length) return 0;
    for (i = 0U; i < 4U; i++) {
        int digit = hex_digit(parser->bytes[parser->position++]);
        if (digit < 0) return 0;
        *value = *value * 16U + (uint32_t)digit;
    }
    return 1;
}

static int append_codepoint(json_buffer *buffer, uint32_t codepoint) {
    char encoded[4];
    size_t length;
    if (codepoint <= 0x7fU) {
        encoded[0] = (char)codepoint;
        length = 1U;
    } else if (codepoint <= 0x7ffU) {
        encoded[0] = (char)(0xc0U | (codepoint >> 6U));
        encoded[1] = (char)(0x80U | (codepoint & 0x3fU));
        length = 2U;
    } else if (codepoint <= 0xffffU) {
        encoded[0] = (char)(0xe0U | (codepoint >> 12U));
        encoded[1] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        encoded[2] = (char)(0x80U | (codepoint & 0x3fU));
        length = 3U;
    } else if (codepoint <= 0x10ffffU) {
        encoded[0] = (char)(0xf0U | (codepoint >> 18U));
        encoded[1] = (char)(0x80U | ((codepoint >> 12U) & 0x3fU));
        encoded[2] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        encoded[3] = (char)(0x80U | (codepoint & 0x3fU));
        length = 4U;
    } else {
        return 0;
    }
    return append(buffer, encoded, length);
}

static pstring *parse_string(json_parser *parser) {
    json_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    if (parser->position >= parser->length ||
        parser->bytes[parser->position++] != '"') return NULL;
    while (parser->position < parser->length) {
        unsigned char byte = (unsigned char)parser->bytes[parser->position++];
        if (byte == '"') {
            pstring *string = ps_new(buffer.data == NULL ? "" : buffer.data,
                                     buffer.length);
            pphp_free(buffer.data);
            return string;
        }
        if (byte < 0x20U) break;
        if (byte != '\\') {
            if (!character(&buffer, (char)byte)) break;
            continue;
        }
        if (parser->position >= parser->length) break;
        byte = (unsigned char)parser->bytes[parser->position++];
        if (byte == '"' || byte == '\\' || byte == '/') {
            if (!character(&buffer, (char)byte)) break;
        } else if (byte == 'b') {
            if (!character(&buffer, '\b')) break;
        } else if (byte == 'f') {
            if (!character(&buffer, '\f')) break;
        } else if (byte == 'n') {
            if (!character(&buffer, '\n')) break;
        } else if (byte == 'r') {
            if (!character(&buffer, '\r')) break;
        } else if (byte == 't') {
            if (!character(&buffer, '\t')) break;
        } else if (byte == 'u') {
            uint32_t codepoint;
            if (!read_hex4(parser, &codepoint)) break;
            if (codepoint >= 0xd800U && codepoint <= 0xdbffU) {
                uint32_t low;
                if (parser->position + 2U > parser->length ||
                    parser->bytes[parser->position] != '\\' ||
                    parser->bytes[parser->position + 1U] != 'u') break;
                parser->position += 2U;
                if (!read_hex4(parser, &low) || low < 0xdc00U ||
                    low > 0xdfffU) break;
                codepoint = 0x10000U + ((codepoint - 0xd800U) << 10U) +
                            (low - 0xdc00U);
            } else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU) {
                break;
            }
            if (!append_codepoint(&buffer, codepoint)) break;
        } else {
            break;
        }
    }
    pphp_free(buffer.data);
    parser->failed = 1;
    return NULL;
}

static pvalue parse_value(json_parser *parser);

static pvalue parse_array(json_parser *parser) {
    parray *array = pa_new(4U);
    if (array == NULL) {
        parser->failed = 1;
        return pv_null();
    }
    parser->position++;
    parser->depth++;
    skip_space(parser);
    if (parser->position < parser->length &&
        parser->bytes[parser->position] == ']') {
        parser->position++;
        parser->depth--;
        return pv_heap(PT_ARRAY, &array->header);
    }
    while (!parser->failed) {
        pvalue value = parse_value(parser);
        if (parser->failed || !pa_push(array, value)) {
            pv_release(value);
            parser->failed = 1;
            break;
        }
        pv_release(value);
        skip_space(parser);
        if (parser->position < parser->length &&
            parser->bytes[parser->position] == ',') {
            parser->position++;
            skip_space(parser);
            continue;
        }
        if (parser->position < parser->length &&
            parser->bytes[parser->position] == ']') {
            parser->position++;
            parser->depth--;
            return pv_heap(PT_ARRAY, &array->header);
        }
        parser->failed = 1;
    }
    pv_release(pv_heap(PT_ARRAY, &array->header));
    return pv_null();
}

static pvalue parse_object(json_parser *parser) {
    parray *array = pa_new(4U);
    if (array == NULL) {
        parser->failed = 1;
        return pv_null();
    }
    parser->position++;
    parser->depth++;
    skip_space(parser);
    if (parser->position < parser->length &&
        parser->bytes[parser->position] == '}') {
        parser->position++;
        parser->depth--;
        return pv_heap(PT_ARRAY, &array->header);
    }
    while (!parser->failed) {
        pstring *key = parse_string(parser);
        pvalue value;
        if (key == NULL) break;
        skip_space(parser);
        if (parser->position >= parser->length ||
            parser->bytes[parser->position++] != ':') {
            ps_destroy(key);
            parser->failed = 1;
            break;
        }
        skip_space(parser);
        value = parse_value(parser);
        if (parser->failed ||
            !pa_set(array, pv_heap(PT_STRING, &key->header), value)) {
            ps_destroy(key);
            pv_release(value);
            parser->failed = 1;
            break;
        }
        pv_release(pv_heap(PT_STRING, &key->header));
        pv_release(value);
        skip_space(parser);
        if (parser->position < parser->length &&
            parser->bytes[parser->position] == ',') {
            parser->position++;
            skip_space(parser);
            continue;
        }
        if (parser->position < parser->length &&
            parser->bytes[parser->position] == '}') {
            parser->position++;
            parser->depth--;
            return pv_heap(PT_ARRAY, &array->header);
        }
        parser->failed = 1;
    }
    pv_release(pv_heap(PT_ARRAY, &array->header));
    return pv_null();
}

static pvalue parse_number(json_parser *parser) {
    size_t start = parser->position;
    size_t length;
    int floating = 0;
    if (parser->bytes[parser->position] == '-') parser->position++;
    if (parser->position >= parser->length) goto invalid;
    if (parser->bytes[parser->position] == '0') {
        parser->position++;
    } else if (parser->bytes[parser->position] >= '1' &&
               parser->bytes[parser->position] <= '9') {
        while (parser->position < parser->length &&
               parser->bytes[parser->position] >= '0' &&
               parser->bytes[parser->position] <= '9') parser->position++;
    } else {
        goto invalid;
    }
    if (parser->position < parser->length &&
        parser->bytes[parser->position] == '.') {
        floating = 1;
        parser->position++;
        if (parser->position >= parser->length ||
            parser->bytes[parser->position] < '0' ||
            parser->bytes[parser->position] > '9') goto invalid;
        while (parser->position < parser->length &&
               parser->bytes[parser->position] >= '0' &&
               parser->bytes[parser->position] <= '9') parser->position++;
    }
    if (parser->position < parser->length &&
        (parser->bytes[parser->position] == 'e' ||
         parser->bytes[parser->position] == 'E')) {
        floating = 1;
        parser->position++;
        if (parser->position < parser->length &&
            (parser->bytes[parser->position] == '+' ||
             parser->bytes[parser->position] == '-')) parser->position++;
        if (parser->position >= parser->length ||
            parser->bytes[parser->position] < '0' ||
            parser->bytes[parser->position] > '9') goto invalid;
        while (parser->position < parser->length &&
               parser->bytes[parser->position] >= '0' &&
               parser->bytes[parser->position] <= '9') parser->position++;
    }
    length = parser->position - start;
    {
        pstring *text = ps_new(parser->bytes + start, length);
        pphp_float number;
        int integer;
        if (text == NULL) goto invalid;
        if (!pv_to_number(pv_heap(PT_STRING, &text->header),
                          &number, &integer)) {
            ps_destroy(text);
            goto invalid;
        }
        ps_destroy(text);
        if (!floating && integer && number >= (pphp_float)INT32_MIN &&
            number <= (pphp_float)INT32_MAX) {
            return pv_int((pphp_int)number);
        }
        return pv_float(number);
    }
invalid:
    parser->failed = 1;
    return pv_null();
}

static pvalue parse_value(json_parser *parser) {
    skip_space(parser);
    if (parser->depth >= 32U || parser->position >= parser->length) {
        parser->failed = 1;
        return pv_null();
    }
    if (parser->bytes[parser->position] == '"') {
        pstring *string = parse_string(parser);
        return string == NULL ? pv_null() : pv_heap(PT_STRING, &string->header);
    }
    if (parser->bytes[parser->position] == '[') return parse_array(parser);
    if (parser->bytes[parser->position] == '{') return parse_object(parser);
    if (parser->position + 4U <= parser->length &&
        memcmp(parser->bytes + parser->position, "true", 4U) == 0) {
        parser->position += 4U;
        return pv_bool(1);
    }
    if (parser->position + 5U <= parser->length &&
        memcmp(parser->bytes + parser->position, "false", 5U) == 0) {
        parser->position += 5U;
        return pv_bool(0);
    }
    if (parser->position + 4U <= parser->length &&
        memcmp(parser->bytes + parser->position, "null", 4U) == 0) {
        parser->position += 4U;
        return pv_null();
    }
    if (parser->bytes[parser->position] == '-' ||
        (parser->bytes[parser->position] >= '0' &&
         parser->bytes[parser->position] <= '9')) return parse_number(parser);
    parser->failed = 1;
    return pv_null();
}

int pphp_call_json_builtin(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    if (name_is(name, "json_encode")) {
        json_buffer buffer;
        int pretty = 0;
        pstring *string;
        memset(&buffer, 0, sizeof(buffer));
        if (count < 1U || count > 2U ||
            (count == 2U && arguments[1].type != PT_INT)) {
            pphp_runtime_error(state, 0U, "json_encode() received invalid arguments");
            return -1;
        }
        if (count == 2U) pretty = (arguments[1].as.i & 128) != 0;
        if (!encode_value(&buffer, arguments[0], pretty, 0U)) {
            pphp_free(buffer.data);
            *result = pv_bool(0);
            return 1;
        }
        string = ps_new(buffer.data == NULL ? "" : buffer.data, buffer.length);
        pphp_free(buffer.data);
        if (string == NULL) {
            pphp_runtime_error(state, 0U, "out of memory encoding JSON");
            return -1;
        }
        *result = pv_heap(PT_STRING, &string->header);
        return 1;
    }
    if (name_is(name, "json_decode")) {
        json_parser parser;
        const pstring *string;
        pvalue value;
        if (count < 1U || count > 2U || arguments[0].type != PT_STRING) {
            pphp_runtime_error(state, 0U, "json_decode() received invalid arguments");
            return -1;
        }
        if (count == 2U && !pv_is_truthy(arguments[1])) {
            pphp_runtime_error(state, 0U,
                               "json_decode() requires associative array mode");
            return -1;
        }
        string = (const pstring *)arguments[0].as.gc;
        memset(&parser, 0, sizeof(parser));
        parser.bytes = string->data;
        parser.length = string->length;
        value = parse_value(&parser);
        skip_space(&parser);
        if (parser.failed || parser.position != parser.length) {
            pv_release(value);
            *result = pv_null();
        } else {
            *result = value;
        }
        return 1;
    }
    return 0;
}
