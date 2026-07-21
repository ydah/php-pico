#include "state.h"

#include "codegen.h"
#include "parser.h"
#include "vm.h"
#include "pclass.h"
#include "parray.h"
#include "pphp/hal.h"
#include "pphp/fs.h"
#include "gc.h"
#include "files.h"
#include "pgems.h"

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
         set_constant(state, "FILE_APPEND", pv_int(8)) &&
         set_constant(state, "ARRAY_FILTER_USE_BOTH", pv_int(1)) &&
         set_constant(state, "ARRAY_FILTER_USE_KEY", pv_int(2)) &&
         set_constant(state, "SORT_REGULAR", pv_int(0)) &&
         set_constant(state, "SORT_NUMERIC", pv_int(1)) &&
         set_constant(state, "SORT_STRING", pv_int(2)) &&
         set_constant(state, "SEEK_SET", pv_int(0)) &&
         set_constant(state, "SEEK_CUR", pv_int(1)) &&
         set_constant(state, "SEEK_END", pv_int(2));
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
    state->root_state = state;
    state->output = discard_output;
    state->invoke = pphp_vm_invoke;
    state->random_state = hal_random();
    if (!psymbol_init(&state->symbols, 64U)) {
        pphp_free(state);
        return NULL;
    }
    state->globals = pa_new(16U);
    state->statics = pa_new(8U);
    state->constants = pa_new(16U);
    state->included_files = pa_new(8U);
    if (state->globals == NULL || state->statics == NULL ||
        state->constants == NULL || state->included_files == NULL) {
        pa_destroy(state->globals);
        pa_destroy(state->statics);
        pa_destroy(state->constants);
        pa_destroy(state->included_files);
        psymbol_destroy(&state->symbols);
        pphp_free(state);
        return NULL;
    }
    if (!initialize_constants(state)) {
        pa_destroy(state->globals);
        pa_destroy(state->statics);
        pa_destroy(state->constants);
        pa_destroy(state->included_files);
        psymbol_destroy(&state->symbols);
        pphp_free(state);
        return NULL;
    }
    if (!pphp_init_pgems(state)) {
        pphp_close(state);
        return NULL;
    }
    pphp_gc_set_state(state);
    return state;
}

void pphp_close(pphp_state *state) {
    size_t i;
    if (state == NULL) {
        return;
    }
    pa_destroy(state->globals);
    pa_destroy(state->statics);
    pa_destroy(state->constants);
    pa_destroy(state->included_files);
    (void)pphp_gc_collect(state);
    pphp_clear_classes(state);
    for (i = 0U; i < state->repl_module_count; i++) {
        pmodule_destroy(state->repl_modules[i]);
        pphp_free(state->repl_modules[i]);
    }
    pphp_free(state->repl_modules);
    for (i = 0U; i < state->native_function_count; i++) {
        ps_destroy(state->native_functions[i].name);
    }
    pphp_free(state->native_functions);
    pphp_free(state->runtime_functions);
    psymbol_destroy(&state->symbols);
    pphp_gc_set_state(NULL);
    pphp_free(state);
}

