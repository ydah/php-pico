#include "pbc.h"

#include "pphp/pphp.h"

#include <string.h>

#if PPHP_TYPECHECK
int ptype_spec_add(ptype_spec *spec, uint8_t kind,
                   const char *name, size_t length) {
    pstring *type_name = NULL;
    if (kind == PTYPE_NAMED) {
        type_name = ps_new(name, length);
        if (type_name == NULL) return 0;
    }
    if (!ptype_spec_add_string(spec, kind, type_name)) {
        ps_destroy(type_name);
        return 0;
    }
    return 1;
}

int ptype_spec_add_string(ptype_spec *spec, uint8_t kind, pstring *name) {
    ptype_member *resized;
    if (spec == NULL || spec->count == UINT8_MAX ||
        kind < PTYPE_INT || kind > PTYPE_NAMED ||
        (kind == PTYPE_NAMED && name == NULL) ||
        (kind != PTYPE_NAMED && name != NULL)) return 0;
    resized = pphp_realloc(spec->members,
                           ((size_t)spec->count + 1U) * sizeof(*spec->members));
    if (resized == NULL) return 0;
    spec->members = resized;
    spec->members[spec->count].kind = kind;
    spec->members[spec->count].name = name;
    spec->count++;
    return 1;
}

void ptype_spec_destroy(ptype_spec *spec) {
    size_t i;
    if (spec == NULL) return;
    for (i = 0U; i < spec->count; i++) ps_destroy(spec->members[i].name);
    pphp_free(spec->members);
    spec->members = NULL;
    spec->count = 0U;
}
#endif

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
#if PPHP_TYPECHECK
    if (proto->parameter_types != NULL) {
        for (i = 0U; i < proto->n_params; i++) {
            ptype_spec_destroy(&proto->parameter_types[i]);
        }
    }
    pphp_free(proto->parameter_types);
    ptype_spec_destroy(&proto->return_type);
#endif
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
