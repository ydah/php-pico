#include "pphp/pphp.h"

#if PPHP_RC_DEBUG

#include "alloc.h"
#include "closure.h"
#include "parray.h"
#include "pclass.h"
#include "pbc.h"
#include "pstring.h"
#include "resource.h"
#include "state.h"

#include <string.h>

typedef struct rc_scan {
    pphp_state *state;
    pphp_state *root;
    pheader **headers;
    size_t header_count;
    pclass **classes;
    size_t class_count;
    size_t class_capacity;
    const pmodule **modules;
    size_t module_count;
    size_t module_capacity;
    int oom;
    int invalid;
} rc_scan;

/* The checker distinguishes pvalue owners (which retain/release) from raw
 * owners (which construct/destroy a heap value directly). GC candidate lists,
 * object-list links, property owners, and class/module/proto links are borrowed
 * or have independent refcounts, so they intentionally contribute no edge. */

static int count_header(pheader *header, void *context) {
    size_t *count = context;
    (void)header;
    (*count)++;
    return 1;
}

static int save_header(pheader *header, void *context) {
    rc_scan *scan = context;
    scan->headers[scan->header_count++] = header;
    return 1;
}

static int grow_pointer_array(void ***items, size_t *capacity) {
    size_t next = *capacity == 0U ? 16U : *capacity * 2U;
    void **resized;
    if (next < *capacity || next > SIZE_MAX / sizeof(*resized)) return 0;
    resized = pphp_realloc(*items, next * sizeof(*resized));
    if (resized == NULL) return 0;
    *items = resized;
    *capacity = next;
    return 1;
}

static void add_class(rc_scan *scan, pclass *class_entry) {
    size_t i;
    if (class_entry == NULL || scan->oom) return;
    for (i = 0U; i < scan->class_count; i++) {
        if (scan->classes[i] == class_entry) return;
    }
    if (scan->class_count == scan->class_capacity &&
        !grow_pointer_array((void ***)&scan->classes,
                            &scan->class_capacity)) {
        scan->oom = 1;
        return;
    }
    scan->classes[scan->class_count++] = class_entry;
}

static void add_module(rc_scan *scan, const pmodule *module) {
    size_t i;
    if (module == NULL || scan->oom) return;
    for (i = 0U; i < scan->module_count; i++) {
        if (scan->modules[i] == module) return;
    }
    if (scan->module_count == scan->module_capacity &&
        !grow_pointer_array((void ***)&scan->modules,
                            &scan->module_capacity)) {
        scan->oom = 1;
        return;
    }
    scan->modules[scan->module_count++] = module;
}

static void observe_value(rc_scan *scan, pvalue value) {
    if ((value.type == PT_STRING || value.type == PT_ROSTRING) &&
        value.as.gc != NULL && value.as.gc->type == PT_ROSTRING) {
        add_module(scan, ps_owner((const pstring *)value.as.gc));
    }
}

static void gather_native_value(void *context, pvalue value) {
    observe_value((rc_scan *)context, value);
}

static void gather_header_owners(rc_scan *scan, pheader *header) {
    size_t i;
    switch ((pvalue_type)header->type) {
        case PT_STRING:
            break;
        case PT_ARRAY: {
            const parray *array = (const parray *)header;
            for (i = 0U; i < array->used; i++) {
                if (array->entries[i].key.type == PT_NULL) continue;
                observe_value(scan, array->entries[i].key);
                observe_value(scan, array->entries[i].value);
            }
            break;
        }
        case PT_OBJECT: {
            const pobject *object = (const pobject *)header;
            add_class(scan, object->class_entry);
            for (i = 0U; i < object->class_entry->property_count; i++) {
                observe_value(scan, object->slots[i]);
            }
            if (object->native_rc_visitor != NULL) {
                object->native_rc_visitor(object, gather_native_value, scan);
            }
            break;
        }
        case PT_CLOSURE: {
            const pclosure *closure = (const pclosure *)header;
            add_module(scan, closure->module);
            add_class(scan, closure->called_scope);
            add_class(scan, closure->called_class);
            for (i = 0U; i < closure->capture_count; i++) {
                observe_value(scan, closure->captures[i]);
            }
            break;
        }
        case PT_RESOURCE: {
            const presource *resource = (const presource *)header;
            if (resource->kind == PRESOURCE_ITERATOR) {
                const parray_iterator *iterator =
                    (const parray_iterator *)resource;
                observe_value(scan,
                              pv_heap(PT_ARRAY, &iterator->array->header));
            }
            break;
        }
        default:
            scan->invalid = 1;
            break;
    }
}

