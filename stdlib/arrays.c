#include "arrays.h"

#include "parray.h"
#include "value_ops.h"

#include <limits.h>
#include <string.h>

#if PPHP_INT64
#define PPHP_INT_MAXIMUM INT64_MAX
#define PPHP_INT_MINIMUM INT64_MIN
#else
#define PPHP_INT_MAXIMUM INT32_MAX
#define PPHP_INT_MINIMUM INT32_MIN
#endif

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int fail_arguments(pphp_state *state, const pstring *name) {
    pphp_runtime_error(state, 0U, "%.*s() received invalid arguments",
                       (int)name->length, name->data);
    return -1;
}

static int values_equal(pvalue left, pvalue right, int strict) {
    int compared = 0;
    const char *error = NULL;
    return pv_compare(left, right, strict, &compared, &error) && compared == 0;
}

static int call_array_search(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    const parray *array;
    size_t position = 0U;
    int strict = 0;
    if (count < 2U || count > 3U || arguments[1].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    if (count == 3U) strict = pv_is_truthy(arguments[2]);
    array = (const parray *)arguments[1].as.gc;
    while (position < array->used) {
        pvalue key;
        pvalue value;
        size_t next;
        if (!pa_entry_at(array, position, &key, &value, &next)) break;
        if (values_equal(arguments[0], value, strict)) {
            pv_release(value);
            if (name_is(name, "in_array")) {
                pv_release(key);
                *result = pv_bool(1);
            } else {
                *result = key;
            }
            return 1;
        }
        pv_release(key);
        pv_release(value);
        position = next;
    }
    *result = pv_bool(0);
    return 1;
}

static int call_key_exists(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    pvalue found = pv_null();
    int exists;
    if (count != 2U || arguments[1].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    exists = pa_get((const parray *)arguments[1].as.gc, arguments[0], &found);
    pv_release(found);
    *result = pv_bool(exists);
    return 1;
}

static int array_push_owned(pphp_state *state, parray *array, pvalue value) {
    if (pa_push(array, value)) return 1;
    pphp_runtime_error(state, 0U, "out of memory growing array result");
    return 0;
}

static int array_set_owned(pphp_state *state, parray *array, pvalue key,
                           pvalue value) {
    if (pa_set(array, key, value)) return 1;
    pphp_runtime_error(state, 0U, "out of memory growing array result");
    return 0;
}

static int generated_array_key(pvalue source, pvalue *key, int *temporary) {
    pstring *string;
    *temporary = 0;
    if (source.type == PT_INT || source.type == PT_STRING) {
        *key = source;
        return 1;
    }
    string = pv_to_string(source);
    if (string == NULL) return 0;
    *key = pv_heap(PT_STRING, &string->header);
    *temporary = 1;
    return 1;
}

static int call_keys_values(pphp_state *state, const pstring *name,
                            const pvalue *arguments, size_t count,
                            pvalue *result) {
    const parray *source;
    parray *output;
    size_t position = 0U;
    int filter = count == 2U;
    if (count < 1U || count > 2U || arguments[0].type != PT_ARRAY ||
        (name_is(name, "array_values") && count != 1U)) {
        return fail_arguments(state, name);
    }
    source = (const parray *)arguments[0].as.gc;
    output = pa_new(source->size);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating array result");
        return -1;
    }
    while (position < source->used) {
        pvalue key;
        pvalue value;
        size_t next;
        pvalue selected;
        if (!pa_entry_at(source, position, &key, &value, &next)) break;
        selected = name_is(name, "array_keys") ? key : value;
        if ((!filter || values_equal(value, arguments[1], 0)) &&
            !array_push_owned(state, output, selected)) {
            pv_release(key);
            pv_release(value);
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
        pv_release(key);
        pv_release(value);
        position = next;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_slice(pphp_state *state, const pstring *name,
                            const pvalue *arguments, size_t count,
                            pvalue *result) {
    const parray *source;
    parray *output;
    int64_t offset;
    int64_t length;
    int preserve = 0;
    size_t position = 0U;
    size_t ordinal = 0U;
    if (count < 2U || count > 4U || arguments[0].type != PT_ARRAY ||
        arguments[1].type != PT_INT ||
        (count >= 3U && arguments[2].type != PT_INT && arguments[2].type != PT_NULL)) {
        return fail_arguments(state, name);
    }
    source = (const parray *)arguments[0].as.gc;
    offset = arguments[1].as.i;
    if (offset < 0) offset = (int64_t)source->size + offset;
    if (offset < 0) offset = 0;
    if (offset > source->size) offset = source->size;
    length = (int64_t)source->size - offset;
    if (count >= 3U && arguments[2].type == PT_INT) {
        length = arguments[2].as.i;
        if (length < 0) length = (int64_t)source->size - offset + length;
        if (length < 0) length = 0;
    }
    if (count == 4U) preserve = pv_is_truthy(arguments[3]);
    output = pa_new((size_t)length);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating slice");
        return -1;
    }
    while (position < source->used && length > 0) {
        pvalue key;
        pvalue value;
        size_t next;
        if (!pa_entry_at(source, position, &key, &value, &next)) break;
        if (ordinal >= (size_t)offset) {
            int ok = preserve || key.type == PT_STRING
                         ? array_set_owned(state, output, key, value)
                         : array_push_owned(state, output, value);
            if (!ok) {
                pv_release(key);
                pv_release(value);
                pv_release(pv_heap(PT_ARRAY, &output->header));
                return -1;
            }
            length--;
        }
        pv_release(key);
        pv_release(value);
        ordinal++;
        position = next;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_merge(pphp_state *state, const pstring *name,
                            const pvalue *arguments, size_t count,
                            pvalue *result) {
    parray *output = pa_new(4U);
    size_t argument;
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating merged array");
        return -1;
    }
    for (argument = 0U; argument < count; argument++) {
        const parray *source;
        size_t position = 0U;
        if (arguments[argument].type != PT_ARRAY) {
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return fail_arguments(state, name);
        }
        source = (const parray *)arguments[argument].as.gc;
        while (position < source->used) {
            pvalue key;
            pvalue value;
            size_t next;
            int ok;
            if (!pa_entry_at(source, position, &key, &value, &next)) break;
            ok = key.type == PT_INT
                     ? array_push_owned(state, output, value)
                     : array_set_owned(state, output, key, value);
            pv_release(key);
            pv_release(value);
            if (!ok) {
                pv_release(pv_heap(PT_ARRAY, &output->header));
                return -1;
            }
            position = next;
        }
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_reverse(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result) {
    const parray *source;
    parray *output;
    int preserve = 0;
    size_t cursor;
    if (count < 1U || count > 2U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    if (count == 2U) preserve = pv_is_truthy(arguments[1]);
    source = (const parray *)arguments[0].as.gc;
    output = pa_new(source->size);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory reversing array");
        return -1;
    }
    cursor = source->used;
    while (cursor > 0U) {
        pvalue key;
        pvalue value;
        int ok;
        cursor--;
        if (source->entries[cursor].key.type == PT_NULL) continue;
        key = source->entries[cursor].key;
        value = source->entries[cursor].value;
        pv_retain(key);
        pv_retain(value);
        ok = preserve || key.type == PT_STRING
                 ? array_set_owned(state, output, key, value)
                 : array_push_owned(state, output, value);
        pv_release(key);
        pv_release(value);
        if (!ok) {
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_product(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result) {
    const parray *array;
    size_t position = 0U;
    pphp_float product = 1;
    int all_integer = 1;
    if (count != 1U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    array = (const parray *)arguments[0].as.gc;
    while (position < array->used) {
        pvalue key;
        pvalue value;
        size_t next;
        pphp_float number;
        int integer;
        if (!pa_entry_at(array, position, &key, &value, &next)) break;
        if (pv_to_number(value, &number, &integer)) {
            product *= number;
            all_integer = all_integer && integer;
        } else {
            product = 0;
        }
        pv_release(key);
        pv_release(value);
        position = next;
    }
    *result = all_integer ? pv_int((pphp_int)product) : pv_float(product);
    return 1;
}

static int call_array_fill(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    pphp_int start;
    pphp_int item_count;
    pphp_int index;
    parray *output;
    size_t i;
    if (count != 3U || arguments[0].type != PT_INT ||
        arguments[1].type != PT_INT || arguments[1].as.i < 0) {
        return fail_arguments(state, name);
    }
    start = arguments[0].as.i;
    item_count = arguments[1].as.i;
    if ((uint64_t)item_count > UINT16_MAX) return fail_arguments(state, name);
    output = pa_new((size_t)item_count);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating filled array");
        return -1;
    }
    index = start;
    for (i = 0U; i < (size_t)item_count; i++) {
        if (!array_set_owned(state, output, pv_int(index), arguments[2])) {
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
        if (i + 1U < (size_t)item_count) {
            if (index == PPHP_INT_MAXIMUM) {
                pv_release(pv_heap(PT_ARRAY, &output->header));
                return fail_arguments(state, name);
            }
            index++;
        }
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_fill_keys(pphp_state *state, const pstring *name,
                                const pvalue *arguments, size_t count,
                                pvalue *result) {
    const parray *keys;
    parray *output;
    size_t position = 0U;
    if (count != 2U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    keys = (const parray *)arguments[0].as.gc;
    output = pa_new(keys->size);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating filled array");
        return -1;
    }
    while (position < keys->used) {
        pvalue source_key;
        pvalue value;
        pvalue key;
        size_t next;
        int temporary;
        int ok;
        if (!pa_entry_at(keys, position, &source_key, &value, &next)) break;
        if (!generated_array_key(value, &key, &temporary)) {
            pv_release(source_key);
            pv_release(value);
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return fail_arguments(state, name);
        }
        ok = array_set_owned(state, output, key, arguments[1]);
        if (temporary) pv_release(key);
        pv_release(source_key);
        pv_release(value);
        if (!ok) {
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
        position = next;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_flip(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    const parray *source;
    parray *output;
    size_t position = 0U;
    if (count != 1U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    source = (const parray *)arguments[0].as.gc;
    output = pa_new(source->size);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory flipping array");
        return -1;
    }
    while (position < source->used) {
        pvalue key;
        pvalue value;
        size_t next;
        if (!pa_entry_at(source, position, &key, &value, &next)) break;
        if ((value.type == PT_INT || value.type == PT_STRING) &&
            !array_set_owned(state, output, value, key)) {
            pv_release(key);
            pv_release(value);
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
        pv_release(key);
        pv_release(value);
        position = next;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_combine(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result) {
    const parray *keys;
    const parray *values;
    parray *output;
    size_t key_position = 0U;
    size_t value_position = 0U;
    if (count != 2U || arguments[0].type != PT_ARRAY ||
        arguments[1].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    keys = (const parray *)arguments[0].as.gc;
    values = (const parray *)arguments[1].as.gc;
    if (keys->size != values->size) return fail_arguments(state, name);
    output = pa_new(keys->size);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory combining arrays");
        return -1;
    }
    while (key_position < keys->used && value_position < values->used) {
        pvalue discarded_key;
        pvalue key;
        pvalue discarded_value_key;
        pvalue value;
        pvalue normalized_key;
        size_t next_key;
        size_t next_value;
        int temporary;
        int ok;
        if (!pa_entry_at(keys, key_position, &discarded_key, &key, &next_key) ||
            !pa_entry_at(values, value_position, &discarded_value_key, &value,
                         &next_value)) {
            break;
        }
        if (!generated_array_key(key, &normalized_key, &temporary)) {
            pv_release(discarded_key);
            pv_release(key);
            pv_release(discarded_value_key);
            pv_release(value);
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return fail_arguments(state, name);
        }
        ok = array_set_owned(state, output, normalized_key, value);
        if (temporary) pv_release(normalized_key);
        pv_release(discarded_key);
        pv_release(key);
        pv_release(discarded_value_key);
        pv_release(value);
        if (!ok) {
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
        key_position = next_key;
        value_position = next_value;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_range(pphp_state *state, const pstring *name,
                      const pvalue *arguments, size_t count, pvalue *result) {
    pphp_int start;
    pphp_int end;
    pphp_int step = 1;
    pphp_int value;
    parray *output;
    size_t guard = 0U;
    if (count < 2U || count > 3U || arguments[0].type != PT_INT ||
        arguments[1].type != PT_INT ||
        (count == 3U && arguments[2].type != PT_INT)) {
        return fail_arguments(state, name);
    }
    start = arguments[0].as.i;
    end = arguments[1].as.i;
    if (count == 3U) step = arguments[2].as.i;
    if (step == 0) return fail_arguments(state, name);
    if (step == PPHP_INT_MINIMUM) return fail_arguments(state, name);
    if (step < 0) step = -step;
    output = pa_new(8U);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory creating range");
        return -1;
    }
    value = start;
    while ((start <= end ? value <= end : value >= end) && guard++ < UINT16_MAX) {
        if (!array_push_owned(state, output, pv_int(value))) {
            pv_release(pv_heap(PT_ARRAY, &output->header));
            return -1;
        }
        if ((start <= end && value > PPHP_INT_MAXIMUM - step) ||
            (start > end && value < PPHP_INT_MINIMUM + step)) {
            break;
        }
        value += start <= end ? step : -step;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
}

static int call_array_is_list(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result) {
    const parray *array;
    size_t position = 0U;
    pphp_int expected = 0;
    int list = 1;
    if (count != 1U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    array = (const parray *)arguments[0].as.gc;
    while (position < array->used) {
        pvalue key;
        pvalue value;
        size_t next;
        if (!pa_entry_at(array, position, &key, &value, &next)) break;
        if (key.type != PT_INT || key.as.i != expected++) list = 0;
        pv_release(key);
        pv_release(value);
        position = next;
    }
    *result = pv_bool(list);
    return 1;
}

int pphp_call_array_builtin(pphp_state *state, const pstring *name,
                            const pvalue *arguments, size_t count,
                            pvalue *result) {
    if (name_is(name, "in_array") || name_is(name, "array_search")) {
        return call_array_search(state, name, arguments, count, result);
    }
    if (name_is(name, "array_key_exists") || name_is(name, "key_exists")) {
        return call_key_exists(state, name, arguments, count, result);
    }
    if (name_is(name, "array_keys") || name_is(name, "array_values")) {
        return call_keys_values(state, name, arguments, count, result);
    }
    if (name_is(name, "array_slice")) {
        return call_array_slice(state, name, arguments, count, result);
    }
    if (name_is(name, "array_merge")) {
        return call_array_merge(state, name, arguments, count, result);
    }
    if (name_is(name, "array_reverse")) {
        return call_array_reverse(state, name, arguments, count, result);
    }
    if (name_is(name, "array_product")) {
        return call_array_product(state, name, arguments, count, result);
    }
    if (name_is(name, "array_fill")) {
        return call_array_fill(state, name, arguments, count, result);
    }
    if (name_is(name, "array_fill_keys")) {
        return call_array_fill_keys(state, name, arguments, count, result);
    }
    if (name_is(name, "array_flip")) {
        return call_array_flip(state, name, arguments, count, result);
    }
    if (name_is(name, "array_combine")) {
        return call_array_combine(state, name, arguments, count, result);
    }
    if (name_is(name, "range")) {
        return call_range(state, name, arguments, count, result);
    }
    if (name_is(name, "array_is_list")) {
        return call_array_is_list(state, name, arguments, count, result);
    }
    return 0;
}
