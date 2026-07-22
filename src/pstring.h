#ifndef PPHP_PSTRING_H
#define PPHP_PSTRING_H

#include <stddef.h>
#include <stdint.h>

#include "value.h"

enum { PSTRING_INTERNED = 1U << 0 };

typedef struct pstring {
    pheader header;
    uint16_t length;
    uint16_t reserved;
    uint32_t hash;
    const char *data;
} pstring;

uint32_t ps_hash_bytes(const char *bytes, size_t length);
pstring *ps_new(const char *bytes, size_t length);
pstring *ps_new_cstr(const char *string);
int ps_equal(const pstring *left, const pstring *right);
int ps_equal_bytes(const pstring *string, const char *bytes, size_t length);
void ps_destroy(pstring *string);

#endif
