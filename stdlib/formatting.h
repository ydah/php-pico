#ifndef PPHP_STDLIB_FORMATTING_H
#define PPHP_STDLIB_FORMATTING_H

#include "state.h"

int pphp_call_formatting_builtin(pphp_state *state, const pstring *name,
                                 const pvalue *arguments, size_t count,
                                 pvalue *result);

#endif
