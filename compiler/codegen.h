#ifndef PPHP_CODEGEN_H
#define PPHP_CODEGEN_H

#include <stdint.h>

#include "ast.h"
#include "pbc.h"

typedef struct pc_codegen_error {
    uint32_t line;
    char message[192];
} pc_codegen_error;

int pc_codegen_program(const pc_ast *program, pmodule *module,
                       pc_codegen_error *error);

#endif

