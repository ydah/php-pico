#include "pstring.h"

#include "alloc.h"
#include "pphp/pphp.h"

#include <string.h>

uint32_t ps_hash_bytes(const char *bytes, size_t length) {
    uint32_t hash = UINT32_C(2166136261);
    size_t i;
    for (i = 0U; i < length; i++) {
        hash ^= (uint8_t)bytes[i];
        hash *= UINT32_C(16777619);
    }
    return hash == 0U ? 1U : hash;
}

pstring *ps_new(const char *bytes, size_t length) {
    pstring *string;
    char *storage;
    if ((bytes == NULL && length != 0U) || length > PPHP_STR_MAX) {
        return NULL;
    }
    string = pphp_alloc(sizeof(*string) + length + 1U);
    if (string == NULL) {
        return NULL;
    }
    string->header.refcnt = 1U;
    string->header.type = PT_STRING;
    string->header.flags = 0U;
    string->length = (uint16_t)length;
    string->reserved = 0U;
    string->hash = ps_hash_bytes(bytes == NULL ? "" : bytes, length);
#if PPHP_RC_DEBUG
    pphp_alloc_track(string);
#endif
    storage = (char *)(string + 1);
    if (length != 0U) {
        memcpy(storage, bytes, length);
    }
    storage[length] = '\0';
    return string;
}

pstring *ps_new_cstr(const char *string) {
    if (string == NULL) {
        return NULL;
    }
    return ps_new(string, strlen(string));
}

int ps_equal(const pstring *left, const pstring *right) {
    if (left == right) {
        return 1;
    }
    if (left == NULL || right == NULL || left->length != right->length ||
        left->hash != right->hash) {
        return 0;
    }
    return memcmp(ps_data(left), ps_data(right), left->length) == 0;
}

int ps_equal_bytes(const pstring *string, const char *bytes, size_t length) {
    if (string == NULL || bytes == NULL || string->length != length) {
        return 0;
    }
    return memcmp(ps_data(string), bytes, length) == 0;
}

void ps_destroy(pstring *string) {
    if (string == NULL || string->header.type == PT_ROSTRING ||
        (string->header.flags & PSTRING_INTERNED) != 0U) {
        return;
    }
    pphp_free(string);
}
