#ifndef PPHP_RESOURCE_H
#define PPHP_RESOURCE_H

#include <stddef.h>

#include "parray.h"

typedef struct presource {
    pheader header;
    void (*destroy)(struct presource *resource);
    uint8_t kind;
} presource;

enum {
    PRESOURCE_ITERATOR = 1,
    PRESOURCE_FILE = 2
};

typedef struct parray_iterator {
    presource resource;
    parray *array;
    size_t position;
} parray_iterator;

parray_iterator *pa_iterator_new(parray *array);
int pa_iterator_next(parray_iterator *iterator, pvalue *key, pvalue *value);
void presource_destroy(presource *resource);

#endif
