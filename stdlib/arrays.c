#include "arrays.h"

#include "parray.h"
#include "value_ops.h"
#include "vm.h"

#include <limits.h>
#include <string.h>

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int fail_arguments(pphp_state *state, const pstring *name) {
    pphp_runtime_error(state, 0U, "%.*s() received invalid arguments",
                       (int)name->length, ps_data(name));
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
#if PPHP_ENABLE_FLOAT
    pphp_int integer_product = 1;
#endif
    if (count != 1U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    array = (const parray *)arguments[0].as.gc;
    while (position < array->used) {
        pvalue key;
        pvalue value;
        size_t next;
        pphp_numeric numeric;
        if (!pa_entry_at(array, position, &key, &value, &next)) break;
        if (pv_to_numeric(value, 1, &numeric)) {
#if PPHP_ENABLE_FLOAT
            product *= numeric.number;
            if (all_integer) {
                pphp_int multiplied;
                if (!numeric.integer_exact) {
                    all_integer = 0;
                } else if (!pphp_integer_multiply(
                               integer_product, numeric.integer,
                               &multiplied)) {
                    all_integer = 0;
                } else {
                    integer_product = multiplied;
                }
            }
#else
            if (!pphp_integer_multiply(product, numeric.integer, &product)) {
                pv_release(key);
                pv_release(value);
                pphp_runtime_error(state, 0U,
                                   "integer overflow requires float support");
                return -1;
            }
#endif
            all_integer = all_integer && numeric.is_integer;
        } else if (numeric.is_integer < 0) {
            pv_release(key);
            pv_release(value);
            pphp_runtime_error(state, 0U,
                               "integer overflow requires float support");
            return -1;
        } else {
            product = 0;
#if PPHP_ENABLE_FLOAT
            integer_product = 0;
#endif
        }
        pv_release(key);
        pv_release(value);
        position = next;
    }
#if PPHP_ENABLE_FLOAT
    *result = all_integer ? pv_int(integer_product) : pv_float(product);
#else
    (void)all_integer;
    *result = pv_int(product);
#endif
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

static void release_array_storage(parray *array) {
    size_t i;
    for (i = 0U; i < array->used; i++) {
        if (array->entries[i].key.type != PT_NULL) {
            pv_release(array->entries[i].key);
            pv_release(array->entries[i].value);
        }
    }
    pphp_free(array->entries);
    pphp_free(array->buckets);
}

static void replace_array_storage(parray *target, parray *replacement) {
    uint16_t references = target->header.refcnt;
    uint8_t runtime_flags = target->header.flags & (uint8_t)~PARRAY_PACKED;
    release_array_storage(target);
    target->header.flags = runtime_flags |
                           (replacement->header.flags & PARRAY_PACKED);
    target->size = replacement->size;
    target->used = replacement->used;
    target->capacity = replacement->capacity;
    target->bucket_count = replacement->bucket_count;
    target->next_index = replacement->next_index;
    target->entries = replacement->entries;
    target->buckets = replacement->buckets;
    target->header.refcnt = references;
    replacement->entries = NULL;
    replacement->buckets = NULL;
    replacement->size = 0U;
    replacement->used = 0U;
    pphp_free(replacement);
}

static int append_source_entry(pphp_state *state, parray *output, pvalue key,
                               pvalue value, int reindex_integer) {
    int ok = reindex_integer && key.type == PT_INT
                 ? pa_push(output, value) : pa_set(output, key, value);
    if (!ok) pphp_runtime_error(state, 0U, "out of memory rebuilding array");
    return ok;
}

static int call_array_mutator(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result) {
    parray *array;
    if (count == 0U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    array = (parray *)arguments[0].as.gc;
    if (name_is(name, "array_push")) {
        size_t i;
        if (count < 2U) return fail_arguments(state, name);
        for (i = 1U; i < count; i++) {
            if (!pa_push(array, arguments[i])) {
                pphp_runtime_error(state, 0U, "out of memory pushing array value");
                return -1;
            }
        }
        *result = pv_int((pphp_int)array->size);
        return 1;
    }
    if (name_is(name, "array_unshift")) {
        parray *replacement;
        size_t i;
        size_t position = 0U;
        if (count < 2U) return fail_arguments(state, name);
        replacement = pa_new(array->size + count - 1U);
        if (replacement == NULL) goto mutation_oom;
        for (i = 1U; i < count; i++) {
            if (!pa_push(replacement, arguments[i])) goto rebuild_failed;
        }
        while (position < array->used) {
            pvalue key;
            pvalue value;
            size_t next;
            if (!pa_entry_at(array, position, &key, &value, &next)) break;
            if (!append_source_entry(state, replacement, key, value, 1)) {
                pv_release(key);
                pv_release(value);
                goto rebuild_failed;
            }
            pv_release(key);
            pv_release(value);
            position = next;
        }
        replace_array_storage(array, replacement);
        *result = pv_int((pphp_int)array->size);
        return 1;
rebuild_failed:
        pv_release(pv_heap(PT_ARRAY, &replacement->header));
        return -1;
    }
    if (name_is(name, "array_pop") || name_is(name, "array_shift")) {
        int shift = name_is(name, "array_shift");
        parray *replacement;
        size_t position = 0U;
        size_t selected_position = SIZE_MAX;
        pvalue selected = pv_null();
        if (count != 1U) return fail_arguments(state, name);
        while (position < array->used) {
            pvalue key;
            pvalue value;
            size_t next;
            if (!pa_entry_at(array, position, &key, &value, &next)) break;
            if (selected_position == SIZE_MAX || !shift) {
                pv_release(selected);
                selected = value;
                selected_position = position;
            } else {
                pv_release(value);
            }
            pv_release(key);
            if (shift) break;
            position = next;
        }
        if (selected_position == SIZE_MAX) {
            *result = pv_null();
            return 1;
        }
        replacement = pa_new(array->size - 1U);
        if (replacement == NULL) {
            pv_release(selected);
            goto mutation_oom;
        }
        position = 0U;
        while (position < array->used) {
            pvalue key;
            pvalue value;
            size_t next;
            if (!pa_entry_at(array, position, &key, &value, &next)) break;
            if (position != selected_position &&
                !append_source_entry(state, replacement, key, value, shift)) {
                pv_release(key);
                pv_release(value);
                pv_release(selected);
                pv_release(pv_heap(PT_ARRAY, &replacement->header));
                return -1;
            }
            pv_release(key);
            pv_release(value);
            position = next;
        }
        replace_array_storage(array, replacement);
        *result = selected;
        return 1;
    }
    return 0;
mutation_oom:
    pphp_runtime_error(state, 0U, "out of memory mutating array");
    return -1;
}

static int invoke_callback(pphp_state *state, pvalue callback,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    if (pphp_vm_invoke(state, callback, arguments, count, result)) return 1;
    if (state->error[0] == '\0') {
        pphp_runtime_error(state, 0U, "array callback invocation failed");
    }
    return 0;
}

static int call_array_map(pphp_state *state, const pstring *name,
                          const pvalue *arguments, size_t count,
                          pvalue *result) {
    const parray *source;
    parray *output;
    size_t position = 0U;
    if (count != 2U || arguments[1].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    source = (const parray *)arguments[1].as.gc;
    output = pa_new(source->size);
    if (output == NULL) goto callback_oom;
    while (position < source->used) {
        pvalue key;
        pvalue value;
        pvalue mapped = pv_null();
        size_t next;
        if (!pa_entry_at(source, position, &key, &value, &next)) break;
        if (!invoke_callback(state, arguments[0], &value, 1U, &mapped) ||
            !pa_set(output, key, mapped)) {
            pv_release(key);
            pv_release(value);
            pv_release(mapped);
            pv_release(pv_heap(PT_ARRAY, &output->header));
            if (state->error[0] == '\0') goto callback_oom;
            return -1;
        }
        pv_release(key);
        pv_release(value);
        pv_release(mapped);
        position = next;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
callback_oom:
    pphp_runtime_error(state, 0U, "out of memory creating callback result");
    return -1;
}

static int call_array_filter(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    const parray *source;
    parray *output;
    size_t position = 0U;
    int mode = 0;
    int has_callback;
    if (count < 1U || count > 3U || arguments[0].type != PT_ARRAY ||
        (count == 3U &&
         (arguments[2].type != PT_INT || arguments[2].as.i < 0 ||
          arguments[2].as.i > 2))) {
        return fail_arguments(state, name);
    }
    has_callback = count >= 2U && arguments[1].type != PT_NULL;
    if (count == 3U) mode = (int)arguments[2].as.i;
    if (mode < 0 || mode > 2) return fail_arguments(state, name);
    source = (const parray *)arguments[0].as.gc;
    output = pa_new(source->size);
    if (output == NULL) goto filter_oom;
    while (position < source->used) {
        pvalue key;
        pvalue value;
        pvalue callback_result = pv_null();
        size_t next;
        int keep;
        if (!pa_entry_at(source, position, &key, &value, &next)) break;
        if (!has_callback) {
            keep = pv_is_truthy(value);
        } else if (mode == 1) {
            pvalue callback_arguments[2] = {value, key};
            if (!invoke_callback(state, arguments[1], callback_arguments, 2U,
                                 &callback_result)) goto filter_failed;
            keep = pv_is_truthy(callback_result);
        } else {
            pvalue callback_argument = mode == 2 ? key : value;
            if (!invoke_callback(state, arguments[1], &callback_argument, 1U,
                                 &callback_result)) goto filter_failed;
            keep = pv_is_truthy(callback_result);
        }
        if (keep && !pa_set(output, key, value)) goto filter_failed;
        pv_release(callback_result);
        pv_release(key);
        pv_release(value);
        position = next;
        continue;
filter_failed:
        pv_release(callback_result);
        pv_release(key);
        pv_release(value);
        pv_release(pv_heap(PT_ARRAY, &output->header));
        if (state->error[0] == '\0') goto filter_oom;
        return -1;
    }
    *result = pv_heap(PT_ARRAY, &output->header);
    return 1;
filter_oom:
    pphp_runtime_error(state, 0U, "out of memory filtering array");
    return -1;
}

static int call_array_reduce(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    const parray *source;
    size_t position = 0U;
    pvalue carry;
    if (count < 2U || count > 3U || arguments[0].type != PT_ARRAY) {
        return fail_arguments(state, name);
    }
    source = (const parray *)arguments[0].as.gc;
    carry = count == 3U ? arguments[2] : pv_null();
    pv_retain(carry);
    while (position < source->used) {
        pvalue key;
        pvalue value;
        pvalue next_carry = pv_null();
        pvalue callback_arguments[2];
        size_t next;
        if (!pa_entry_at(source, position, &key, &value, &next)) break;
        callback_arguments[0] = carry;
        callback_arguments[1] = value;
        if (!invoke_callback(state, arguments[1], callback_arguments, 2U,
                             &next_carry)) {
            pv_release(key);
            pv_release(value);
            pv_release(carry);
            return -1;
        }
        pv_release(key);
        pv_release(value);
        pv_release(carry);
        carry = next_carry;
        position = next;
    }
    *result = carry;
    return 1;
}

typedef struct sortable_entry {
    pvalue key;
    pvalue value;
} sortable_entry;

static void release_sortable(sortable_entry *entries, size_t count) {
    size_t i;
    for (i = 0U; i < count; i++) {
        pv_release(entries[i].key);
        pv_release(entries[i].value);
    }
    pphp_free(entries);
}

static int compare_sortable(pphp_state *state, const sortable_entry *left,
                            const sortable_entry *right, int keys,
                            int descending, int user, pvalue callback,
                            int *compared) {
    const char *error = NULL;
    if (user) {
        pvalue callback_arguments[2];
        pvalue callback_result = pv_null();
        pphp_float number;
        int integer;
        callback_arguments[0] = keys ? left->key : left->value;
        callback_arguments[1] = keys ? right->key : right->value;
        if (!invoke_callback(state, callback, callback_arguments, 2U,
                             &callback_result) ||
            !pv_to_number(callback_result, &number, &integer)) {
            pv_release(callback_result);
            if (state->error[0] == '\0') {
                pphp_runtime_error(state, 0U,
                                   "sort callback must return a number");
            }
            return 0;
        }
        *compared = number < 0 ? -1 : (number > 0 ? 1 : 0);
        pv_release(callback_result);
    } else if (!pv_compare(keys ? left->key : left->value,
                           keys ? right->key : right->value, 0,
                           compared, &error)) {
        pphp_runtime_error(state, 0U, "%s",
                           error == NULL ? "array comparison failed" : error);
        return 0;
    }
    if (descending) *compared = -*compared;
    return 1;
}

static int call_array_sort(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    parray *array;
    sortable_entry *entries;
    size_t position = 0U;
    size_t used = 0U;
    size_t i;
    int user = name_is(name, "usort") || name_is(name, "uasort") ||
               name_is(name, "uksort");
    int keys = name_is(name, "ksort") || name_is(name, "krsort") ||
               name_is(name, "uksort");
    int preserve = keys || name_is(name, "asort") || name_is(name, "arsort") ||
                   name_is(name, "uasort");
    int descending = name_is(name, "rsort") || name_is(name, "arsort") ||
                     name_is(name, "krsort");
    pvalue callback = pv_null();
    parray *replacement;
    if ((!user && (count < 1U || count > 2U)) ||
        (user && count != 2U) || arguments[0].type != PT_ARRAY ||
        (!user && count == 2U && arguments[1].type != PT_INT)) {
        return fail_arguments(state, name);
    }
    if (user) callback = arguments[1];
    array = (parray *)arguments[0].as.gc;
    entries = array->size == 0U
                  ? NULL : pphp_alloc(array->size * sizeof(*entries));
    if (array->size != 0U && entries == NULL) goto sort_oom;
    while (position < array->used) {
        size_t next;
        if (!pa_entry_at(array, position, &entries[used].key,
                         &entries[used].value, &next)) break;
        used++;
        position = next;
    }
    for (i = 1U; i < used; i++) {
        sortable_entry current = entries[i];
        size_t insert = i;
        while (insert > 0U) {
            int compared;
            if (!compare_sortable(state, &entries[insert - 1U], &current,
                                  keys, descending, user, callback, &compared)) {
                release_sortable(entries, used);
                return -1;
            }
            if (compared <= 0) break;
            entries[insert] = entries[insert - 1U];
            insert--;
        }
        entries[insert] = current;
    }
    replacement = pa_new(used);
    if (replacement == NULL) {
        release_sortable(entries, used);
        goto sort_oom;
    }
    for (i = 0U; i < used; i++) {
        int ok = preserve ? pa_set(replacement, entries[i].key, entries[i].value)
                          : pa_push(replacement, entries[i].value);
        if (!ok) {
            pv_release(pv_heap(PT_ARRAY, &replacement->header));
            release_sortable(entries, used);
            goto sort_oom;
        }
    }
    replace_array_storage(array, replacement);
    release_sortable(entries, used);
    *result = pv_bool(1);
    return 1;
sort_oom:
    pphp_runtime_error(state, 0U, "out of memory sorting array");
    return -1;
}

int pphp_call_array_builtin(pphp_state *state, const pstring *name,
                            const pvalue *arguments, size_t count,
                            pvalue *result) {
    if (name_is(name, "array_push") || name_is(name, "array_pop") ||
        name_is(name, "array_shift") || name_is(name, "array_unshift")) {
        return call_array_mutator(state, name, arguments, count, result);
    }
    if (name_is(name, "array_map")) {
        return call_array_map(state, name, arguments, count, result);
    }
    if (name_is(name, "array_filter")) {
        return call_array_filter(state, name, arguments, count, result);
    }
    if (name_is(name, "array_reduce")) {
        return call_array_reduce(state, name, arguments, count, result);
    }
    if (name_is(name, "sort") || name_is(name, "rsort") ||
        name_is(name, "usort") || name_is(name, "asort") ||
        name_is(name, "arsort") || name_is(name, "ksort") ||
        name_is(name, "krsort") || name_is(name, "uasort") ||
        name_is(name, "uksort")) {
        return call_array_sort(state, name, arguments, count, result);
    }
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
