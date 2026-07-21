#include "state.h"

#include "codegen.h"
#include "parser.h"
#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void discard_output(void *context, const char *bytes, size_t length) {
    (void)context;
    (void)bytes;
    (void)length;
}

pphp_state *pphp_open(void *pool, size_t pool_size) {
    pphp_state *state;
    if (pool != NULL) {
        pphp_pool_init(pool, pool_size);
    }
    state = pphp_alloc(sizeof(*state));
    if (state == NULL) {
        return NULL;
    }
    memset(state, 0, sizeof(*state));
    state->output = discard_output;
    if (!psymbol_init(&state->symbols, 64U)) {
        pphp_free(state);
        return NULL;
    }
    return state;
}

void pphp_close(pphp_state *state) {
    if (state == NULL) {
        return;
    }
    psymbol_destroy(&state->symbols);
    pphp_free(state);
}

void pphp_set_output(pphp_state *state, pphp_output_fn output, void *context) {
    if (state == NULL) {
        return;
    }
    state->output = output == NULL ? discard_output : output;
    state->output_context = context;
}

void pphp_output(pphp_state *state, const char *bytes, size_t length) {
    if (state != NULL && state->output != NULL && bytes != NULL && length != 0U) {
        state->output(state->output_context, bytes, length);
    }
}

void pphp_runtime_error(pphp_state *state, uint32_t line, const char *format, ...) {
    va_list arguments;
    if (state == NULL || state->error[0] != '\0') {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(state->error, sizeof(state->error), format, arguments);
    va_end(arguments);
    state->error_line = line;
}

int pphp_exec_source_mode(pphp_state *state, const char *source, size_t length,
                          const char *chunk_name, int repl) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pmodule module;
    pc_codegen_error codegen_error;
    int result;
    if (state == NULL || source == NULL) {
        return PPHP_E_RUNTIME;
    }
    state->error[0] = '\0';
    state->error_line = 0U;
    pc_arena_init(&arena, 4096U);
    pc_parser_init(&parser, &arena, source, length, repl);
    program = pc_parse_program(&parser);
    if (program == NULL) {
        pphp_runtime_error(state, pc_parser_error_line(&parser),
                           "Parse error: %s in %s", pc_parser_error(&parser),
                           chunk_name == NULL ? "<source>" : chunk_name);
        pc_arena_destroy(&arena);
        return PPHP_E_PARSE;
    }
    if (!pc_codegen_program(program, &module, &codegen_error)) {
        pphp_runtime_error(state, codegen_error.line,
                           "Compile error: %s in %s", codegen_error.message,
                           chunk_name == NULL ? "<source>" : chunk_name);
        pc_arena_destroy(&arena);
        return PPHP_E_PARSE;
    }
    pc_arena_destroy(&arena);
    result = pphp_vm_execute(state, &module);
    pmodule_destroy(&module);
    return result;
}

int pphp_exec_source(pphp_state *state, const char *source, size_t length,
                     const char *chunk_name) {
    return pphp_exec_source_mode(state, source, length, chunk_name, 0);
}

int pphp_exec_pbc(pphp_state *state, const void *pbc, size_t length) {
    pmodule module;
    int result;
    if (state == NULL) {
        return PPHP_E_RUNTIME;
    }
    state->error[0] = '\0';
    state->error_line = 0U;
    result = pphp_pbc_load(pbc, length, &module);
    if (result != PPHP_OK) {
        pphp_runtime_error(state, 0U, "invalid or incompatible PBC image");
        return result == PPHP_E_NOMEM ? PPHP_E_RUNTIME : PPHP_E_PARSE;
    }
    result = pphp_vm_execute(state, &module);
    pmodule_destroy(&module);
    return result;
}

void pphp_tick(pphp_state *state) {
    if (state != NULL) {
        state->ticks++;
    }
}

const char *pphp_last_error(const pphp_state *state) {
    return state == NULL ? "invalid state" : state->error;
}

uint32_t pphp_last_error_line(const pphp_state *state) {
    return state == NULL ? 0U : state->error_line;
}
