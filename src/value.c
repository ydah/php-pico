#include "value.h"

#include "pstring.h"
#include "parray.h"
#include "resource.h"
#include "pclass.h"
#include "closure.h"
#include "gc.h"
#include "pbc.h"

#include <string.h>

static pvalue make_value(pvalue_type type) {
    pvalue value;
    memset(&value, 0, sizeof(value));
    value.type = (uint8_t)type;
    return value;
}

pvalue pv_null(void) {
    return make_value(PT_NULL);
}

pvalue pv_bool(int value) {
    return make_value(value ? PT_TRUE : PT_FALSE);
}

pvalue pv_int(pphp_int number) {
    pvalue value = make_value(PT_INT);
    value.as.i = number;
    return value;
}

#if PPHP_ENABLE_FLOAT
pvalue pv_float(pphp_float number) {
    pvalue value = make_value(PT_FLOAT);
    value.as.f = number;
    return value;
}
#endif

pvalue pv_heap(pvalue_type type, pheader *header) {
    pvalue value = make_value(type);
    value.as.gc = header;
    return value;
}

int pv_is_truthy(pvalue value) {
    switch ((pvalue_type)value.type) {
        case PT_NULL:
        case PT_FALSE:
            return 0;
        case PT_TRUE:
            return 1;
        case PT_INT:
            return value.as.i != 0;
#if PPHP_ENABLE_FLOAT
        case PT_FLOAT:
            return value.as.f != (pphp_float)0;
#endif
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return string != NULL && string->length != 0U &&
                   !(string->length == 1U && ps_data(string)[0] == '0');
        }
        case PT_ARRAY:
            return value.as.gc != NULL && ((const parray *)value.as.gc)->size != 0U;
        case PT_ROSTRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return string != NULL && string->length != 0U &&
                   !(string->length == 1U && ps_data(string)[0] == '0');
        }
        default:
            return value.as.gc != NULL;
    }
}

void pv_retain(pvalue value) {
    pheader *header;
    if (value.type < PT_STRING || value.as.gc == NULL) {
        return;
    }
    header = value.as.gc;
    if (value.type == PT_ROSTRING || header->type == PT_ROSTRING) {
        pmodule_retain(ps_owner((pstring *)header));
        return;
    }
    if (header->refcnt != UINT16_MAX) {
        header->refcnt++;
    }
}

void pv_release(pvalue value) {
    pheader *header;
    if (value.type < PT_STRING || value.as.gc == NULL) {
        return;
    }
    header = value.as.gc;
    if (value.type == PT_ROSTRING || header->type == PT_ROSTRING) {
        pmodule_release(ps_owner((pstring *)header));
        return;
    }
    if (header->refcnt == UINT16_MAX || header->refcnt == 0U) {
        return;
    }
    header->refcnt--;
    if (header->refcnt != 0U) {
        pphp_gc_buffer(header);
        return;
    }
    switch ((pvalue_type)value.type) {
        case PT_STRING:
            ps_destroy((pstring *)header);
            break;
        case PT_ARRAY:
            pa_destroy((parray *)header);
            break;
        case PT_OBJECT:
            pobject_destroy((pobject *)header);
            break;
        case PT_CLOSURE:
            pclosure_destroy((pclosure *)header);
            break;
        case PT_RESOURCE:
            presource_destroy((presource *)header);
            break;
        default:
            break;
    }
}

const char *pv_type_name(pvalue_type type) {
    static const char *const names[] = {
        "NULL", "boolean", "boolean", "integer", "double", "string",
        "array", "object", "Closure", "cfunc", "resource", "string"
    };
    if ((size_t)type >= sizeof(names) / sizeof(names[0])) {
        return "unknown";
    }
    return names[type];
}
