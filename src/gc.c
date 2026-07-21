#include "gc.h"

#include "closure.h"
#include "parray.h"
#include "pclass.h"

#include <string.h>

enum {
    PPHP_GC_BUFFER_MAX = 256,
    PPHP_GC_BUFFERED = 1U << 5,
    PPHP_GC_COLLECTING = 1U << 6,
    PPHP_GC_DESTROYING = 1U << 7
};

typedef void (*child_visitor)(pvalue *child, void *context);

static pphp_state *active_state;
static pheader *candidates[PPHP_GC_BUFFER_MAX];
static size_t candidate_count;
static pheader *nodes[PPHP_GC_BUFFER_MAX];
static uint16_t external_refs[PPHP_GC_BUFFER_MAX];
static uint8_t reachable[PPHP_GC_BUFFER_MAX];
static size_t node_count;
static int collecting;
static int collection_requested;

static int is_container_type(uint8_t type) {
    return type == PT_ARRAY || type == PT_OBJECT || type == PT_CLOSURE;
}

static void visit_children(pheader *header, child_visitor visitor,
                           void *context) {
    size_t i;
    if (header == NULL || visitor == NULL) return;
    if (header->type == PT_ARRAY) {
        parray *array = (parray *)header;
        for (i = 0U; i < array->used; i++) {
            if (array->entries[i].key.type == PT_NULL) continue;
            visitor(&array->entries[i].key, context);
            visitor(&array->entries[i].value, context);
        }
    } else if (header->type == PT_OBJECT) {
        pobject *object = (pobject *)header;
        for (i = 0U; i < object->class_entry->property_count; i++) {
            visitor(&object->slots[i], context);
        }
    } else if (header->type == PT_CLOSURE) {
        pclosure *closure = (pclosure *)header;
        for (i = 0U; i < closure->capture_count; i++) {
            visitor(&closure->captures[i], context);
        }
    }
}

static size_t find_node(const pheader *header) {
    size_t i;
    for (i = 0U; i < node_count; i++) {
        if (nodes[i] == header) return i;
    }
    return node_count;
}

static void discover_child(pvalue *child, void *context) {
    pheader *header;
    (void)context;
    if (child == NULL || !is_container_type(child->type) ||
        child->as.gc == NULL) return;
    header = child->as.gc;
    if (find_node(header) == node_count && node_count < PPHP_GC_BUFFER_MAX) {
        nodes[node_count++] = header;
    }
}

static void subtract_internal_ref(pvalue *child, void *context) {
    size_t index;
    (void)context;
    if (child == NULL || !is_container_type(child->type) ||
        child->as.gc == NULL) return;
    index = find_node(child->as.gc);
    if (index < node_count && external_refs[index] != 0U) {
        external_refs[index]--;
    }
}

typedef struct reach_context {
    int changed;
} reach_context;

static void mark_reachable_child(pvalue *child, void *context) {
    reach_context *reach = context;
    size_t index;
    if (child == NULL || !is_container_type(child->type) ||
        child->as.gc == NULL) return;
    index = find_node(child->as.gc);
    if (index < node_count && !reachable[index]) {
        reachable[index] = 1U;
        reach->changed = 1;
    }
}

static void release_child(pvalue *child, void *context) {
    pvalue value;
    (void)context;
    if (child == NULL) return;
    value = *child;
    *child = pv_null();
    pv_release(value);
}

void pphp_gc_set_state(pphp_state *state) {
    size_t i;
    active_state = state;
    if (state == NULL) {
        for (i = 0U; i < candidate_count; i++) {
            if (candidates[i] != NULL) {
                candidates[i]->flags &= (uint8_t)~PPHP_GC_BUFFERED;
            }
        }
        candidate_count = 0U;
        collection_requested = 0;
    }
}

void pphp_gc_unbuffer(pheader *header) {
    size_t i;
    if (header == NULL || (header->flags & PPHP_GC_BUFFERED) == 0U) return;
    for (i = 0U; i < candidate_count; i++) {
        if (candidates[i] == header) {
            candidate_count--;
            candidates[i] = candidates[candidate_count];
            candidates[candidate_count] = NULL;
            break;
        }
    }
    header->flags &= (uint8_t)~PPHP_GC_BUFFERED;
}

void pphp_gc_buffer(pheader *header) {
#if PPHP_ENABLE_CYCLE_GC
    if (header == NULL || !is_container_type(header->type) ||
        header->refcnt == 0U || header->refcnt == UINT16_MAX ||
        (header->flags & (PPHP_GC_BUFFERED | PPHP_GC_COLLECTING |
                          PPHP_GC_DESTROYING)) != 0U) {
        return;
    }
    if (candidate_count < PPHP_GC_BUFFER_MAX) {
        header->flags |= PPHP_GC_BUFFERED;
        candidates[candidate_count++] = header;
        if (candidate_count == PPHP_GC_BUFFER_MAX) collection_requested = 1;
    } else {
        collection_requested = 1;
    }
#else
    (void)header;
#endif
}

void pphp_gc_maybe_collect(pphp_state *state) {
    if (collection_requested && !collecting) (void)pphp_gc_collect(state);
}

size_t pphp_gc_collect(pphp_state *state) {
#if PPHP_ENABLE_CYCLE_GC
    size_t initial_count;
    size_t collected = 0U;
    size_t i;
    int changed;
    if (state == NULL || collecting || candidate_count == 0U) return 0U;
    collecting = 1;
    collection_requested = 0;
    node_count = candidate_count;
    memcpy(nodes, candidates, node_count * sizeof(*nodes));
    initial_count = candidate_count;
    candidate_count = 0U;
    for (i = 0U; i < initial_count; i++) {
        candidates[i] = NULL;
        nodes[i]->flags &= (uint8_t)~PPHP_GC_BUFFERED;
    }
    for (i = 0U; i < node_count; i++) {
        visit_children(nodes[i], discover_child, NULL);
    }
    for (i = 0U; i < node_count; i++) {
        external_refs[i] = nodes[i]->refcnt;
        reachable[i] = 0U;
    }
    for (i = 0U; i < node_count; i++) {
        visit_children(nodes[i], subtract_internal_ref, NULL);
    }
    for (i = 0U; i < node_count; i++) {
        if (external_refs[i] != 0U) reachable[i] = 1U;
    }
    do {
        reach_context context = {0};
        for (i = 0U; i < node_count; i++) {
            if (reachable[i]) {
                visit_children(nodes[i], mark_reachable_child, &context);
            }
        }
        changed = context.changed;
    } while (changed);
    for (i = 0U; i < node_count; i++) {
        if (!reachable[i]) {
            nodes[i]->flags |= PPHP_GC_COLLECTING;
            pv_retain(pv_heap((pvalue_type)nodes[i]->type, nodes[i]));
            collected++;
        }
    }
    for (i = 0U; i < node_count; i++) {
        if (!reachable[i]) visit_children(nodes[i], release_child, NULL);
    }
    for (i = 0U; i < node_count; i++) {
        if (!reachable[i]) {
            pv_release(pv_heap((pvalue_type)nodes[i]->type, nodes[i]));
        }
    }
    node_count = 0U;
    collecting = 0;
    return collected;
#else
    (void)state;
    return 0U;
#endif
}
