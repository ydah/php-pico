#ifndef PPHP_STATE_H
#define PPHP_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "pbc.h"
#include "pphp/pphp.h"
#include "symbol.h"

typedef struct pframe {
    const pproto *proto;
    size_t pc;
    size_t base;
    uint32_t line;
    pvalue return_override;
    int has_return_override;
    pclass *called_scope;
} pframe;

struct pphp_state {
    pvalue stack[PPHP_STACK_SLOTS];
    pframe frames[PPHP_FRAME_MAX];
    size_t stack_count;
    size_t frame_count;
    const pmodule *module;
    psymbol_table symbols;
    pclass **classes;
    size_t class_count;
    size_t class_capacity;
    pclass *building_class;
    pobject *oom_exception;
    pvalue pending_exception;
    int has_pending_exception;
    pphp_output_fn output;
    void *output_context;
    const char *chunk_name;
    uint32_t error_line;
    uint32_t ticks;
    char error[256];
};

void pphp_output(pphp_state *state, const char *bytes, size_t length);
void pphp_runtime_error(pphp_state *state, uint32_t line, const char *format, ...);
int pphp_exec_source_mode(pphp_state *state, const char *source, size_t length,
                          const char *chunk_name, int repl);
pclass *pphp_find_class(const pphp_state *state, const char *name, size_t length);
int pphp_register_class(pphp_state *state, pclass *class_entry);
void pphp_clear_classes(pphp_state *state);

#endif
