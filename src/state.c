#include "state.h"

#include "codegen.h"
#include "parser.h"
#include "vm.h"
#include "pclass.h"
#include "parray.h"

#include <stdarg.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void discard_output(void *context, const char *bytes, size_t length) {
    (void)context;
    (void)bytes;
    (void)length;
}

static int set_constant(pphp_state *state, const char *name, pvalue value) {
    pstring *key = psymbol_intern(&state->symbols, name, strlen(name));
    return key != NULL && pa_set(state->constants,
                                 pv_heap(PT_STRING, &key->header), value);
}

static int initialize_constants(pphp_state *state) {
    pstring *newline = ps_new("\n", 1U);
    pvalue newline_value;
    int ok;
    if (newline == NULL) return 0;
    newline_value = pv_heap(PT_STRING, &newline->header);
    ok = set_constant(state, "PHP_INT_MAX",
                      pv_int((pphp_int)(PPHP_INT64 ? INT64_MAX : INT32_MAX))) &&
         set_constant(state, "PHP_INT_SIZE", pv_int((pphp_int)sizeof(pphp_int))) &&
         set_constant(state, "PHP_FLOAT_EPSILON",
                      pv_float((pphp_float)(PPHP_USE_DOUBLE ? DBL_EPSILON
                                                           : FLT_EPSILON))) &&
         set_constant(state, "M_PI", pv_float((pphp_float)3.14159265358979323846)) &&
         set_constant(state, "NAN", pv_float((pphp_float)NAN)) &&
         set_constant(state, "INF", pv_float((pphp_float)INFINITY)) &&
         set_constant(state, "PHP_EOL", newline_value) &&
         set_constant(state, "JSON_PRETTY_PRINT", pv_int(128)) &&
         set_constant(state, "FILE_APPEND", pv_int(8));
    pv_release(newline_value);
    return ok;
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
    state->globals = pa_new(16U);
    state->statics = pa_new(8U);
    state->constants = pa_new(16U);
    if (state->globals == NULL || state->statics == NULL ||
        state->constants == NULL) {
        pa_destroy(state->globals);
        pa_destroy(state->statics);
        pa_destroy(state->constants);
        psymbol_destroy(&state->symbols);
        pphp_free(state);
        return NULL;
    }
    if (!initialize_constants(state)) {
        pa_destroy(state->globals);
        pa_destroy(state->statics);
        pa_destroy(state->constants);
        psymbol_destroy(&state->symbols);
        pphp_free(state);
        return NULL;
    }
    return state;
}

void pphp_close(pphp_state *state) {
    if (state == NULL) {
        return;
    }
    pphp_clear_classes(state);
    pa_destroy(state->globals);
    pa_destroy(state->statics);
    pa_destroy(state->constants);
    psymbol_destroy(&state->symbols);
    pphp_free(state);
}

static int class_name_equal(const pstring *name, const char *other, size_t length) {
    size_t i;
    if (name->length != length) return 0;
    for (i = 0U; i < length; i++) {
        unsigned char a = (unsigned char)name->data[i];
        unsigned char b = (unsigned char)other[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
        if (a != b) return 0;
    }
    return 1;
}

pclass *pphp_find_class(const pphp_state *state, const char *name, size_t length) {
    size_t i;
    if (state == NULL) return NULL;
    for (i = 0U; i < state->class_count; i++) {
        if (class_name_equal(state->classes[i]->name, name, length)) {
            return state->classes[i];
        }
    }
    return NULL;
}

int pphp_register_class(pphp_state *state, pclass *class_entry) {
    pclass **classes;
    size_t capacity;
    if (state == NULL || class_entry == NULL ||
        pphp_find_class(state, class_entry->name->data, class_entry->name->length) != NULL) {
        return 0;
    }
    if (state->class_count == state->class_capacity) {
        capacity = state->class_capacity == 0U ? 8U : state->class_capacity * 2U;
        classes = pphp_realloc(state->classes, capacity * sizeof(*classes));
        if (classes == NULL) return 0;
        state->classes = classes;
        state->class_capacity = capacity;
    }
    state->classes[state->class_count++] = class_entry;
    return 1;
}

void pphp_clear_classes(pphp_state *state) {
    size_t i;
    if (state == NULL) return;
    if (state->oom_exception != NULL) {
        pv_release(pv_heap(PT_OBJECT, &state->oom_exception->header));
        state->oom_exception = NULL;
    }
    if (state->building_class != NULL) {
        pclass_destroy(state->building_class);
        state->building_class = NULL;
    }
    for (i = state->class_count; i > 0U; i--) {
        pclass_destroy(state->classes[i - 1U]);
    }
    pphp_free(state->classes);
    state->classes = NULL;
    state->class_count = 0U;
    state->class_capacity = 0U;
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
    state->chunk_name = chunk_name == NULL ? "<source>" : chunk_name;
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
    pphp_clear_classes(state);
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
    state->chunk_name = "<pbc>";
    result = pphp_pbc_load(pbc, length, &module);
    if (result != PPHP_OK) {
        pphp_runtime_error(state, 0U, "invalid or incompatible PBC image");
        return result == PPHP_E_NOMEM ? PPHP_E_RUNTIME : PPHP_E_PARSE;
    }
    result = pphp_vm_execute(state, &module);
    pphp_clear_classes(state);
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
