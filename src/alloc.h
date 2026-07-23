#ifndef PPHP_ALLOC_H
#define PPHP_ALLOC_H

#include <stddef.h>

#include "pphp/pphp.h"

int pphp_pool_check(void);

#if PPHP_RC_DEBUG
typedef int (*pphp_tracked_visit_fn)(pheader *header, void *context);
void pphp_alloc_track(void *ptr);
int pphp_alloc_visit_tracked(pphp_tracked_visit_fn visit, void *context);
#endif

#endif
