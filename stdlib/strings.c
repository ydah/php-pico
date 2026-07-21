#include "strings.h"

#include "parray.h"
#include "value_ops.h"

#include <string.h>

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int fail_arguments(pphp_state *state, const pstring *name) {
    pphp_runtime_error(state, 0U, "%.*s() received invalid arguments",
                       (int)name->length, name->data);
    return -1;
}

static int string_result(pphp_state *state, const char *bytes, size_t length,
                         pvalue *result) {
    pstring *string;
    if (length > PPHP_STR_MAX) {
        pphp_runtime_error(state, 0U, "string result exceeds maximum length");
        return -1;
    }
    string = ps_new(bytes, length);
    if (string == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating string result");
        return -1;
    }
    *result = pv_heap(PT_STRING, &string->header);
    return 1;
}

static pstring *argument_string(pphp_state *state, const pstring *name,
                                pvalue value) {
    pstring *string = pv_to_string(value);
    if (string == NULL) {
        pphp_runtime_error(state, 0U, "%.*s() expects string-compatible values",
                           (int)name->length, name->data);
    }
    return string;
}

static int call_substr(pphp_state *state, const pstring *name,
                       const pvalue *arguments, size_t count, pvalue *result) {
    pstring *string;
    int64_t offset;
    int64_t length;
    int64_t size;
    if (count < 2U || count > 3U || arguments[1].type != PT_INT) {
        return fail_arguments(state, name);
    }
    string = argument_string(state, name, arguments[0]);
    if (string == NULL) return -1;
    size = string->length;
    offset = arguments[1].as.i;
    if (offset < 0) offset = size + offset;
    if (offset < 0) offset = 0;
    if (offset > size) offset = size;
    length = size - offset;
    if (count == 3U && arguments[2].type != PT_NULL) {
        if (arguments[2].type != PT_INT) {
            ps_destroy(string);
            return fail_arguments(state, name);
        }
        length = arguments[2].as.i;
        if (length < 0) length = size - offset + length;
        if (length < 0) length = 0;
        if (offset + length > size) length = size - offset;
    }
    {
        int handled = string_result(state, string->data + offset,
                                    (size_t)length, result);
        ps_destroy(string);
        return handled;
    }
}

static int find_bytes(const pstring *haystack, const pstring *needle,
                      size_t start, int reverse, size_t *position) {
    size_t i;
    if (needle->length == 0U) {
        *position = start <= haystack->length ? start : haystack->length;
        return 1;
    }
    if (needle->length > haystack->length ||
        start > (size_t)haystack->length - (size_t)needle->length) return 0;
    if (reverse) {
        i = haystack->length - needle->length;
        for (;;) {
            if (i >= start && memcmp(haystack->data + i, needle->data,
                                     needle->length) == 0) {
                *position = i;
                return 1;
            }
            if (i == 0U || i <= start) break;
            i--;
        }
    } else {
        for (i = start; i + needle->length <= haystack->length; i++) {
            if (memcmp(haystack->data + i, needle->data, needle->length) == 0) {
                *position = i;
                return 1;
            }
        }
    }
    return 0;
}

static int call_search(pphp_state *state, const pstring *name,
                       const pvalue *arguments, size_t count, pvalue *result) {
    pstring *haystack;
    pstring *needle;
    size_t position = 0U;
    size_t offset = 0U;
    int found;
    if (count < 2U || count > 3U ||
        (count == 3U && arguments[2].type != PT_INT)) {
        return fail_arguments(state, name);
    }
    haystack = argument_string(state, name, arguments[0]);
    needle = argument_string(state, name, arguments[1]);
    if (haystack == NULL || needle == NULL) {
        ps_destroy(haystack);
        ps_destroy(needle);
        return -1;
    }
    if (count == 3U) {
        int64_t requested = arguments[2].as.i;
        if (requested < 0) requested = (int64_t)haystack->length + requested;
        if (requested < 0 || requested > haystack->length) {
            ps_destroy(haystack);
            ps_destroy(needle);
            return fail_arguments(state, name);
        }
        offset = (size_t)requested;
    }
    found = find_bytes(haystack, needle, offset, name_is(name, "strrpos"),
                       &position);
    if (name_is(name, "str_contains")) {
        *result = pv_bool(found);
    } else {
        *result = found ? pv_int((pphp_int)position) : pv_bool(0);
    }
    ps_destroy(haystack);
    ps_destroy(needle);
    return 1;
}

