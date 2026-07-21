#ifndef PPHP_VALUE_H
#define PPHP_VALUE_H

#include <stddef.h>
#include <stdint.h>

#include "pphp/pphp_config.h"

typedef enum pvalue_type {
    PT_NULL = 0,
    PT_FALSE,
    PT_TRUE,
    PT_INT,
    PT_FLOAT,
    PT_STRING,
    PT_ARRAY,
    PT_OBJECT,
    PT_CLOSURE,
    PT_CFUNC,
    PT_RESOURCE,
    PT_ROSTRING
} pvalue_type;

typedef struct pheader {
    uint16_t refcnt;
    uint8_t type;
    uint8_t flags;
} pheader;

typedef struct pvalue {
    uint8_t type;
    uint8_t reserved[7];
    union {
        pphp_int i;
        pphp_float f;
        pheader *gc;
        const void *ptr;
    } as;
} pvalue;

pvalue pv_null(void);
pvalue pv_bool(int value);
pvalue pv_int(pphp_int value);
pvalue pv_float(pphp_float value);
pvalue pv_heap(pvalue_type type, pheader *header);
int pv_is_truthy(pvalue value);
void pv_retain(pvalue value);
void pv_release(pvalue value);
const char *pv_type_name(pvalue_type type);

#endif

