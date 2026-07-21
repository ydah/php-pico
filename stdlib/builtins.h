#ifndef PPHP_BUILTINS_H
#define PPHP_BUILTINS_H

#include <stddef.h>

#include "state.h"

int pphp_call_builtin(pphp_state *state, const pstring *name,
                      const pvalue *arguments, size_t count, pvalue *result);
int pphp_builtin_exists(const pstring *name);

#endif
