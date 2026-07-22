#include "state.h"

#if PPHP_ENABLE_COMPILER
#include "codegen.h"
#include "parser.h"
#endif
#include "vm.h"
#include "closure.h"
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

/* PBC v2 stores these lexer token values as the OP_INCLUDE operand. */
enum {
    PPHP_PBC_INCLUDE = 113,
    PPHP_PBC_INCLUDE_ONCE = 114,
    PPHP_PBC_REQUIRE = 129,
    PPHP_PBC_REQUIRE_ONCE = 130
};

#if PPHP_ENABLE_COMPILER
typedef char pphp_include_modes_match_pbc_v2[
    T_INCLUDE == PPHP_PBC_INCLUDE &&
    T_INCLUDE_ONCE == PPHP_PBC_INCLUDE_ONCE &&
    T_REQUIRE == PPHP_PBC_REQUIRE &&
    T_REQUIRE_ONCE == PPHP_PBC_REQUIRE_ONCE ? 1 : -1];
#endif

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
                      pv_int(PPHP_INT_MAXIMUM)) &&
         set_constant(state, "PHP_INT_SIZE", pv_int((pphp_int)sizeof(pphp_int))) &&
#if PPHP_ENABLE_FLOAT
         set_constant(state, "PHP_FLOAT_EPSILON",
                      pv_float((pphp_float)(PPHP_USE_DOUBLE ? DBL_EPSILON
                                                           : FLT_EPSILON))) &&
         set_constant(state, "M_PI", pv_float((pphp_float)3.14159265358979323846)) &&
         set_constant(state, "NAN", pv_float((pphp_float)NAN)) &&
         set_constant(state, "INF", pv_float((pphp_float)INFINITY)) &&
#endif
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
    pobject *object;
    if (state == NULL) {
        return;
    }
    object = state->gc_objects;
    if (object != NULL) {
        pv_retain(pv_heap(PT_OBJECT, &object->header));
    }
    while (object != NULL) {
        pobject *next = object->gc_next;
        if (next != NULL) pv_retain(pv_heap(PT_OBJECT, &next->header));
        pobject_run_destructor(object);
        pv_release(pv_heap(PT_OBJECT, &object->header));
        object = next;
    }
    pa_destroy(state->globals);
    pa_destroy(state->statics);
    pa_destroy(state->constants);
    pa_destroy(state->included_files);
    (void)pphp_gc_collect(state);
    pphp_clear_classes(state);
    for (i = 0U; i < state->runtime_function_count; i++) {
        pmodule_release((pmodule *)state->runtime_functions[i].module);
    }
    for (i = 0U; i < state->repl_module_count; i++) {
        pmodule_release(state->repl_modules[i]);
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
        unsigned char a = (unsigned char)ps_data(left)[i];
        unsigned char b = (unsigned char)ps_data(right)[i];
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
    pmodule_retain((pmodule *)module);
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
        } else {
            pmodule_release((pmodule *)module);
        }
    }
    root->runtime_function_count = write_index;
}

static int move_module(pmodule *module, pmodule **owned) {
    size_t i;
    *owned = pphp_alloc(sizeof(**owned));
    if (*owned == NULL) return 0;
    **owned = *module;
    (*owned)->heap_allocated = 1U;
    for (i = 0U; i < (*owned)->ro_string_count; i++) {
        (*owned)->ro_strings[i].owner = *owned;
    }
    memset(module, 0, sizeof(*module));
    return 1;
}

static int retain_module(pphp_state *state, pmodule *module,
                         pmodule **owned) {
    pmodule **resized;
    size_t capacity;
    if (state->repl_module_count == state->repl_module_capacity) {
        capacity = state->repl_module_capacity == 0U
                       ? 8U : state->repl_module_capacity * 2U;
        resized = pphp_realloc(state->repl_modules,
                               capacity * sizeof(*resized));
        if (resized == NULL) {
            return 0;
        }
        state->repl_modules = resized;
        state->repl_module_capacity = capacity;
    }
    if (!move_module(module, owned)) return 0;
    state->repl_modules[state->repl_module_count++] = *owned;
    return 1;
}