static int call_edge_test(pphp_state *state, const pstring *name,
                          const pvalue *arguments, size_t count,
                          pvalue *result) {
    pstring *haystack;
    pstring *needle;
    int matches;
    if (count != 2U) return fail_arguments(state, name);
    haystack = argument_string(state, name, arguments[0]);
    needle = argument_string(state, name, arguments[1]);
    if (haystack == NULL || needle == NULL) {
        ps_destroy(haystack);
        ps_destroy(needle);
        return -1;
    }
    matches = needle->length <= haystack->length;
    if (matches && name_is(name, "str_starts_with")) {
        matches = memcmp(haystack->data, needle->data, needle->length) == 0;
    } else if (matches) {
        matches = memcmp(haystack->data + haystack->length - needle->length,
                         needle->data, needle->length) == 0;
    }
    *result = pv_bool(matches);
    ps_destroy(haystack);
    ps_destroy(needle);
    return 1;
}

static int call_ascii_case(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    pstring *input;
    char *bytes;
    size_t i;
    int upper = name_is(name, "strtoupper");
    if (count != 1U) return fail_arguments(state, name);
    input = argument_string(state, name, arguments[0]);
    if (input == NULL) return -1;
    bytes = pphp_alloc(input->length == 0U ? 1U : input->length);
    if (bytes == NULL) {
        ps_destroy(input);
        pphp_runtime_error(state, 0U, "out of memory changing string case");
        return -1;
    }
    memcpy(bytes, input->data, input->length);
    for (i = 0U; i < input->length; i++) {
        unsigned char c = (unsigned char)bytes[i];
        int selected = (!name_is(name, "ucfirst") &&
                        !name_is(name, "lcfirst")) || i == 0U;
        int make_upper = upper || name_is(name, "ucfirst");
        if (selected && make_upper && c >= 'a' && c <= 'z') bytes[i] = (char)(c - 32U);
        if (selected && !make_upper && c >= 'A' && c <= 'Z') bytes[i] = (char)(c + 32U);
    }
    {
        int handled = string_result(state, bytes, input->length, result);
        pphp_free(bytes);
        ps_destroy(input);
        return handled;
    }
}

static int trim_contains(const pstring *characters, unsigned char c) {
    size_t i;
    if (characters == NULL) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
               c == '\v' || c == 0U;
    }
    for (i = 0U; i < characters->length; i++) {
        if ((unsigned char)characters->data[i] == c) return 1;
    }
    return 0;
}

static int call_trim(pphp_state *state, const pstring *name,
                     const pvalue *arguments, size_t count, pvalue *result) {
    pstring *input;
    pstring *characters = NULL;
    size_t begin = 0U;
    size_t end;
    int left = !name_is(name, "rtrim");
    int right = !name_is(name, "ltrim");
    if (count < 1U || count > 2U) return fail_arguments(state, name);
    input = argument_string(state, name, arguments[0]);
    if (count == 2U) characters = argument_string(state, name, arguments[1]);
    if (input == NULL || (count == 2U && characters == NULL)) {
        ps_destroy(input);
        ps_destroy(characters);
        return -1;
    }
    end = input->length;
    if (left) while (begin < end && trim_contains(characters,
                      (unsigned char)input->data[begin])) begin++;
    if (right) while (end > begin && trim_contains(characters,
                       (unsigned char)input->data[end - 1U])) end--;
    {
        int handled = string_result(state, input->data + begin,
                                    end - begin, result);
        ps_destroy(input);
        ps_destroy(characters);
        return handled;
    }
}

