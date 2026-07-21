#ifndef PPHP_SYMBOL_H
#define PPHP_SYMBOL_H

#include <stddef.h>

#include "pstring.h"

typedef struct psymbol_table {
    pstring **entries;
    size_t capacity;
    size_t count;
} psymbol_table;

int psymbol_init(psymbol_table *table, size_t initial_capacity);
void psymbol_destroy(psymbol_table *table);
pstring *psymbol_intern(psymbol_table *table, const char *bytes, size_t length);
pstring *psymbol_find(const psymbol_table *table, const char *bytes, size_t length);

#endif

