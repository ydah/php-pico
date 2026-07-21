#include "parray.h"

#include "gc.h"

#include "pphp/pphp.h"
#include "pstring.h"

#include <limits.h>
#include <string.h>

#define EMPTY_BUCKET UINT16_MAX

static uint32_t integer_hash(pphp_int value) {
    uint32_t hash = (uint32_t)value;
    hash ^= hash >> 16U;
    hash *= UINT32_C(0x7feb352d);
    hash ^= hash >> 15U;
    hash *= UINT32_C(0x846ca68b);
    hash ^= hash >> 16U;
    return hash;
}

static uint32_t key_hash(pvalue key) {
    if (key.type == PT_INT) {
        return integer_hash(key.as.i);
    }
    return ((const pstring *)key.as.gc)->hash;
}

static int keys_equal(pvalue left, pvalue right) {
    if (left.type != right.type) {
        return 0;
    }
    if (left.type == PT_INT) {
        return left.as.i == right.as.i;
    }
    return left.type == PT_STRING &&
           ps_equal((const pstring *)left.as.gc, (const pstring *)right.as.gc);
}

static int numeric_string_key(const pstring *string, pphp_int *integer) {
    size_t i = 0U;
    int negative = 0;
    uint64_t value = 0U;
    uint64_t limit;
    if (string->length == 0U) {
        return 0;
    }
    if (string->data[i] == '-') {
        negative = 1;
        i++;
        if (i == string->length) return 0;
    } else if (string->data[i] == '+') {
        return 0;
    }
    if (string->data[i] == '0' && i + 1U < string->length) {
        return 0;
    }
    limit = negative ? (uint64_t)INT32_MAX + 1U : (uint64_t)INT32_MAX;
    for (; i < string->length; i++) {
        unsigned digit;
        if (string->data[i] < '0' || string->data[i] > '9') return 0;
        digit = (unsigned)(string->data[i] - '0');
        if (value > (limit - digit) / 10U) return 0;
        value = value * 10U + digit;
    }
    *integer = negative ? (pphp_int)(-(int64_t)value) : (pphp_int)value;
    return 1;
}

static int normalize_key(pvalue input, pvalue *key, int *temporary) {
    *temporary = 0;
    switch ((pvalue_type)input.type) {
        case PT_INT:
            *key = input;
            return 1;
        case PT_FLOAT:
            *key = pv_int((pphp_int)input.as.f);
            return 1;
        case PT_TRUE:
            *key = pv_int(1);
            return 1;
        case PT_FALSE:
            *key = pv_int(0);
            return 1;
        case PT_NULL: {
            pstring *empty = ps_new("", 0U);
            if (empty == NULL) return 0;
            *key = pv_heap(PT_STRING, &empty->header);
            *temporary = 1;
            return 1;
        }
        case PT_STRING: {
            pphp_int integer;
            if (numeric_string_key((const pstring *)input.as.gc, &integer)) {
                *key = pv_int(integer);
            } else {
                *key = input;
            }
            return 1;
        }
        default:
            return 0;
    }
}

static int ensure_entries(parray *array, size_t minimum) {
    size_t capacity;
    pentry *entries;
    if (minimum <= array->capacity) {
        return 1;
    }
    capacity = array->capacity == 0U ? 8U : (size_t)array->capacity * 2U;
    if (capacity < minimum) capacity = minimum;
    if (capacity > UINT16_MAX) return 0;
    entries = pphp_realloc(array->entries, capacity * sizeof(*entries));
    if (entries == NULL) return 0;
    array->entries = entries;
    array->capacity = (uint16_t)capacity;
    return 1;
}

static int rehash(parray *array, size_t bucket_count) {
    uint16_t *buckets;
    size_t i;
    if (bucket_count < 8U) bucket_count = 8U;
    if (bucket_count > UINT16_MAX) return 0;
    buckets = pphp_alloc(bucket_count * sizeof(*buckets));
    if (buckets == NULL) return 0;
    for (i = 0U; i < bucket_count; i++) buckets[i] = EMPTY_BUCKET;
    for (i = 0U; i < array->used; i++) {
        pentry *entry = &array->entries[i];
        if (entry->key.type != PT_NULL) {
            size_t bucket = key_hash(entry->key) & (bucket_count - 1U);
            entry->next = buckets[bucket];
            buckets[bucket] = (uint16_t)i;
        }
    }
    pphp_free(array->buckets);
    array->buckets = buckets;
    array->bucket_count = (uint16_t)bucket_count;
    array->header.flags &= (uint8_t)~PARRAY_PACKED;
    return 1;
}

static int promote_hash(parray *array) {
    size_t buckets = 8U;
    while (buckets < (size_t)array->size * 2U) buckets *= 2U;
    return rehash(array, buckets);
}

static int find_entry(const parray *array, pvalue key, uint16_t *entry_index,
                      uint16_t **link) {
    uint16_t *cursor;
    if ((array->header.flags & PARRAY_PACKED) != 0U) {
        if (key.type == PT_INT && key.as.i >= 0 && key.as.i < array->used) {
            *entry_index = (uint16_t)key.as.i;
            if (link != NULL) *link = NULL;
            return 1;
        }
        return 0;
    }
    if (array->bucket_count == 0U) return 0;
    cursor = &array->buckets[key_hash(key) & (array->bucket_count - 1U)];
    while (*cursor != EMPTY_BUCKET) {
        pentry *entry = &array->entries[*cursor];
        if (keys_equal(entry->key, key)) {
            *entry_index = *cursor;
            if (link != NULL) *link = cursor;
            return 1;
        }
        cursor = &entry->next;
    }
    if (link != NULL) *link = cursor;
    return 0;
}