static int call_repeat_or_reverse(pphp_state *state, const pstring *name,
                                  const pvalue *arguments, size_t count,
                                  pvalue *result) {
    pstring *input;
    char *bytes;
    size_t length;
    size_t i;
    if (count < 1U || count > 2U ||
        (name_is(name, "str_repeat") &&
         (count != 2U || arguments[1].type != PT_INT || arguments[1].as.i < 0))) {
        return fail_arguments(state, name);
    }
    input = argument_string(state, name, arguments[0]);
    if (input == NULL) return -1;
    length = name_is(name, "str_repeat")
                 ? (size_t)arguments[1].as.i * input->length
                 : input->length;
    if (length > PPHP_STR_MAX || (input->length != 0U &&
        name_is(name, "str_repeat") &&
        length / input->length != (size_t)arguments[1].as.i)) {
        ps_destroy(input);
        pphp_runtime_error(state, 0U, "string result exceeds maximum length");
        return -1;
    }
    bytes = pphp_alloc(length == 0U ? 1U : length);
    if (bytes == NULL) {
        ps_destroy(input);
        pphp_runtime_error(state, 0U, "out of memory building string");
        return -1;
    }
    if (name_is(name, "str_repeat")) {
        for (i = 0U; i < (size_t)arguments[1].as.i; i++) {
            memcpy(bytes + i * input->length, input->data, input->length);
        }
    } else {
        for (i = 0U; i < length; i++) bytes[i] = input->data[length - i - 1U];
    }
    {
        int handled = string_result(state, bytes, length, result);
        pphp_free(bytes);
        ps_destroy(input);
        return handled;
    }
}

static int compare_ascii(const pstring *left, const pstring *right,
                         size_t limit, int insensitive) {
    size_t i;
    size_t length = left->length < right->length ? left->length : right->length;
    if (length > limit) length = limit;
    for (i = 0U; i < length; i++) {
        unsigned char a = (unsigned char)left->data[i];
        unsigned char b = (unsigned char)right->data[i];
        if (insensitive && a >= 'A' && a <= 'Z') a = (unsigned char)(a + 32U);
        if (insensitive && b >= 'A' && b <= 'Z') b = (unsigned char)(b + 32U);
        if (a != b) return a < b ? -1 : 1;
    }
    if (length == limit) return 0;
    return (left->length > right->length) - (left->length < right->length);
}

static int call_compare(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count, pvalue *result) {
    pstring *left;
    pstring *right;
    size_t limit = SIZE_MAX;
    if (count < 2U || count > 3U ||
        (name_is(name, "strncmp") &&
         (count != 3U || arguments[2].type != PT_INT || arguments[2].as.i < 0))) {
        return fail_arguments(state, name);
    }
    left = argument_string(state, name, arguments[0]);
    right = argument_string(state, name, arguments[1]);
    if (left == NULL || right == NULL) {
        ps_destroy(left);
        ps_destroy(right);
        return -1;
    }
    if (name_is(name, "strncmp")) limit = (size_t)arguments[2].as.i;
    *result = pv_int((pphp_int)compare_ascii(left, right, limit,
                                             name_is(name, "strcasecmp")));
    ps_destroy(left);
    ps_destroy(right);
    return 1;
}