static void release_retained_module(pphp_state *state, pmodule *module) {
    size_t i;
    if (state == NULL || module == NULL) return;
    for (i = 0U; i < state->repl_module_count; i++) {
        if (state->repl_modules[i] != module) continue;
        if (i + 1U < state->repl_module_count) {
            memmove(state->repl_modules + i, state->repl_modules + i + 1U,
                    (state->repl_module_count - i - 1U) *
                        sizeof(*state->repl_modules));
        }
        state->repl_module_count--;
        pmodule_release(module);
        return;
    }
}

static int module_has_persistent_functions(const pmodule *module) {
    size_t i;
    if (module == NULL) return 0;
    for (i = 1U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        if (!proto->is_method && !proto->conditional &&
            proto->name->length != 0U && ps_data(proto->name)[0] != '{' &&
            strstr(ps_data(proto->name), "::") == NULL) return 1;
    }
    return 0;
}

static int validate_repl_functions(pphp_state *state, const pmodule *module) {
    size_t i;
    for (i = 1U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        if (proto->is_method || proto->conditional ||
            proto->name->length == 0U ||
            ps_data(proto->name)[0] == '{' ||
            strstr(ps_data(proto->name), "::") != NULL) continue;
        if (pphp_find_function(state, proto->name, NULL) != NULL) {
            pphp_runtime_error(state, 0U, "function %.*s is already defined",
                               (int)proto->name->length, ps_data(proto->name));
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
#if PPHP_ENABLE_COMPILER
    static const char *const suffixes[] = {".pbc", ".php"};
#else
    static const char *const suffixes[] = {".pbc"};
#endif
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
#if PPHP_ENABLE_COMPILER
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pc_codegen_error codegen_error;
#endif
    pphp_state child;
    int once = mode == (uint8_t)PPHP_PBC_INCLUDE_ONCE ||
               mode == (uint8_t)PPHP_PBC_REQUIRE_ONCE;
    int required = mode == (uint8_t)PPHP_PBC_REQUIRE ||
                   mode == (uint8_t)PPHP_PBC_REQUIRE_ONCE;
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
#if PPHP_ENABLE_COMPILER
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
#else
        pphp_runtime_error(state, 0U,
                           "source compiler is disabled; include requires PBC: %s",
                           path);
        pphp_free(bytes);
        pv_release(path_value);
        return PPHP_E_PARSE;
#endif
    }
    pphp_free(bytes);
    if (!validate_repl_functions(owner, &module)) {
        pmodule_destroy(&module);
        pv_release(path_value);
        (void)snprintf(state->error, sizeof(state->error), "%s", owner->error);
        state->error_line = owner->error_line;
        return PPHP_E_RUNTIME;
    }
    if (!retain_module(owner, &module, &execution_module)) {
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
        unsigned char a = (unsigned char)ps_data(name)[i];
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
        pphp_find_class(state, ps_data(class_entry->name), class_entry->name->length) != NULL) {
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
        pclass_release_values(state->building_class);
    }
    for (i = 0U; i < state->class_count; i++) {
        pclass_release_values(state->classes[i]);
    }
    (void)pphp_gc_collect(state);
    if (state->building_class != NULL) {
        pclass_release(state->building_class);
        state->building_class = NULL;
    }
    for (i = state->class_count; i > 0U; i--) {
        pclass_release(state->classes[i - 1U]);
    }
    pphp_free(state->classes);
    state->classes = NULL;
    state->class_count = 0U;
    state->class_capacity = 0U;
}

typedef struct class_reachability {
    pphp_state *state;
    uint8_t *reachable_classes;
    pclass **class_queue;
    size_t class_queue_count;
    size_t class_queue_cursor;
    pheader **containers;
    size_t container_count;
    size_t container_capacity;
    size_t container_cursor;
    int failed;
} class_reachability;

static void reachability_destroy(class_reachability *reachability) {
    if (reachability == NULL) return;
    pphp_free(reachability->reachable_classes);
    pphp_free(reachability->class_queue);
    pphp_free(reachability->containers);
    memset(reachability, 0, sizeof(*reachability));
}

static int reachability_class_index(const class_reachability *reachability,
                                    const pclass *class_entry,
                                    size_t *index) {
    size_t i;
    if (reachability == NULL || class_entry == NULL) return 0;
    for (i = 0U; i < reachability->state->class_count; i++) {
        if (reachability->state->classes[i] == class_entry) {
            if (index != NULL) *index = i;
            return 1;
        }
    }
    return 0;
}

static void reachability_mark_class(class_reachability *reachability,
                                    pclass *class_entry) {
    size_t index;
    if (reachability == NULL || reachability->failed || class_entry == NULL ||
        !reachability_class_index(reachability, class_entry, &index) ||
        reachability->reachable_classes[index]) return;
    reachability->reachable_classes[index] = 1U;
    reachability->class_queue[reachability->class_queue_count++] = class_entry;
}

static int reachability_has_container(const class_reachability *reachability,
                                      const pheader *header) {
    size_t i;
    if (reachability == NULL || header == NULL) return 0;
    for (i = 0U; i < reachability->container_count; i++) {
        if (reachability->containers[i] == header) return 1;
    }
    return 0;
}

static void reachability_mark_value(class_reachability *reachability,
                                    pvalue value) {
    pheader **resized;
    size_t capacity;
    if (reachability == NULL || reachability->failed || value.as.gc == NULL ||
        (value.type != PT_ARRAY && value.type != PT_OBJECT &&
         value.type != PT_CLOSURE) ||
        reachability_has_container(reachability, value.as.gc)) return;
    if (reachability->container_count == reachability->container_capacity) {
        capacity = reachability->container_capacity == 0U
                       ? 16U : reachability->container_capacity * 2U;
        if (capacity < reachability->container_capacity ||
            capacity > SIZE_MAX / sizeof(*resized)) {
            reachability->failed = 1;
            return;
        }
        resized = pphp_realloc(reachability->containers,
                               capacity * sizeof(*resized));
        if (resized == NULL) {
            reachability->failed = 1;
            return;
        }
        reachability->containers = resized;
        reachability->container_capacity = capacity;
    }
    reachability->containers[reachability->container_count++] = value.as.gc;
}

static void reachability_mark_table(class_reachability *reachability,
                                    const parray *table) {
    size_t i;
    if (table == NULL) return;
    for (i = 0U; i < table->used; i++) {
        if (table->entries[i].key.type == PT_NULL) continue;
        reachability_mark_value(reachability, table->entries[i].key);
        reachability_mark_value(reachability, table->entries[i].value);
    }
}

static void reachability_expand_class(class_reachability *reachability,
                                      pclass *class_entry) {
    size_t i;
    reachability_mark_class(reachability, class_entry->parent);
    for (i = 0U; i < class_entry->interface_count; i++) {
        reachability_mark_class(reachability, class_entry->interfaces[i]);
    }
    for (i = 0U; i < class_entry->property_count; i++) {
        reachability_mark_value(reachability,
                                class_entry->properties[i].default_value);
    }
    reachability_mark_table(reachability, class_entry->static_properties);
    reachability_mark_table(reachability, class_entry->constants);
}

static void reachability_expand_container(class_reachability *reachability,
                                          pheader *header) {
    size_t i;
    if (header->type == PT_ARRAY) {
        reachability_mark_table(reachability, (const parray *)header);
        return;
    }
    if (header->type == PT_OBJECT) {
        pobject *object = (pobject *)header;
        reachability_mark_class(reachability, object->class_entry);
        for (i = 0U; i < object->class_entry->property_count; i++) {
            reachability_mark_value(reachability, object->slots[i]);
        }
        return;
    }
    if (header->type == PT_CLOSURE) {
        pclosure *closure = (pclosure *)header;
        reachability_mark_class(reachability, closure->called_scope);
        reachability_mark_class(reachability, closure->called_class);
        for (i = 0U; i < closure->capture_count; i++) {
            reachability_mark_value(reachability, closure->captures[i]);
        }
    }
}

static int reachability_build(pphp_state *state,
                              class_reachability *reachability) {
    size_t i;
    memset(reachability, 0, sizeof(*reachability));
    reachability->state = state;
    if (state->class_count != 0U) {
        reachability->reachable_classes = pphp_alloc(state->class_count);
        reachability->class_queue = pphp_alloc(
            state->class_count * sizeof(*reachability->class_queue));
        if (reachability->reachable_classes == NULL ||
            reachability->class_queue == NULL) {
            reachability_destroy(reachability);
            return 0;
        }
        memset(reachability->reachable_classes, 0, state->class_count);
    }
    for (i = 0U; i < state->class_count; i++) {
        if (state->classes[i]->persistent) {
            reachability_mark_class(reachability, state->classes[i]);
        }
    }
    reachability_mark_table(reachability, state->globals);
    reachability_mark_table(reachability, state->statics);
    reachability_mark_table(reachability, state->constants);
    reachability_mark_table(reachability, state->included_files);
    for (i = 0U; i < state->stack_count; i++) {
        reachability_mark_value(reachability, state->stack[i]);
    }
    for (i = 0U; i < state->frame_count; i++) {
        pframe *frame = &state->frames[i];
        reachability_mark_class(reachability, frame->called_scope);
        reachability_mark_class(reachability, frame->called_class);
        if (frame->has_return_override) {
            reachability_mark_value(reachability, frame->return_override);
        }
    }
    if (state->oom_exception != NULL) {
        reachability_mark_value(
            reachability,
            pv_heap(PT_OBJECT, &state->oom_exception->header));
    }
    while (!reachability->failed &&
           (reachability->class_queue_cursor <
                reachability->class_queue_count ||
            reachability->container_cursor < reachability->container_count)) {
        while (reachability->class_queue_cursor <
               reachability->class_queue_count) {
            reachability_expand_class(
                reachability,
                reachability->class_queue[
                    reachability->class_queue_cursor++]);
        }
        while (reachability->container_cursor <
               reachability->container_count) {
            reachability_expand_container(
                reachability,
                reachability->containers[
                    reachability->container_cursor++]);
        }
    }
    if (reachability->failed) {
        reachability_destroy(reachability);
        return 0;
    }
    return 1;
}

static int run_unreachable_object_destructors(
    pphp_state *state, const class_reachability *reachability) {
    pobject *object = state->gc_objects;
    int invoked = 0;
    if (object != NULL) pv_retain(pv_heap(PT_OBJECT, &object->header));
    while (object != NULL) {
        pobject *next = object->gc_next;
        if (next != NULL) pv_retain(pv_heap(PT_OBJECT, &next->header));
        if (!reachability_has_container(reachability, &object->header)) {
            invoked |= pobject_run_destructor(object);
        }
        pv_release(pv_heap(PT_OBJECT, &object->header));
        object = next;
    }
    return invoked;
}

void pphp_clear_user_classes(pphp_state *state) {
    class_reachability reachability;
    size_t read_index;
    size_t write_index;
    int invoked;
    if (state == NULL) return;
    do {
        if (!reachability_build(state, &reachability)) return;
        invoked = run_unreachable_object_destructors(state, &reachability);
        if (invoked) reachability_destroy(&reachability);
    } while (invoked);
    if (state->building_class != NULL &&
        !state->building_class->persistent) {
        pclass_release(state->building_class);
        state->building_class = NULL;
    }
    for (read_index = 0U; read_index < state->class_count; read_index++) {
        if (!reachability.reachable_classes[read_index] &&
            !state->classes[read_index]->persistent) {
            pclass_release_values(state->classes[read_index]);
        }
    }
    (void)pphp_gc_collect(state);
    write_index = 0U;
    for (read_index = 0U; read_index < state->class_count; read_index++) {
        pclass *class_entry = state->classes[read_index];
        if (!reachability.reachable_classes[read_index] &&
            !class_entry->persistent) {
            pclass_release(class_entry);
        } else {
            state->classes[write_index++] = class_entry;
        }
    }
    state->class_count = write_index;
    reachability_destroy(&reachability);
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

#if PPHP_WARNINGS
void pphp_warning(pphp_state *state, uint32_t line, const char *format, ...) {
    static const char prefix[] = "Warning: ";
    char output[320];
    const size_t suffix_reserve = 32U;
    size_t used = sizeof(prefix) - 1U;
    size_t message_capacity = sizeof(output) - used - suffix_reserve;
    va_list arguments;
    int length;
    if (state == NULL || format == NULL) return;
    if (line == 0U && state->frame_count != 0U) {
        line = state->frames[state->frame_count - 1U].line;
    }
    memcpy(output, prefix, used);
    va_start(arguments, format);
    length = vsnprintf(output + used, message_capacity, format, arguments);
    va_end(arguments);
    if (length > 0) {
        size_t appended = (size_t)length;
        if (appended >= message_capacity) appended = message_capacity - 1U;
        used += appended;
    }
    length = snprintf(output + used, sizeof(output) - used,
                      " on line %lu\n", (unsigned long)line);
    if (length <= 0) return;
    if ((size_t)length >= sizeof(output) - used) {
        used = sizeof(output) - 1U;
    } else {
        used += (size_t)length;
    }
    pphp_output(state, output, used);
}
#endif

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
#if PPHP_ENABLE_COMPILER
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pmodule module;
    pc_codegen_error codegen_error;
    int result;
    int retained;
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
    retained = repl || module_has_persistent_functions(&module);
    if (retained && !validate_repl_functions(state, &module)) {
        pmodule_destroy(&module);
        return PPHP_E_RUNTIME;
    }
    if (retained && !retain_module(state, &module, &execution_module)) {
        pmodule_destroy(&module);
        pphp_runtime_error(state, 0U,
                           "out of memory retaining function definitions");
        return PPHP_E_RUNTIME;
    }
    if (!retained && !move_module(&module, &execution_module)) {
        pmodule_destroy(&module);
        pphp_runtime_error(state, 0U,
                           "out of memory preparing source module");
        return PPHP_E_RUNTIME;
    }
    result = pphp_vm_execute(state, execution_module);
    if (!repl) {
        state->module = NULL;
        if (result != PPHP_OK) {
            pphp_remove_module_functions(state, execution_module);
            if (retained) release_retained_module(state, execution_module);
        } else if (!retained) {
            pmodule_release(execution_module);
        }
        pphp_clear_user_classes(state);
        if (result != PPHP_OK && !retained) pmodule_release(execution_module);
    }
    return result;
#else
    (void)length;
    (void)repl;
    if (state == NULL || source == NULL) {
        return PPHP_E_RUNTIME;
    }
    state->error[0] = '\0';
    state->raised_class[0] = '\0';
    state->error_line = 0U;
    state->exit_requested = 0;
    state->exit_status = 0;
    state->chunk_name = chunk_name == NULL ? "<source>" : chunk_name;
    pphp_runtime_error(state, 0U,
                       "source compiler is disabled; execute PBC instead");
    return PPHP_E_PARSE;
#endif
}

int pphp_exec_source(pphp_state *state, const char *source, size_t length,
                     const char *chunk_name) {
    return pphp_exec_source_mode(state, source, length, chunk_name, 0);
}

static int exec_pbc(pphp_state *state, const void *pbc, size_t length,
                    void *owned_backing) {
    pmodule module;
    pmodule *execution_module = &module;
    int result;
    int retained;
    if (state == NULL) {
        pphp_free(owned_backing);
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
        pphp_free(owned_backing);
        pphp_runtime_error(state, 0U,
                           result == PPHP_E_UNSUPPORTED
                               ? "PBC image requires float support"
                               : "invalid or incompatible PBC image");
        return result == PPHP_E_NOMEM ? PPHP_E_RUNTIME : PPHP_E_PARSE;
    }
    module.backing = owned_backing;
    module.owns_backing = owned_backing != NULL;
    retained = module_has_persistent_functions(&module);
    if (retained && !validate_repl_functions(state, &module)) {
        pmodule_destroy(&module);
        return PPHP_E_RUNTIME;
    }
    if (retained && !retain_module(state, &module, &execution_module)) {
        pmodule_destroy(&module);
        pphp_runtime_error(state, 0U,
                           "out of memory retaining function definitions");
        return PPHP_E_RUNTIME;
    }
    if (!retained && !move_module(&module, &execution_module)) {
        pmodule_destroy(&module);
        pphp_runtime_error(state, 0U, "out of memory preparing PBC module");
        return PPHP_E_RUNTIME;
    }
    result = pphp_vm_execute(state, execution_module);
    state->module = NULL;
    if (result != PPHP_OK) {
        pphp_remove_module_functions(state, execution_module);
        if (retained) release_retained_module(state, execution_module);
    } else if (!retained) {
        pmodule_release(execution_module);
    }
    pphp_clear_user_classes(state);
    if (result != PPHP_OK && !retained) pmodule_release(execution_module);
    return result;
}

int pphp_exec_pbc(pphp_state *state, const void *pbc, size_t length) {
    return exec_pbc(state, pbc, length, NULL);
}

int pphp_exec_pbc_owned(pphp_state *state, void *pbc, size_t length) {
    return exec_pbc(state, pbc, length, pbc);
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