static void gather_execution_state(rc_scan *scan, const pphp_state *state) {
    size_t i;
    if (state == NULL) return;
    add_module(scan, state->module);
    for (i = 0U; i < state->stack_count; i++) {
        observe_value(scan, state->stack[i]);
    }
    for (i = 0U; i < state->frame_count; i++) {
        const pframe *frame = &state->frames[i];
        add_module(scan, frame->module);
        add_class(scan, frame->called_scope);
        add_class(scan, frame->called_class);
        if (frame->has_return_override) {
            observe_value(scan, frame->return_override);
        }
    }
}

static void gather_persistent_state(rc_scan *scan) {
    pphp_state *root = scan->root;
    size_t i;
    for (i = 0U; i < root->class_count; i++) add_class(scan, root->classes[i]);
    add_class(scan, root->building_class);
    add_module(scan, root->module);
    for (i = 0U; i < root->repl_module_count; i++) {
        add_module(scan, root->repl_modules[i]);
    }
    for (i = 0U; i < root->runtime_function_count; i++) {
        add_module(scan, root->runtime_functions[i].module);
    }
    if (root->oom_exception != NULL) {
        add_class(scan, root->oom_exception->class_entry);
    }
}

static void gather_class(rc_scan *scan, const pclass *class_entry) {
    size_t i;
    add_class(scan, class_entry->parent);
    for (i = 0U; i < class_entry->interface_count; i++) {
        add_class(scan, class_entry->interfaces[i]);
    }
    for (i = 0U; i < class_entry->property_count; i++) {
        observe_value(scan, class_entry->properties[i].default_value);
    }
    for (i = 0U; i < class_entry->method_count; i++) {
        add_module(scan, class_entry->methods[i].module);
    }
}

static void gather_module(rc_scan *scan, const pmodule *module) {
    size_t i;
    size_t j;
    for (i = 0U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        for (j = 0U; j < proto->constant_count; j++) {
            observe_value(scan, proto->constants[j]);
        }
    }
}

static int gather_reachable_metadata(rc_scan *scan) {
    size_t class_index = 0U;
    size_t module_index = 0U;
    size_t i;
    gather_persistent_state(scan);
    gather_execution_state(scan, scan->state);
    if (scan->root != scan->state) gather_execution_state(scan, scan->root);
    for (i = 0U; i < scan->header_count; i++) {
        gather_header_owners(scan, scan->headers[i]);
    }
    while (!scan->oom && !scan->invalid &&
           (class_index < scan->class_count ||
            module_index < scan->module_count)) {
        while (!scan->oom && !scan->invalid &&
               class_index < scan->class_count) {
            gather_class(scan, scan->classes[class_index++]);
        }
        while (!scan->oom && !scan->invalid &&
               module_index < scan->module_count) {
            gather_module(scan, scan->modules[module_index++]);
        }
    }
    return !scan->oom && !scan->invalid;
}

static size_t value_edge(const pheader *target, pvalue value) {
    if (value.type < PT_STRING || value.as.gc == NULL ||
        value.type == PT_ROSTRING || value.as.gc->type == PT_ROSTRING) {
        return 0U;
    }
    return value.as.gc == target ? 1U : 0U;
}

typedef struct native_edge_count {
    const pheader *target;
    size_t expected;
} native_edge_count;

static void count_native_value(void *context, pvalue value) {
    native_edge_count *count = context;
    count->expected += value_edge(count->target, value);
}

static size_t raw_edge(const pheader *target, const void *owner) {
    return owner == target ? 1U : 0U;
}