static int call_hex(pphp_state *state, const pstring *name,
                    const pvalue *arguments, size_t count, pvalue *result) {
    static const char digits[] = "0123456789abcdef";
    pstring *input;
    char *bytes;
    size_t i;
    int handled;
    if (count != 1U) return fail_arguments(state, name);
    input = argument_string(state, name, arguments[0]);
    if (input == NULL) return -1;
    if (name_is(name, "bin2hex")) {
        if ((size_t)input->length * 2U > PPHP_STR_MAX) {
            ps_destroy(input);
            pphp_runtime_error(state, 0U, "hex result exceeds maximum length");
            return -1;
        }
        bytes = pphp_alloc((size_t)input->length * 2U + 1U);
        if (bytes == NULL) {
            ps_destroy(input);
            pphp_runtime_error(state, 0U, "out of memory encoding hex");
            return -1;
        }
        for (i = 0U; i < input->length; i++) {
            unsigned char c = (unsigned char)input->data[i];
            bytes[i * 2U] = digits[c >> 4U];
            bytes[i * 2U + 1U] = digits[c & 15U];
        }
        handled = string_result(state, bytes,
                                (size_t)input->length * 2U, result);
    } else {
        if ((input->length & 1U) != 0U) {
            ps_destroy(input);
            return fail_arguments(state, name);
        }
        bytes = pphp_alloc(input->length / 2U + 1U);
        if (bytes == NULL) {
            ps_destroy(input);
            pphp_runtime_error(state, 0U, "out of memory decoding hex");
            return -1;
        }
        for (i = 0U; i < input->length; i += 2U) {
            unsigned values[2];
            size_t j;
            for (j = 0U; j < 2U; j++) {
                unsigned char c = (unsigned char)input->data[i + j];
                if (c >= '0' && c <= '9') values[j] = (unsigned)(c - '0');
                else if (c >= 'a' && c <= 'f') values[j] = (unsigned)(c - 'a') + 10U;
                else if (c >= 'A' && c <= 'F') values[j] = (unsigned)(c - 'A') + 10U;
                else {
                    pphp_free(bytes);
                    ps_destroy(input);
                    return fail_arguments(state, name);
                }
            }
            bytes[i / 2U] = (char)(values[0] * 16U + values[1]);
        }
        handled = string_result(state, bytes, input->length / 2U, result);
    }
    pphp_free(bytes);
    ps_destroy(input);
    return handled;
}

static int call_chr_ord(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count, pvalue *result) {
    if (count != 1U) return fail_arguments(state, name);
    if (name_is(name, "chr")) {
        char byte;
        if (arguments[0].type != PT_INT) return fail_arguments(state, name);
        byte = (char)((unsigned)arguments[0].as.i & 0xffU);
        return string_result(state, &byte, 1U, result);
    }
    {
        pstring *input = argument_string(state, name, arguments[0]);
        if (input == NULL) return -1;
        if (input->length == 0U) {
            ps_destroy(input);
            return fail_arguments(state, name);
        }
        *result = pv_int((pphp_int)(unsigned char)input->data[0]);
        ps_destroy(input);
        return 1;
    }
}

