#include "value.h"

#include "pstring.h"
#include "parray.h"
#include "resource.h"
#include "pclass.h"

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

pvalue pv_float(pphp_float number) {
    pvalue value = make_value(PT_FLOAT);
    value.as.f = number;
    return value;
}

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
        case PT_FLOAT:
            return value.as.f != (pphp_float)0;
        case PT_STRING: {
            const pstring *string = (const pstring *)value.as.gc;
            return string != NULL && string->length != 0U &&
                   !(string->length == 1U && string->data[0] == '0');
        }
        case PT_ARRAY:
            return value.as.gc != NULL && ((const parray *)value.as.gc)->size != 0U;
        case PT_ROSTRING: {
            const pro_string *string = (const pro_string *)value.as.ptr;
            return string != NULL && string->length != 0U &&
                   !(string->length == 1U && string->data[0] == '0');
        }
        default:
            return value.as.gc != NULL;
    }
}

void pv_retain(pvalue value) {
    pheader *header;
    if (value.type < PT_STRING || value.type == PT_ROSTRING || value.as.gc == NULL) {
        return;
    }
    header = value.as.gc;
    if (header->refcnt != UINT16_MAX) {
        header->refcnt++;
    }
}

void pv_release(pvalue value) {
    pheader *header;
    if (value.type < PT_STRING || value.type == PT_ROSTRING || value.as.gc == NULL) {
        return;
    }
    header = value.as.gc;
    if (header->refcnt == UINT16_MAX || header->refcnt == 0U) {
        return;
    }
    header->refcnt--;
    if (header->refcnt != 0U) {
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
