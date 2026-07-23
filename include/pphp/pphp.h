#ifndef PPHP_H
#define PPHP_H

#include <stddef.h>
#include <stdint.h>

#include "pphp_config.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PPHP_VERSION "1.0.0"
#define PPHP_PBC_FORMAT_VERSION 2U

enum {
    PPHP_OK = 0,
    PPHP_E_PARSE = 1,
    PPHP_E_RUNTIME = 2,
    PPHP_E_NOMEM = 3,
    PPHP_E_IO = 4,
    PPHP_E_UNSUPPORTED = 5
};

typedef struct pphp_state pphp_state;
typedef struct pphp_ctx pphp_ctx;
typedef struct pclass pclass;
typedef struct pobject pobject;
typedef void (*pphp_output_fn)(void *context, const char *bytes, size_t length);
typedef int (*pphp_cfunc)(pphp_ctx *context);

enum {
    PPHP_PUBLIC = 1U << 0,
    PPHP_PROTECTED = 1U << 1,
    PPHP_PRIVATE = 1U << 2,
    PPHP_STATIC = 1U << 3,
    PPHP_ABSTRACT = 1U << 4,
    PPHP_FINAL = 1U << 5,
    PPHP_READONLY = 1U << 6
};

typedef struct pphp_pool_stats {
    size_t total;
    size_t used;
    size_t free;
    size_t largest_free;
    size_t fragments;
} pphp_pool_stats;

#if PPHP_RC_DEBUG
enum {
    PPHP_RC_CHECK_OK = 0,
    PPHP_RC_CHECK_MISMATCH = 1,
    PPHP_RC_CHECK_NOMEM = 2,
    PPHP_RC_CHECK_INVALID = 3
};

typedef struct pphp_rc_check_result {
    int status;
    size_t checked;
    const pheader *target;
    uint16_t actual;
    size_t expected;
} pphp_rc_check_result;

typedef void (*pphp_rc_observe_fn)(void *context, pvalue value);
typedef void (*pphp_native_rc_visit_fn)(const pobject *object,
                                        pphp_rc_observe_fn observe,
                                        void *context);
#endif

void pphp_pool_init(void *buffer, size_t size);
void *pphp_alloc(size_t size);
void *pphp_realloc(void *ptr, size_t size);
void pphp_free(void *ptr);
pphp_pool_stats pphp_pool_get_stats(void);

pphp_state *pphp_open(void *pool, size_t pool_size);
void pphp_close(pphp_state *state);
int pphp_exec_source(pphp_state *state, const char *source, size_t length,
                     const char *chunk_name);
/*
 * The PBC image is executed in place. The caller must keep it readable and
 * unchanged until the state is closed; php-pico never writes to the image.
 * File-backed frontends transfer ownership of their buffers to the state.
 */
int pphp_exec_pbc(pphp_state *state, const void *pbc, size_t length);
void pphp_tick(pphp_state *state);
void pphp_set_output(pphp_state *state, pphp_output_fn output, void *context);
const char *pphp_last_error(const pphp_state *state);
uint32_t pphp_last_error_line(const pphp_state *state);
int pphp_exit_requested(const pphp_state *state);
int pphp_exit_status(const pphp_state *state);
#if PPHP_RC_DEBUG
int pphp_rc_check(pphp_state *state, pphp_rc_check_result *result);
#endif

void pphp_def_func(pphp_state *state, const char *name, pphp_cfunc function,
                   int minimum_arguments, int maximum_arguments);
pclass *pphp_def_class(pphp_state *state, const char *name,
                       const char *parent);
void pphp_def_method(pclass *class_entry, const char *name,
                     pphp_cfunc function, uint8_t flags);
void pphp_def_cconst_int(pclass *class_entry, const char *name, pphp_int value);

int pphp_argc(pphp_ctx *context);
pvalue pphp_arg(pphp_ctx *context, int index);
pphp_int pphp_arg_int(pphp_ctx *context, int index);
const char *pphp_arg_str(pphp_ctx *context, int index, size_t *length);
pobject *pphp_this(pphp_ctx *context);
void pphp_ret_null(pphp_ctx *context);
void pphp_ret_int(pphp_ctx *context, pphp_int value);
#if PPHP_ENABLE_FLOAT
void pphp_ret_float(pphp_ctx *context, pphp_float value);
#endif
void pphp_ret_bool(pphp_ctx *context, int value);
void pphp_ret_strn(pphp_ctx *context, const char *bytes, size_t length);
void pphp_ret_value(pphp_ctx *context, pvalue value);
void pphp_ret_object(pphp_ctx *context, pobject *object);
int pphp_raise(pphp_ctx *context, const char *class_name,
               const char *format, ...);
pobject *pphp_obj_new_with(pphp_ctx *context, pclass *class_entry,
                           size_t extra_bytes, void (*finalizer)(void *));
void *pphp_obj_data(pobject *object);
const void *pphp_obj_const_data(const pobject *object);
#if PPHP_RC_DEBUG
/* Native data that owns pvalues must enumerate each owning edge exactly once.
 * The visitor is called only by pphp_rc_check; it must not retain, release, or
 * mutate the values it passes to observe. */
void pphp_obj_set_rc_visitor(pobject *object,
                             pphp_native_rc_visit_fn visitor);
#endif

#ifdef __cplusplus
}
#endif

#endif
