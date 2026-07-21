#ifndef PPHP_H
#define PPHP_H

#include <stddef.h>
#include <stdint.h>

#include "pphp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PPHP_VERSION "0.1.0-dev"

enum {
    PPHP_OK = 0,
    PPHP_E_PARSE = 1,
    PPHP_E_RUNTIME = 2,
    PPHP_E_NOMEM = 3,
    PPHP_E_IO = 4
};

typedef struct pphp_state pphp_state;
typedef struct pphp_ctx pphp_ctx;
typedef struct pclass pclass;
typedef struct pobject pobject;
typedef void (*pphp_output_fn)(void *context, const char *bytes, size_t length);

typedef struct pphp_pool_stats {
    size_t total;
    size_t used;
    size_t free;
    size_t largest_free;
    size_t fragments;
} pphp_pool_stats;

void pphp_pool_init(void *buffer, size_t size);
void *pphp_alloc(size_t size);
void *pphp_realloc(void *ptr, size_t size);
void pphp_free(void *ptr);
pphp_pool_stats pphp_pool_get_stats(void);

pphp_state *pphp_open(void *pool, size_t pool_size);
void pphp_close(pphp_state *state);
int pphp_exec_source(pphp_state *state, const char *source, size_t length,
                     const char *chunk_name);
int pphp_exec_pbc(pphp_state *state, const void *pbc, size_t length);
void pphp_tick(pphp_state *state);
void pphp_set_output(pphp_state *state, pphp_output_fn output, void *context);
const char *pphp_last_error(const pphp_state *state);
uint32_t pphp_last_error_line(const pphp_state *state);
int pphp_exit_requested(const pphp_state *state);
int pphp_exit_status(const pphp_state *state);

#ifdef __cplusplus
}
#endif

#endif
