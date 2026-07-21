#ifndef PPHP_GC_H
#define PPHP_GC_H

#include <stddef.h>

#include "state.h"

size_t pphp_gc_collect(pphp_state *state);
void pphp_gc_set_state(pphp_state *state);
void pphp_gc_buffer(pheader *header);
void pphp_gc_unbuffer(pheader *header);
void pphp_gc_maybe_collect(pphp_state *state);

#endif
