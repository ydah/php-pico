#ifndef PPHP_STDLIB_SYSTEM_H
#define PPHP_STDLIB_SYSTEM_H

#include "state.h"

int pphp_call_system_builtin(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result);

#endif
