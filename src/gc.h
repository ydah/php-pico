#ifndef PPHP_GC_H
#define PPHP_GC_H

#include <stddef.h>

#include "state.h"

size_t pphp_gc_collect(pphp_state *state);

#endif
