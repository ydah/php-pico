#ifndef PPHP_DISASM_H
#define PPHP_DISASM_H

#include <stdio.h>

#include "pbc.h"

int pphp_disassemble_module(FILE *stream, const pmodule *module);

#endif

