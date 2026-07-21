#ifndef PPHP_STATE_H
#define PPHP_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "pbc.h"
#include "parray.h"
#include "pphp/pphp.h"
#include "symbol.h"

typedef struct pframe {
    const pproto *proto;
    const pmodule *module;
    size_t pc;
    size_t base;
    uint32_t line;
    uint8_t argument_count;
    uint8_t all_globals;
    uint8_t global_mask[32];
    uint8_t static_mask[32];
    pvalue return_override;
    int has_return_override;
    pclass *called_scope;
    pclass *called_class;
} pframe;

typedef struct pnative_function {
    pstring *name;
    pphp_cfunc function;
    int minimum_arguments;
    int maximum_arguments;
} pnative_function;

struct pphp_ctx {
    pphp_state *state;
    pobject *this_object;
    const pvalue *arguments;
    size_t argument_count;
    pvalue result;
    pstring *temporaries[31];
    size_t temporary_count;
    int failed;
};

struct pphp_state {
    pvalue stack[PPHP_STACK_SLOTS];
    pframe frames[PPHP_FRAME_MAX];
    size_t stack_count;
    size_t frame_count;
    const pmodule *module;
    pmodule **repl_modules;
    size_t repl_module_count;
    size_t repl_module_capacity;
    psymbol_table symbols;
    parray *globals;
    parray *statics;
    parray *constants;
    parray *included_files;
    pclass **classes;
    size_t class_count;
    size_t class_capacity;
    pclass *building_class;
    pobject *gc_objects;
    struct pphp_state *root_state;
    pnative_function *native_functions;
    size_t native_function_count;
    size_t native_function_capacity;
    pobject *oom_exception;
    pphp_output_fn output;
    void *output_context;
    const char *chunk_name;
    uint32_t error_line;
    uint32_t ticks;
    uint32_t random_state;
    int exit_requested;
    int exit_status;
    int repl_mode;
    int capture_halt_result;
    pvalue *halt_result;
    int (*invoke)(struct pphp_state *state, pvalue callable,
                  const pvalue *arguments, size_t count, pvalue *result);
    char error[256];
    char raised_class[48];
};

void pphp_output(pphp_state *state, const char *bytes, size_t length);
void pphp_runtime_error(pphp_state *state, uint32_t line, const char *format, ...);
int pphp_exec_source_mode(pphp_state *state, const char *source, size_t length,
                          const char *chunk_name, int repl);
int pphp_exec_include(pphp_state *state, const char *path, uint8_t mode,
                      pvalue *result);
pclass *pphp_find_class(const pphp_state *state, const char *name, size_t length);
int pphp_register_class(pphp_state *state, pclass *class_entry);
void pphp_clear_classes(pphp_state *state);
void pphp_clear_user_classes(pphp_state *state);
const pproto *pphp_find_function(const pphp_state *state, const pstring *name,
                                 const pmodule **owner);
int pphp_native_function_exists(const pphp_state *state,
                                const pstring *name);
int pphp_call_native_function(pphp_state *state, const pstring *name,
                              const pvalue *arguments, size_t count,
                              pvalue *result);
int pphp_call_native_method(pphp_state *state, pphp_cfunc function,
                            pobject *this_object, const pvalue *arguments,
                            size_t count, pvalue *result);

#endif
