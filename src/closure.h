#ifndef PPHP_CLOSURE_H
#define PPHP_CLOSURE_H

#include <stddef.h>

#include "pbc.h"
#include "value.h"

typedef struct pclosure {
    pheader header;
    const pproto *proto;
    uint8_t capture_count;
    pvalue captures[];
} pclosure;

pclosure *pclosure_new(const pproto *proto, const pvalue *captures,
                       size_t capture_count);
void pclosure_destroy(pclosure *closure);

#endif
