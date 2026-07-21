#ifndef PPHP_FILES_H
#define PPHP_FILES_H

#include "state.h"

int pphp_call_file_builtin(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result);
int pphp_file_builtin_exists(const pstring *name);
int pphp_file_read_all(const char *path, char **bytes, size_t *length);

#endif
