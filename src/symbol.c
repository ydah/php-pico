#include "symbol.h"

#include "pphp/pphp.h"

#include <string.h>

static size_t next_power_of_two(size_t value) {
    size_t result = 8U;
    while (result < value && result <= SIZE_MAX / 2U) {
        result *= 2U;
    }
    return result;
}

static size_t find_slot(pstring *const *entries, size_t capacity,
                        const char *bytes, size_t length, uint32_t hash) {
    size_t slot = hash & (capacity - 1U);
    while (entries[slot] != NULL &&
           !ps_equal_bytes(entries[slot], bytes, length)) {
        slot = (slot + 1U) & (capacity - 1U);
    }
    return slot;
}

static int resize_table(psymbol_table *table, size_t capacity) {
    pstring **entries;
    size_t i;
    capacity = next_power_of_two(capacity);
    entries = pphp_alloc(capacity * sizeof(*entries));
    if (entries == NULL) {
        return 0;
    }
    memset(entries, 0, capacity * sizeof(*entries));
    for (i = 0U; i < table->capacity; i++) {
        pstring *string = table->entries[i];
        if (string != NULL) {
            size_t slot = find_slot(entries, capacity, ps_data(string),
                                    string->length, string->hash);
            entries[slot] = string;
        }
    }
    pphp_free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
    return 1;
}

int psymbol_init(psymbol_table *table, size_t initial_capacity) {
    if (table == NULL) {
        return 0;
    }
    memset(table, 0, sizeof(*table));
    return resize_table(table, initial_capacity < 64U ? 64U : initial_capacity);
}

void psymbol_destroy(psymbol_table *table) {
    size_t i;
    if (table == NULL) {
        return;
    }
    for (i = 0U; i < table->capacity; i++) {
        if (table->entries[i] != NULL) {
            table->entries[i]->header.flags &= (uint8_t)~PSTRING_INTERNED;
            ps_destroy(table->entries[i]);
        }
    }
    pphp_free(table->entries);
    memset(table, 0, sizeof(*table));
}

pstring *psymbol_find(const psymbol_table *table, const char *bytes, size_t length) {
    uint32_t hash;
    size_t slot;
    if (table == NULL || table->capacity == 0U || bytes == NULL) {
        return NULL;
    }
    hash = ps_hash_bytes(bytes, length);
    slot = find_slot(table->entries, table->capacity, bytes, length, hash);
    return table->entries[slot];
}

pstring *psymbol_intern(psymbol_table *table, const char *bytes, size_t length) {
    uint32_t hash;
    size_t slot;
    pstring *string;

    if (table == NULL || table->capacity == 0U || bytes == NULL) {
        return NULL;
    }
    if ((table->count + 1U) * 4U >= table->capacity * 3U &&
        !resize_table(table, table->capacity * 2U)) {
        return NULL;
    }
    hash = ps_hash_bytes(bytes, length);
    slot = find_slot(table->entries, table->capacity, bytes, length, hash);
    if (table->entries[slot] != NULL) {
        return table->entries[slot];
    }
    string = ps_new(bytes, length);
    if (string == NULL) {
        return NULL;
    }
    string->header.flags |= PSTRING_INTERNED;
    table->entries[slot] = string;
    table->count++;
    return string;
}