static int function_name_equal(const pstring *left, const pstring *right) {
    size_t i;
    if (left->length != right->length) return 0;
    for (i = 0U; i < left->length; i++) {
        unsigned char a = (unsigned char)left->data[i];
        unsigned char b = (unsigned char)right->data[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
        if (a != b) return 0;
    }
    return 1;
}

const pproto *pphp_find_function(const pphp_state *state, const pstring *name,
                                 const pmodule **owner) {
    const pphp_state *root;
    const pproto *proto;
    size_t i;
    if (owner != NULL) *owner = NULL;
    if (state == NULL || name == NULL) return NULL;
    root = state->root_state == NULL ? state : state->root_state;
    for (i = root->runtime_function_count; i > 0U; i--) {
        const pruntime_function *entry = &root->runtime_functions[i - 1U];
        if (function_name_equal(entry->proto->name, name)) {
            if (owner != NULL) *owner = entry->module;
            return entry->proto;
        }
    }
    if (state->module != NULL &&
        (proto = pmodule_find(state->module, name)) != NULL) {
        if (owner != NULL) *owner = state->module;
        return proto;
    }
    for (i = state->repl_module_count; i > 0U; i--) {
        const pmodule *module = state->repl_modules[i - 1U];
        if (module == state->module) continue;
        proto = pmodule_find(module, name);
        if (proto != NULL) {
            if (owner != NULL) *owner = module;
            return proto;
        }
    }
    return NULL;
}

int pphp_register_runtime_function(pphp_state *state, const pproto *proto,
                                   const pmodule *module) {
    pphp_state *root;
    pruntime_function *resized;
    size_t capacity;
    if (state == NULL || proto == NULL || module == NULL) return 0;
    root = state->root_state == NULL ? state : state->root_state;
    if (root->runtime_function_count >= 512U) return 0;
    if (root->runtime_function_count == root->runtime_function_capacity) {
        capacity = root->runtime_function_capacity == 0U
                       ? 8U : root->runtime_function_capacity * 2U;
        resized = pphp_realloc(root->runtime_functions,
                               capacity * sizeof(*resized));
        if (resized == NULL) return 0;
        root->runtime_functions = resized;
        root->runtime_function_capacity = capacity;
    }
    root->runtime_functions[root->runtime_function_count].proto = proto;
    root->runtime_functions[root->runtime_function_count].module = module;
    root->runtime_function_count++;
    return 1;
}

void pphp_remove_module_functions(pphp_state *state, const pmodule *module) {
    pphp_state *root;
    size_t read_index;
    size_t write_index = 0U;
    if (state == NULL || module == NULL) return;
    root = state->root_state == NULL ? state : state->root_state;
    for (read_index = 0U; read_index < root->runtime_function_count;
         read_index++) {
        if (root->runtime_functions[read_index].module != module) {
            root->runtime_functions[write_index++] =
                root->runtime_functions[read_index];
        }
    }
    root->runtime_function_count = write_index;
}

static int retain_repl_module(pphp_state *state, pmodule *module,
                              pmodule **owned) {
    pmodule **resized;
    size_t capacity;
    *owned = pphp_alloc(sizeof(**owned));
    if (*owned == NULL) return 0;
    if (state->repl_module_count == state->repl_module_capacity) {
        capacity = state->repl_module_capacity == 0U
                       ? 8U : state->repl_module_capacity * 2U;
        resized = pphp_realloc(state->repl_modules,
                               capacity * sizeof(*resized));
        if (resized == NULL) {
            pphp_free(*owned);
            *owned = NULL;
            return 0;
        }
        state->repl_modules = resized;
        state->repl_module_capacity = capacity;
    }
    **owned = *module;
    memset(module, 0, sizeof(*module));
    state->repl_modules[state->repl_module_count++] = *owned;
    return 1;
}

static int validate_repl_functions(pphp_state *state, const pmodule *module) {
    size_t i;
    for (i = 1U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        if (proto->is_method || proto->conditional ||
            proto->name->length == 0U ||
            proto->name->data[0] == '{' ||
            strstr(proto->name->data, "::") != NULL) continue;
        if (pphp_find_function(state, proto->name, NULL) != NULL) {
            pphp_runtime_error(state, 0U, "function %.*s is already defined",
                               (int)proto->name->length, proto->name->data);
            return 0;
        }
    }
    return 1;
}

static int include_candidate(const char *prefix, const char *path,
                             const char *suffix, char *resolved) {
    char candidate[PPHP_FS_PATH_MAX];
    size_t prefix_length = prefix == NULL ? 0U : strlen(prefix);
    size_t path_length = strlen(path);
    size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);
    if (prefix_length + path_length + suffix_length + 1U >
        sizeof(candidate)) return 0;
    if (prefix_length != 0U) memcpy(candidate, prefix, prefix_length);
    memcpy(candidate + prefix_length, path, path_length);
    if (suffix_length != 0U) {
        memcpy(candidate + prefix_length + path_length, suffix, suffix_length);
    }
    candidate[prefix_length + path_length + suffix_length] = '\0';
    if (!pphp_fs_exists(candidate)) return 0;
    if (pphp_fs_canonicalize(candidate, resolved, PPHP_FS_PATH_MAX)) return 1;
    memcpy(resolved, candidate,
           prefix_length + path_length + suffix_length + 1U);
    return 1;
}

static int has_php_extension(const char *path) {
    size_t length = strlen(path);
    return (length >= 4U && strcmp(path + length - 4U, ".php") == 0) ||
           (length >= 4U && strcmp(path + length - 4U, ".pbc") == 0);
}