static size_t count_array_edges(const pheader *target, const parray *array) {
    size_t expected = 0U;
    size_t i;
    for (i = 0U; i < array->used; i++) {
        if (array->entries[i].key.type == PT_NULL) continue;
        expected += value_edge(target, array->entries[i].key);
        expected += value_edge(target, array->entries[i].value);
    }
    return expected;
}

#if PPHP_TYPECHECK
static size_t count_type_edges(const pheader *target,
                               const ptype_spec *spec) {
    size_t expected = 0U;
    size_t i;
    for (i = 0U; i < spec->count; i++) {
        expected += raw_edge(target, spec->members[i].name);
    }
    return expected;
}
#endif

static size_t count_tracked_container_edges(const rc_scan *scan,
                                            const pheader *target) {
    size_t expected = 0U;
    size_t i;
    size_t j;
    for (i = 0U; i < scan->header_count; i++) {
        const pheader *header = scan->headers[i];
        switch ((pvalue_type)header->type) {
            case PT_STRING:
                break;
            case PT_ARRAY:
                expected += count_array_edges(target,
                                              (const parray *)header);
                break;
            case PT_OBJECT: {
                const pobject *object = (const pobject *)header;
                native_edge_count native = {target, 0U};
                for (j = 0U; j < object->class_entry->property_count; j++) {
                    expected += value_edge(target, object->slots[j]);
                }
                if (object->native_rc_visitor != NULL) {
                    object->native_rc_visitor(object, count_native_value,
                                              &native);
                    expected += native.expected;
                }
                break;
            }
            case PT_CLOSURE: {
                const pclosure *closure = (const pclosure *)header;
                for (j = 0U; j < closure->capture_count; j++) {
                    expected += value_edge(target, closure->captures[j]);
                }
                break;
            }
            case PT_RESOURCE: {
                const presource *resource = (const presource *)header;
                if (resource->kind == PRESOURCE_ITERATOR) {
                    const parray_iterator *iterator =
                        (const parray_iterator *)resource;
                    expected += raw_edge(target, iterator->array);
                }
                break;
            }
            default:
                break;
        }
    }
    return expected;
}

static size_t count_state_edges(const rc_scan *scan,
                                const pheader *target) {
    const pphp_state *state = scan->state;
    const pphp_state *root = scan->root;
    size_t expected = 0U;
    size_t i;
    expected += raw_edge(target, root->globals);
    expected += raw_edge(target, root->statics);
    expected += raw_edge(target, root->constants);
    expected += raw_edge(target, root->included_files);
    expected += raw_edge(target, root->oom_exception);
    for (i = 0U; i < root->symbols.capacity; i++) {
        expected += raw_edge(target, root->symbols.entries[i]);
    }
    for (i = 0U; i < root->native_function_count; i++) {
        expected += raw_edge(target, root->native_functions[i].name);
    }
    for (i = 0U; i < state->stack_count; i++) {
        expected += value_edge(target, state->stack[i]);
    }
    for (i = 0U; i < state->frame_count; i++) {
        if (state->frames[i].has_return_override) {
            expected += value_edge(target,
                                   state->frames[i].return_override);
        }
    }
    if (root != state) {
        for (i = 0U; i < root->stack_count; i++) {
            expected += value_edge(target, root->stack[i]);
        }
        for (i = 0U; i < root->frame_count; i++) {
            if (root->frames[i].has_return_override) {
                expected += value_edge(target,
                                       root->frames[i].return_override);
            }
        }
    }
    return expected;
}

