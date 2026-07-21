#ifndef PPHP_P2SH_DEVICE_H
#define PPHP_P2SH_DEVICE_H

#include "pphp/pphp.h"

enum {
    PPHP_P2SH_DONE = 0,
    PPHP_P2SH_ENTER_REPL = 1
};

void pphp_p2sh_init(void);
int pphp_p2sh_execute(pphp_state *state, char *line);
int pphp_p2sh_run_file(pphp_state *state, const char *path);

#endif