static int resolve_include_path(const char *path, char *resolved) {
    static const char *const prefixes[] = {"/lib/", "/home/"};
    static const char *const suffixes[] = {".pbc", ".php"};
    size_t i;
    size_t j;
    int extension;
    if (path == NULL || *path == '\0' || strlen(path) >= PPHP_FS_PATH_MAX) {
        return 0;
    }
    if (include_candidate(NULL, path, NULL, resolved)) return 1;
    extension = has_php_extension(path);
    if (!extension) {
        for (j = 0U; j < sizeof(suffixes) / sizeof(suffixes[0]); j++) {
            if (include_candidate(NULL, path, suffixes[j], resolved)) return 1;
        }
    }
    if (*path == '/') return 0;
    for (i = 0U; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (extension && include_candidate(prefixes[i], path, NULL, resolved)) {
            return 1;
        }
        if (!extension) {
            for (j = 0U; j < sizeof(suffixes) / sizeof(suffixes[0]); j++) {
                if (include_candidate(prefixes[i], path, suffixes[j],
                                      resolved)) return 1;
            }
        }
    }
    return 0;
}

int pphp_exec_include(pphp_state *state, const char *path, uint8_t mode,
                      pvalue *result) {
    pphp_state *owner;
    pstring *path_string;
    pvalue path_value;
    pvalue existing = pv_null();
    char *bytes = NULL;
    size_t length = 0U;
    pmodule module;
    pmodule *execution_module = &module;
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pc_codegen_error codegen_error;
    pphp_state child;
    int once = mode == (uint8_t)T_INCLUDE_ONCE ||
               mode == (uint8_t)T_REQUIRE_ONCE;
    int required = mode == (uint8_t)T_REQUIRE ||
                   mode == (uint8_t)T_REQUIRE_ONCE;
    int status;
    char resolved[PPHP_FS_PATH_MAX];
    if (state == NULL || path == NULL || result == NULL) return PPHP_E_RUNTIME;
    *result = pv_bool(0);
    owner = state->root_state == NULL ? state : state->root_state;
    if (!resolve_include_path(path, resolved)) {
        if (required) {
            pphp_runtime_error(state, 0U,
                               "required file %s could not be opened", path);
        }
        return required ? PPHP_E_RUNTIME : PPHP_OK;
    }
    path = resolved;
    path_string = ps_new(path, strlen(path));
    if (path_string == NULL) {
        pphp_runtime_error(state, 0U, "out of memory resolving include path");
        return PPHP_E_NOMEM;
    }
    path_value = pv_heap(PT_STRING, &path_string->header);
    if (once && pa_get(owner->included_files, path_value, &existing)) {
        pv_release(existing);
        pv_release(path_value);
        *result = pv_bool(1);
        return PPHP_OK;
    }
    if (!pphp_file_read_all(path, &bytes, &length)) {
        if (required) {
            pphp_runtime_error(state, 0U,
                               "required file %s could not be opened", path);
        }
        pv_release(path_value);
        return required ? PPHP_E_RUNTIME : PPHP_OK;
    }
    if (length >= 4U && memcmp(bytes, "PPBC", 4U) == 0) {
        status = pphp_pbc_load(bytes, length, &module);
        if (status != PPHP_OK) {
            pphp_runtime_error(state, 0U,
                               "included file %s is not valid PBC", path);
            pphp_free(bytes);
            pv_release(path_value);
            return status;
        }
        module.backing = bytes;
        module.owns_backing = 1U;
        bytes = NULL;
    } else {
        pc_arena_init(&arena, 4096U);
        pc_parser_init(&parser, &arena, bytes, length, 0);
        program = pc_parse_program(&parser);
        if (program == NULL) {
            pphp_runtime_error(state, pc_parser_error_line(&parser),
                               "Parse error: %s in %s",
                               pc_parser_error(&parser), path);
            pc_arena_destroy(&arena);
            pphp_free(bytes);
            pv_release(path_value);
            return PPHP_E_PARSE;
        }
        if (!pc_codegen_program(program, &module, &codegen_error)) {
            pphp_runtime_error(state, codegen_error.line,
                               "Compile error: %s in %s",
                               codegen_error.message, path);
            pc_arena_destroy(&arena);
            pphp_free(bytes);
            pv_release(path_value);
            return PPHP_E_PARSE;
        }
        pc_arena_destroy(&arena);
    }
    pphp_free(bytes);
    if (!validate_repl_functions(owner, &module)) {
        pmodule_destroy(&module);
        pv_release(path_value);
        (void)snprintf(state->error, sizeof(state->error), "%s", owner->error);
        state->error_line = owner->error_line;
        return PPHP_E_RUNTIME;
    }
    if (!retain_repl_module(owner, &module, &execution_module)) {
        pmodule_destroy(&module);
        pv_release(path_value);
        pphp_runtime_error(state, 0U,
                           "out of memory retaining included definitions");
        return PPHP_E_NOMEM;
    }
    if (once && !pa_set(owner->included_files, path_value, pv_bool(1))) {
        pv_release(path_value);
        pphp_runtime_error(state, 0U, "out of memory tracking included file");
        return PPHP_E_NOMEM;
    }
    pv_release(path_value);
    child = *owner;
    child.stack_count = 0U;
    child.frame_count = 0U;
    child.module = NULL;
    child.root_state = owner;
    child.repl_mode = 1;
    child.capture_halt_result = 0;
    child.halt_result = NULL;
    child.exit_requested = 0;
    child.exit_status = 0;
    child.error[0] = '\0';
    child.error_line = 0U;
    child.chunk_name = path;
    status = pphp_vm_execute(&child, execution_module);
    owner->classes = child.classes;
    owner->class_count = child.class_count;
    owner->class_capacity = child.class_capacity;
    owner->random_state = child.random_state;
    if (child.exit_requested) {
        owner->exit_requested = 1;
        owner->exit_status = child.exit_status;
        state->exit_requested = 1;
        state->exit_status = child.exit_status;
    }
    if (status != PPHP_OK) {
        (void)snprintf(state->error, sizeof(state->error), "%s", child.error);
        state->error_line = child.error_line;
        return status;
    }
    *result = pv_bool(1);
    return PPHP_OK;
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

void pphp_clear_user_classes(pphp_state *state) {
    size_t read_index;
    if (state == NULL) return;
    if (state->oom_exception != NULL &&
        !state->oom_exception->class_entry->persistent) {
        pv_release(pv_heap(PT_OBJECT, &state->oom_exception->header));
        state->oom_exception = NULL;
    }
    if (state->building_class != NULL && !state->building_class->persistent) {
        pclass_destroy(state->building_class);
        state->building_class = NULL;
    }
    for (read_index = state->class_count; read_index > 0U; read_index--) {
        pclass *class_entry = state->classes[read_index - 1U];
        if (!class_entry->persistent) {
            pclass_destroy(class_entry);
            if (read_index < state->class_count) {
                memmove(state->classes + read_index - 1U,
                        state->classes + read_index,
                        (state->class_count - read_index) *
                            sizeof(*state->classes));
            }
            state->class_count--;
        }
    }
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
    pmodule *execution_module = &module;
    if (state == NULL || source == NULL) {
        return PPHP_E_RUNTIME;
    }
    state->error[0] = '\0';
    state->raised_class[0] = '\0';
    state->error_line = 0U;
    state->exit_requested = 0;
    state->exit_status = 0;
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
    state->repl_mode = repl;
    if (repl && !validate_repl_functions(state, &module)) {
        pmodule_destroy(&module);
        return PPHP_E_RUNTIME;
    }
    if (repl && !retain_repl_module(state, &module, &execution_module)) {
        pmodule_destroy(&module);
        pphp_runtime_error(state, 0U,
                           "out of memory retaining REPL definitions");
        return PPHP_E_RUNTIME;
    }
    result = pphp_vm_execute(state, execution_module);
    if (!repl) {
        pphp_remove_module_functions(state, execution_module);
        pphp_clear_user_classes(state);
        pmodule_destroy(&module);
        state->module = NULL;
    }
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
    state->raised_class[0] = '\0';
    state->error_line = 0U;
    state->exit_requested = 0;
    state->exit_status = 0;
    state->chunk_name = "<pbc>";
    state->repl_mode = 0;
    result = pphp_pbc_load(pbc, length, &module);
    if (result != PPHP_OK) {
        pphp_runtime_error(state, 0U, "invalid or incompatible PBC image");
        return result == PPHP_E_NOMEM ? PPHP_E_RUNTIME : PPHP_E_PARSE;
    }
    result = pphp_vm_execute(state, &module);
    pphp_remove_module_functions(state, &module);
    pphp_clear_user_classes(state);
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

int pphp_exit_requested(const pphp_state *state) {
    return state != NULL && state->exit_requested;
}

int pphp_exit_status(const pphp_state *state) {
    return state == NULL ? 0 : state->exit_status;
}
