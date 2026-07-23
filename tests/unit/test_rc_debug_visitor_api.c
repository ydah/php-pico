#include "pphp/pphp.h"

#if !PPHP_RC_DEBUG
#error "visitor API compile test requires PPHP_RC_DEBUG=1"
#endif

typedef struct external_native_data {
    pvalue owned;
} external_native_data;

void external_native_rc_visit(const pobject *object,
                              pphp_rc_observe_fn observe,
                              void *context) {
    const external_native_data *data = pphp_obj_const_data(object);
    if (data != NULL) observe(context, data->owned);
}

pphp_native_rc_visit_fn external_native_rc_visitor =
    external_native_rc_visit;
