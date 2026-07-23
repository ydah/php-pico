#include "closure.h"

#include "alloc.h"
#include "pphp/pphp.h"
#include "gc.h"
#include "pclass.h"

pclosure *pclosure_new(const pproto *proto, const pmodule *module,
                       struct pclass *called_scope,
                       struct pclass *called_class,
                       const pvalue *captures, size_t capture_count) {
    pclosure *closure;
    size_t i;
    if (proto == NULL || capture_count > UINT8_MAX ||
        (capture_count != 0U && captures == NULL)) return NULL;
    closure = pphp_alloc(sizeof(*closure) + capture_count * sizeof(*captures));
    if (closure == NULL) return NULL;
    closure->header.refcnt = 1U;
    closure->header.type = PT_CLOSURE;
    closure->header.flags = 0U;
#if PPHP_RC_DEBUG
    pphp_alloc_track(closure);
#endif
    closure->proto = proto;
    closure->module = module;
    closure->called_scope = called_scope;
    closure->called_class = called_class;
    closure->capture_count = (uint8_t)capture_count;
    pmodule_retain((pmodule *)module);
    pclass_retain_runtime(called_scope);
    pclass_retain_runtime(called_class);
    for (i = 0U; i < capture_count; i++) {
        closure->captures[i] = captures[i];
        pv_retain(captures[i]);
    }
    return closure;
}

void pclosure_destroy(pclosure *closure) {
    size_t i;
    if (closure == NULL) return;
    pphp_gc_unbuffer(&closure->header);
    for (i = 0U; i < closure->capture_count; i++) {
        pv_release(closure->captures[i]);
    }
    pmodule_release((pmodule *)closure->module);
    pclass_release_runtime(closure->called_scope);
    pclass_release_runtime(closure->called_class);
    pphp_free(closure);
}
