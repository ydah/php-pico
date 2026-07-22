#ifndef PPHP_PSTRING_H
#define PPHP_PSTRING_H

#include <stddef.h>
#include <stdint.h>

#include "value.h"

struct pmodule;

enum { PSTRING_INTERNED = 1U << 0 };

typedef struct pstring {
    pheader header;
    uint16_t length;
    uint16_t reserved;
    uint32_t hash;
} pstring;

typedef struct pro_string {
    pstring base;
    const char *data;
    struct pmodule *owner;
} pro_string;

static inline const char *ps_data(const pstring *string) {
    if (string == NULL) return NULL;
    if (string->header.type == PT_ROSTRING) {
        return ((const pro_string *)string)->data;
    }
    return (const char *)(string + 1);
}

static inline struct pmodule *ps_owner(const pstring *string) {
    return string != NULL && string->header.type == PT_ROSTRING
               ? ((const pro_string *)string)->owner : NULL;
}

uint32_t ps_hash_bytes(const char *bytes, size_t length);
pstring *ps_new(const char *bytes, size_t length);
pstring *ps_new_cstr(const char *string);
int ps_equal(const pstring *left, const pstring *right);
int ps_equal_bytes(const pstring *string, const char *bytes, size_t length);
void ps_destroy(pstring *string);

#endif