static int call_implode(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count, pvalue *result) {
    pstring *glue;
    const parray *array;
    size_t position = 0U;
    size_t total = 0U;
    size_t items = 0U;
    char *bytes;
    size_t output = 0U;
    if (count != 2U || arguments[1].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    glue = argument_string(state, name, arguments[0]);
    if (glue == NULL) return -1;
    array = (const parray *)arguments[1].as.gc;
    while (position < array->used) {
        pvalue key;
        pvalue item;
        size_t next;
        pstring *part;
        if (!pa_entry_at(array, position, &key, &item, &next)) break;
        part = argument_string(state, name, item);
        pv_release(key);
        pv_release(item);
        if (part == NULL) {
            ps_destroy(glue);
            return -1;
        }
        total += part->length + (items == 0U ? 0U : glue->length);
        ps_destroy(part);
        if (total > PPHP_STR_MAX) {
            ps_destroy(glue);
            pphp_runtime_error(state, 0U, "implode result exceeds maximum length");
            return -1;
        }
        items++;
        position = next;
    }
    bytes = pphp_alloc(total == 0U ? 1U : total);
    if (bytes == NULL) {
        ps_destroy(glue);
        pphp_runtime_error(state, 0U, "out of memory joining strings");
        return -1;
    }
    position = 0U;
    items = 0U;
    while (position < array->used) {
        pvalue key;
        pvalue item;
        size_t next;
        pstring *part;
        if (!pa_entry_at(array, position, &key, &item, &next)) break;
        part = argument_string(state, name, item);
        pv_release(key);
        pv_release(item);
        if (part == NULL) {
            pphp_free(bytes);
            ps_destroy(glue);
            return -1;
        }
        if (items++ != 0U) {
            memcpy(bytes + output, glue->data, glue->length);
            output += glue->length;
        }
        memcpy(bytes + output, part->data, part->length);
        output += part->length;
        ps_destroy(part);
        position = next;
    }
    {
        int handled = string_result(state, bytes, output, result);
        pphp_free(bytes);
        ps_destroy(glue);
        return handled;
    }
}

static int push_slice(pphp_state *state, parray *array, const char *bytes,
                      size_t length) {
    pstring *part = ps_new(bytes, length);
    pvalue value;
    int pushed;
    if (part == NULL) {
        pphp_runtime_error(state, 0U, "out of memory splitting string");
        return 0;
    }
    value = pv_heap(PT_STRING, &part->header);
    pushed = pa_push(array, value);
    pv_release(value);
    if (!pushed) pphp_runtime_error(state, 0U, "out of memory growing split result");
    return pushed;
}

static int call_explode(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count, pvalue *result) {
    pstring *delimiter;
    pstring *input;
    parray *array;
    size_t start = 0U;
    size_t pieces = 0U;
    size_t limit = SIZE_MAX;
    if (count < 2U || count > 3U ||
        (count == 3U && arguments[2].type != PT_INT)) {
        return fail_arguments(state, name);
    }
    delimiter = argument_string(state, name, arguments[0]);
    input = argument_string(state, name, arguments[1]);
    if (delimiter == NULL || input == NULL || delimiter->length == 0U) {
        ps_destroy(delimiter);
        ps_destroy(input);
        return fail_arguments(state, name);
    }
    if (count == 3U) {
        if (arguments[2].as.i <= 0) {
            ps_destroy(delimiter);
            ps_destroy(input);
            return fail_arguments(state, name);
        }
        limit = (size_t)arguments[2].as.i;
    }
    array = pa_new(4U);
    if (array == NULL) {
        ps_destroy(delimiter);
        ps_destroy(input);
        pphp_runtime_error(state, 0U, "out of memory creating explode result");
        return -1;
    }
    while (pieces + 1U < limit && start <= input->length) {
        size_t found;
        if (!find_bytes(input, delimiter, start, 0, &found)) break;
        if (!push_slice(state, array, input->data + start, found - start)) {
            pv_release(pv_heap(PT_ARRAY, &array->header));
            ps_destroy(delimiter);
            ps_destroy(input);
            return -1;
        }
        pieces++;
        start = found + delimiter->length;
    }
    if (!push_slice(state, array, input->data + start, input->length - start)) {
        pv_release(pv_heap(PT_ARRAY, &array->header));
        ps_destroy(delimiter);
        ps_destroy(input);
        return -1;
    }
    ps_destroy(delimiter);
    ps_destroy(input);
    *result = pv_heap(PT_ARRAY, &array->header);
    return 1;
}

static int call_str_split(pphp_state *state, const pstring *name,
                          const pvalue *arguments, size_t count,
                          pvalue *result) {
    pstring *input;
    size_t chunk = 1U;
    size_t offset;
    parray *array;
    if (count < 1U || count > 2U ||
        (count == 2U && (arguments[1].type != PT_INT || arguments[1].as.i < 1))) {
        return fail_arguments(state, name);
    }
    input = argument_string(state, name, arguments[0]);
    if (input == NULL) return -1;
    if (count == 2U) chunk = (size_t)arguments[1].as.i;
    array = pa_new(chunk == 0U ? 0U : input->length / chunk + 1U);
    if (array == NULL) {
        ps_destroy(input);
        pphp_runtime_error(state, 0U, "out of memory creating split result");
        return -1;
    }
    for (offset = 0U; offset < input->length; offset += chunk) {
        size_t length = input->length - offset < chunk
                            ? input->length - offset : chunk;
        if (!push_slice(state, array, input->data + offset, length)) {
            pv_release(pv_heap(PT_ARRAY, &array->header));
            ps_destroy(input);
            return -1;
        }
    }
    ps_destroy(input);
    *result = pv_heap(PT_ARRAY, &array->header);
    return 1;
}

static int replace_scalar(pphp_state *state, const pstring *name,
                          pvalue search_value, pvalue replacement_value,
                          pvalue subject_value, pvalue *result) {
    pstring *search;
    pstring *replacement;
    pstring *subject;
    size_t occurrences = 0U;
    size_t position = 0U;
    size_t found;
    size_t length;
    char *bytes;
    size_t output = 0U;
    search = argument_string(state, name, search_value);
    replacement = argument_string(state, name, replacement_value);
    subject = argument_string(state, name, subject_value);
    if (search == NULL || replacement == NULL || subject == NULL) {
        ps_destroy(search);
        ps_destroy(replacement);
        ps_destroy(subject);
        return -1;
    }
    if (search->length == 0U) {
        int handled = string_result(state, subject->data, subject->length, result);
        ps_destroy(search);
        ps_destroy(replacement);
        ps_destroy(subject);
        return handled;
    }
    while (find_bytes(subject, search, position, 0, &found)) {
        occurrences++;
        position = found + search->length;
    }
    length = subject->length - occurrences * search->length +
             occurrences * replacement->length;
    if (length > PPHP_STR_MAX) {
        ps_destroy(search);
        ps_destroy(replacement);
        ps_destroy(subject);
        pphp_runtime_error(state, 0U, "replace result exceeds maximum length");
        return -1;
    }
    bytes = pphp_alloc(length == 0U ? 1U : length);
    if (bytes == NULL) {
        ps_destroy(search);
        ps_destroy(replacement);
        ps_destroy(subject);
        pphp_runtime_error(state, 0U, "out of memory replacing string");
        return -1;
    }
    position = 0U;
    while (find_bytes(subject, search, position, 0, &found)) {
        memcpy(bytes + output, subject->data + position, found - position);
        output += found - position;
        memcpy(bytes + output, replacement->data, replacement->length);
        output += replacement->length;
        position = found + search->length;
    }
    memcpy(bytes + output, subject->data + position, subject->length - position);
    output += subject->length - position;
    {
        int handled = string_result(state, bytes, output, result);
        pphp_free(bytes);
        ps_destroy(search);
        ps_destroy(replacement);
        ps_destroy(subject);
        return handled;
    }
}

static int replace_value(pphp_state *state, const pstring *name,
                         pvalue search, pvalue replacement, pvalue subject,
                         pvalue *result) {
    if (subject.type == PT_ARRAY) {
        const parray *source = (const parray *)subject.as.gc;
        parray *output = pa_new(source->size);
        size_t position = 0U;
        if (output == NULL) goto replace_oom;
        while (position < source->used) {
            pvalue key;
            pvalue item;
            pvalue replaced = pv_null();
            size_t next;
            if (!pa_entry_at(source, position, &key, &item, &next)) break;
            if (!replace_value(state, name, search, replacement, item,
                               &replaced) ||
                !pa_set(output, key, replaced)) {
                pv_release(key);
                pv_release(item);
                pv_release(replaced);
                pv_release(pv_heap(PT_ARRAY, &output->header));
                if (state->error[0] == '\0') goto replace_oom;
                return 0;
            }
            pv_release(key);
            pv_release(item);
            pv_release(replaced);
            position = next;
        }
        *result = pv_heap(PT_ARRAY, &output->header);
        return 1;
    }
    if (search.type == PT_ARRAY) {
        const parray *searches = (const parray *)search.as.gc;
        const parray *replacements = replacement.type == PT_ARRAY
                                         ? (const parray *)replacement.as.gc
                                         : NULL;
        size_t position = 0U;
        pphp_int replacement_index = 0;
        pvalue current = subject;
        pv_retain(current);
        while (position < searches->used) {
            pvalue key;
            pvalue needle;
            pvalue replacement_item = replacement;
            pvalue replaced = pv_null();
            size_t next;
            if (!pa_entry_at(searches, position, &key, &needle, &next)) break;
            if (replacements != NULL &&
                !pa_get(replacements, pv_int(replacement_index),
                        &replacement_item)) {
                replacement_item = pv_null();
            } else if (replacements == NULL) {
                pv_retain(replacement_item);
            }
            if (replace_scalar(state, name, needle, replacement_item,
                               current, &replaced) <= 0) {
                pv_release(key);
                pv_release(needle);
                pv_release(replacement_item);
                pv_release(current);
                return 0;
            }
            pv_release(key);
            pv_release(needle);
            pv_release(replacement_item);
            pv_release(current);
            current = replaced;
            replacement_index++;
            position = next;
        }
        if (searches->size == 0U) {
            pstring *string = pv_to_string(current);
            pv_release(current);
            if (string == NULL) return 0;
            current = pv_heap(PT_STRING, &string->header);
        }
        *result = current;
        return 1;
    }
    if (replacement.type == PT_ARRAY) return 0;
    return replace_scalar(state, name, search, replacement, subject, result) > 0;
replace_oom:
    pphp_runtime_error(state, 0U, "out of memory replacing string array");
    return 0;
}

static int call_replace(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count, pvalue *result) {
    if (count != 3U ||
        !replace_value(state, name, arguments[0], arguments[1], arguments[2],
                       result)) {
        if (state->error[0] == '\0') return fail_arguments(state, name);
        return -1;
    }
    return 1;
}

static int call_str_pad(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count, pvalue *result) {
    pstring *input;
    pstring *padding = NULL;
    size_t target;
    int type = 1;
    size_t needed;
    size_t left;
    size_t right;
    char *bytes;
    size_t i;
    if (count < 2U || count > 4U || arguments[1].type != PT_INT ||
        arguments[1].as.i < 0 ||
        (count == 4U && arguments[3].type != PT_INT)) {
        return fail_arguments(state, name);
    }
    input = argument_string(state, name, arguments[0]);
    if (input == NULL) return -1;
    target = (size_t)arguments[1].as.i;
    if (count >= 3U) padding = argument_string(state, name, arguments[2]);
    else padding = ps_new(" ", 1U);
    if (padding == NULL || padding->length == 0U || target > PPHP_STR_MAX) {
        ps_destroy(input);
        ps_destroy(padding);
        return fail_arguments(state, name);
    }
    if (count == 4U) type = arguments[3].as.i;
    if (type < 0 || type > 2) {
        ps_destroy(input);
        ps_destroy(padding);
        return fail_arguments(state, name);
    }
    if (target <= input->length) {
        int handled = string_result(state, input->data, input->length, result);
        ps_destroy(input);
        ps_destroy(padding);
        return handled;
    }
    needed = target - input->length;
    left = type == 0 ? needed : (type == 2 ? needed / 2U : 0U);
    right = needed - left;
    bytes = pphp_alloc(target);
    if (bytes == NULL) {
        ps_destroy(input);
        ps_destroy(padding);
        pphp_runtime_error(state, 0U, "out of memory padding string");
        return -1;
    }
    for (i = 0U; i < left; i++) bytes[i] = padding->data[i % padding->length];
    memcpy(bytes + left, input->data, input->length);
    for (i = 0U; i < right; i++) {
        bytes[left + input->length + i] = padding->data[i % padding->length];
    }
    {
        int handled = string_result(state, bytes, target, result);
        pphp_free(bytes);
        ps_destroy(input);
        ps_destroy(padding);
        return handled;
    }
}

static int call_base_conversion(pphp_state *state, const pstring *name,
                                const pvalue *arguments, size_t count,
                                pvalue *result) {
    static const char digits[] = "0123456789abcdef";
    unsigned base;
    int encode;
    if (count != 1U) return fail_arguments(state, name);
    encode = name_is(name, "dechex") || name_is(name, "decbin") ||
             name_is(name, "decoct");
    base = name_is(name, "dechex") || name_is(name, "hexdec") ? 16U :
           (name_is(name, "decbin") || name_is(name, "bindec") ? 2U : 8U);
    if (encode) {
        char buffer[33];
        size_t length = 0U;
        uint32_t number;
        size_t i;
        if (arguments[0].type != PT_INT) return fail_arguments(state, name);
        number = (uint32_t)arguments[0].as.i;
        do {
            buffer[length++] = digits[number % base];
            number /= base;
        } while (number != 0U);
        for (i = 0U; i < length / 2U; i++) {
            char swap = buffer[i];
            buffer[i] = buffer[length - i - 1U];
            buffer[length - i - 1U] = swap;
        }
        return string_result(state, buffer, length, result);
    }
    {
        pstring *input = argument_string(state, name, arguments[0]);
        uint32_t number = 0U;
        size_t i;
        if (input == NULL) return -1;
        for (i = 0U; i < input->length; i++) {
            unsigned char c = (unsigned char)input->data[i];
            unsigned digit;
            if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a') + 10U;
            else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A') + 10U;
            else continue;
            if (digit < base) number = number * base + digit;
        }
        ps_destroy(input);
        *result = pv_int((pphp_int)number);
        return 1;
    }
}

int pphp_call_string_builtin(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    if (name_is(name, "substr")) return call_substr(state, name, arguments, count, result);
    if (name_is(name, "strpos") || name_is(name, "strrpos") ||
        name_is(name, "str_contains")) {
        return call_search(state, name, arguments, count, result);
    }
    if (name_is(name, "str_starts_with") || name_is(name, "str_ends_with")) {
        return call_edge_test(state, name, arguments, count, result);
    }
    if (name_is(name, "strtolower") || name_is(name, "strtoupper") ||
        name_is(name, "ucfirst") || name_is(name, "lcfirst")) {
        return call_ascii_case(state, name, arguments, count, result);
    }
    if (name_is(name, "trim") || name_is(name, "ltrim") || name_is(name, "rtrim")) {
        return call_trim(state, name, arguments, count, result);
    }
    if (name_is(name, "str_repeat") || name_is(name, "strrev")) {
        return call_repeat_or_reverse(state, name, arguments, count, result);
    }
    if (name_is(name, "strcmp") || name_is(name, "strcasecmp") ||
        name_is(name, "strncmp")) {
        return call_compare(state, name, arguments, count, result);
    }
    if (name_is(name, "bin2hex") || name_is(name, "hex2bin")) {
        return call_hex(state, name, arguments, count, result);
    }
    if (name_is(name, "chr") || name_is(name, "ord")) {
        return call_chr_ord(state, name, arguments, count, result);
    }
    if (name_is(name, "implode") || name_is(name, "join")) {
        return call_implode(state, name, arguments, count, result);
    }
    if (name_is(name, "explode")) {
        return call_explode(state, name, arguments, count, result);
    }
    if (name_is(name, "str_split")) {
        return call_str_split(state, name, arguments, count, result);
    }
    if (name_is(name, "str_replace")) {
        return call_replace(state, name, arguments, count, result);
    }
    if (name_is(name, "str_pad")) {
        return call_str_pad(state, name, arguments, count, result);
    }
    if (name_is(name, "dechex") || name_is(name, "hexdec") ||
        name_is(name, "decbin") || name_is(name, "bindec") ||
        name_is(name, "decoct") || name_is(name, "octdec")) {
        return call_base_conversion(state, name, arguments, count, result);
    }
    return 0;
}
