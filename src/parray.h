#ifndef PPHP_PARRAY_H
#define PPHP_PARRAY_H

#include <stddef.h>
#include <stdint.h>

#include "value.h"

enum { PARRAY_PACKED = 1U << 2 };

typedef struct pentry {
    pvalue key;
    pvalue value;
    uint16_t next;
} pentry;

typedef struct parray {
    pheader header;
    uint16_t size;
    uint16_t used;
    uint16_t capacity;
    uint16_t bucket_count;
    pphp_int next_index;
    pentry *entries;
    uint16_t *buckets;
} parray;

parray *pa_new(size_t capacity_hint);
parray *pa_clone(const parray *array);
void pa_destroy(parray *array);
size_t pa_count(const parray *array);
int pa_get(const parray *array, pvalue key, pvalue *value);
int pa_set(parray *array, pvalue key, pvalue value);
int pa_push(parray *array, pvalue value);
int pa_remove(parray *array, pvalue key);
int pa_entry_at(const parray *array, size_t position, pvalue *key, pvalue *value,
                size_t *next_position);

#endif