static size_t count_class_edges(const rc_scan *scan,
                                const pheader *target) {
    size_t expected = 0U;
    size_t i;
    size_t j;
    for (i = 0U; i < scan->class_count; i++) {
        const pclass *class_entry = scan->classes[i];
        expected += raw_edge(target, class_entry->name);
        expected += raw_edge(target, class_entry->static_properties);
        expected += raw_edge(target, class_entry->constants);
        for (j = 0U; j < class_entry->property_count; j++) {
            const pproperty *property = &class_entry->properties[j];
            expected += raw_edge(target, property->name);
            expected += value_edge(target, property->default_value);
#if PPHP_TYPECHECK
            expected += count_type_edges(target, &property->type);
#endif
        }
        for (j = 0U; j < class_entry->static_property_count; j++) {
            const pproperty *property =
                &class_entry->static_property_defs[j];
            expected += raw_edge(target, property->name);
#if PPHP_TYPECHECK
            expected += count_type_edges(target, &property->type);
#endif
        }
        for (j = 0U; j < class_entry->method_count; j++) {
            expected += raw_edge(target, class_entry->methods[j].name);
        }
    }
    return expected;
}

static size_t count_module_edges(const rc_scan *scan,
                                 const pheader *target) {
    size_t expected = 0U;
    size_t i;
    size_t j;
    size_t k;
    for (i = 0U; i < scan->module_count; i++) {
        const pmodule *module = scan->modules[i];
        for (j = 0U; j < module->count; j++) {
            const pproto *proto = module->protos[j];
            expected += raw_edge(target, proto->name);
            for (k = 0U; k < proto->constant_count; k++) {
                expected += value_edge(target, proto->constants[k]);
            }
            for (k = 0U; k < proto->n_locals; k++) {
                expected += raw_edge(target, proto->locals[k]);
            }
#if PPHP_TYPECHECK
            if (proto->parameter_types != NULL) {
                for (k = 0U; k < proto->n_params; k++) {
                    expected += count_type_edges(
                        target, &proto->parameter_types[k]);
                }
            }
            expected += count_type_edges(target, &proto->return_type);
#endif
        }
    }
    return expected;
}

static size_t expected_refcount(const rc_scan *scan,
                                const pheader *target) {
    return count_state_edges(scan, target) +
           count_tracked_container_edges(scan, target) +
           count_class_edges(scan, target) +
           count_module_edges(scan, target);
}

int pphp_rc_check(pphp_state *state, pphp_rc_check_result *result) {
    rc_scan scan;
    size_t tracked_count = 0U;
    size_t i;
    if (result == NULL) return 0;
    memset(result, 0, sizeof(*result));
    if (state == NULL) {
        result->status = PPHP_RC_CHECK_INVALID;
        return 0;
    }
    memset(&scan, 0, sizeof(scan));
    scan.state = state;
    scan.root = state->root_state == NULL ? state : state->root_state;
    if (!pphp_alloc_visit_tracked(count_header, &tracked_count)) {
        result->status = PPHP_RC_CHECK_INVALID;
        return 0;
    }
    if (tracked_count > SIZE_MAX / sizeof(*scan.headers)) {
        result->status = PPHP_RC_CHECK_NOMEM;
        return 0;
    }
    if (tracked_count != 0U) {
        scan.headers = pphp_alloc(tracked_count * sizeof(*scan.headers));
        if (scan.headers == NULL) {
            result->status = PPHP_RC_CHECK_NOMEM;
            return 0;
        }
    }
    if (!pphp_alloc_visit_tracked(save_header, &scan) ||
        scan.header_count != tracked_count ||
        !gather_reachable_metadata(&scan)) {
        result->status = scan.oom ? PPHP_RC_CHECK_NOMEM
                                  : PPHP_RC_CHECK_INVALID;
        pphp_free(scan.modules);
        pphp_free(scan.classes);
        pphp_free(scan.headers);
        return 0;
    }
    result->status = PPHP_RC_CHECK_OK;
    for (i = 0U; i < scan.header_count; i++) {
        pheader *target = scan.headers[i];
        size_t expected;
        if (target->refcnt == UINT16_MAX) continue;
        expected = expected_refcount(&scan, target);
        result->checked++;
        if ((size_t)target->refcnt != expected) {
            result->status = PPHP_RC_CHECK_MISMATCH;
            result->target = target;
            result->actual = target->refcnt;
            result->expected = expected;
            break;
        }
    }
    pphp_free(scan.modules);
    pphp_free(scan.classes);
    pphp_free(scan.headers);
    return result->status == PPHP_RC_CHECK_OK;
}

#endif