parray *pa_new(size_t capacity_hint) {
    parray *array = pphp_alloc(sizeof(*array));
    if (array == NULL) return NULL;
    memset(array, 0, sizeof(*array));
    array->header.refcnt = 1U;
    array->header.type = PT_ARRAY;
    array->header.flags = PARRAY_PACKED;
    if (capacity_hint != 0U && !ensure_entries(array, capacity_hint)) {
        pphp_free(array);
        return NULL;
    }
    return array;
}

parray *pa_clone(const parray *array) {
    parray *copy = pa_new(array->capacity);
    size_t i;
    if (copy == NULL) return NULL;
    copy->header.flags = array->header.flags & PARRAY_PACKED;
    copy->size = array->size;
    copy->used = array->used;
    copy->next_index = array->next_index;
    for (i = 0U; i < array->used; i++) {
        copy->entries[i] = array->entries[i];
        if (copy->entries[i].key.type != PT_NULL) {
            pv_retain(copy->entries[i].key);
            pv_retain(copy->entries[i].value);
        }
    }
    if ((array->header.flags & PARRAY_PACKED) == 0U &&
        !rehash(copy, array->bucket_count)) {
        pa_destroy(copy);
        return NULL;
    }
    return copy;
}

void pa_destroy(parray *array) {
    size_t i;
    if (array == NULL) return;
    pphp_gc_unbuffer(&array->header);
    for (i = 0U; i < array->used; i++) {
        if (array->entries[i].key.type != PT_NULL) {
            pv_release(array->entries[i].key);
            pv_release(array->entries[i].value);
        }
    }
    pphp_free(array->entries);
    pphp_free(array->buckets);
    pphp_free(array);
}

size_t pa_count(const parray *array) {
    return array == NULL ? 0U : array->size;
}

int pa_get(const parray *array, pvalue input, pvalue *value) {
    pvalue key;
    int temporary;
    uint16_t index;
    if (array == NULL || value == NULL || !normalize_key(input, &key, &temporary)) {
        return 0;
    }
    if (!find_entry(array, key, &index, NULL)) {
        if (temporary) pv_release(key);
        return 0;
    }
    *value = array->entries[index].value;
    pv_retain(*value);
    if (temporary) pv_release(key);
    return 1;
}

int pa_set(parray *array, pvalue input, pvalue value) {
    pvalue key;
    int temporary;
    uint16_t index;
    uint16_t *link = NULL;
    if (array == NULL || !normalize_key(input, &key, &temporary)) return 0;
    if (find_entry(array, key, &index, &link)) {
        pv_retain(value);
        pv_release(array->entries[index].value);
        array->entries[index].value = value;
        if (temporary) pv_release(key);
        return 1;
    }
    if ((array->header.flags & PARRAY_PACKED) != 0U &&
        !(key.type == PT_INT && key.as.i == array->used)) {
        if (!promote_hash(array)) {
            if (temporary) pv_release(key);
            return 0;
        }
        (void)find_entry(array, key, &index, &link);
    }
    if ((array->header.flags & PARRAY_PACKED) == 0U &&
        ((size_t)array->size + 1U) * 4U >
            (size_t)array->bucket_count * 3U &&
        !rehash(array, (size_t)array->bucket_count * 2U)) {
        if (temporary) pv_release(key);
        return 0;
    }
    if (!ensure_entries(array, (size_t)array->used + 1U)) {
        if (temporary) pv_release(key);
        return 0;
    }
    index = array->used++;
    array->size++;
    array->entries[index].key = key;
    array->entries[index].value = value;
    array->entries[index].next = EMPTY_BUCKET;
    pv_retain(key);
    pv_retain(value);
    if ((array->header.flags & PARRAY_PACKED) == 0U) {
        size_t bucket;
        bucket = key_hash(key) & (array->bucket_count - 1U);
        array->entries[index].next = array->buckets[bucket];
        array->buckets[bucket] = index;
    }
    if (key.type == PT_INT && key.as.i >= array->next_index && key.as.i < INT32_MAX) {
        array->next_index = key.as.i + 1;
    }
    if (temporary) pv_release(key);
    return 1;
}

int pa_push(parray *array, pvalue value) {
    return array != NULL && pa_set(array, pv_int(array->next_index), value);
}

int pa_remove(parray *array, pvalue input) {
    pvalue key;
    int temporary;
    uint16_t index;
    uint16_t *link = NULL;
    if (array == NULL || !normalize_key(input, &key, &temporary)) return 0;
    if ((array->header.flags & PARRAY_PACKED) != 0U && !promote_hash(array)) {
        if (temporary) pv_release(key);
        return 0;
    }
    if (!find_entry(array, key, &index, &link)) {
        if (temporary) pv_release(key);
        return 0;
    }
    if (link != NULL) *link = array->entries[index].next;
    pv_release(array->entries[index].key);
    pv_release(array->entries[index].value);
    array->entries[index].key = pv_null();
    array->entries[index].value = pv_null();
    array->entries[index].next = EMPTY_BUCKET;
    array->size--;
    if (temporary) pv_release(key);
    return 1;
}

int pa_entry_at(const parray *array, size_t position, pvalue *key, pvalue *value,
                size_t *next_position) {
    if (array == NULL) return 0;
    while (position < array->used && array->entries[position].key.type == PT_NULL) {
        position++;
    }
    if (position >= array->used) return 0;
    *key = array->entries[position].key;
    *value = array->entries[position].value;
    pv_retain(*key);
    pv_retain(*value);
    *next_position = position + 1U;
    return 1;
}
