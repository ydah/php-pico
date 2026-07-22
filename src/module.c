#include "pbc.h"

#include "pphp/pphp.h"

#include <string.h>

void pproto_destroy(pproto *proto) {
    size_t i;
    if (proto == NULL) return;
    for (i = 0U; i < proto->constant_count; i++) {
        pv_release(proto->constants[i]);
    }
    if (proto->locals != NULL) {
        for (i = 0U; i < proto->n_locals; i++) {
            ps_destroy(proto->locals[i]);
        }
    }
    ps_destroy(proto->name);
    pphp_free(proto->locals);
    pphp_free(proto->constants);
    if (proto->owns_code) pphp_free(proto->code);
    pphp_free(proto->catches);
    pphp_free(proto);
}

int pmodule_init(pmodule *module) {
    if (module == NULL) return 0;
    memset(module, 0, sizeof(*module));
    module->refcnt = 1U;
    return 1;
}

void pmodule_destroy(pmodule *module) {
    size_t i;
    if (module == NULL) return;
    for (i = 0U; i < module->ro_string_count; i++) {
        module->ro_strings[i].owner = NULL;
    }
    for (i = 0U; i < module->count; i++) {
        pproto_destroy(module->protos[i]);
    }
    pphp_free(module->protos);
    pphp_free(module->ro_strings);
    if (module->owns_backing) pphp_free(module->backing);
    memset(module, 0, sizeof(*module));
}

void pmodule_retain(pmodule *module) {
    if (module != NULL && module->refcnt != UINT16_MAX) module->refcnt++;
}

void pmodule_release(pmodule *module) {
    uint8_t heap_allocated;
    if (module == NULL || module->refcnt == 0U ||
        module->refcnt == UINT16_MAX) return;
    module->refcnt--;
    if (module->refcnt != 0U) return;
    heap_allocated = module->heap_allocated;
    pmodule_destroy(module);
    if (heap_allocated) pphp_free(module);
}
