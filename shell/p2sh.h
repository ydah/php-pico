#ifndef PPHP_P2SH_H
#define PPHP_P2SH_H

#include <stdio.h>

#include "state.h"

int pphp_host_repl(pphp_state *state, FILE *input, FILE *output,
                   FILE *errors);
int pphp_host_shell(pphp_state *state, FILE *input, FILE *output,
                    FILE *errors);

#endif
