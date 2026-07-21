#ifndef PPHP_VM_H
#define PPHP_VM_H

#include "state.h"

int pphp_vm_execute(pphp_state *state, const pmodule *module);
int pphp_vm_invoke(pphp_state *state, pvalue callable,
                   const pvalue *arguments, size_t count, pvalue *result);

#endif
