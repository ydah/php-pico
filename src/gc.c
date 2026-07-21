#include "gc.h"

#include "pclass.h"
#include "pphp/pphp.h"

#include <string.h>

enum { POBJECT_GC_COLLECTING = 1U << 6 };

static size_t object_index(pobject *const *objects, size_t count,
                           const pobject *object) {
    size_t i;
    for (i = 0U; i < count; i++) {
        if (objects[i] == object) return i;
    }
    return count;
}

static void mark_reachable(pobject *const *objects, size_t count,
                           uint8_t *reachable, size_t index) {
    pobject *object;
    size_t i;
    if (index >= count || reachable[index]) return;
    reachable[index] = 1U;
    object = objects[index];
    for (i = 0U; i < object->class_entry->property_count; i++) {
        pvalue value = object->slots[i];
        if (value.type == PT_OBJECT && value.as.gc != NULL) {
            mark_reachable(objects, count, reachable,
                           object_index(objects, count,
                                        (const pobject *)value.as.gc));
        }
    }
}

size_t pphp_gc_collect(pphp_state *state) {
#if PPHP_ENABLE_CYCLE_GC
    pobject **objects;
    uint32_t *external;
    uint8_t *reachable;
    pobject *cursor;
    size_t count = 0U;
    size_t collected = 0U;
    size_t i;
    if (state == NULL) return 0U;
    for (cursor = state->gc_objects; cursor != NULL; cursor = cursor->gc_next) {
        count++;
    }
    if (count == 0U) return 0U;
    objects = pphp_alloc(count * sizeof(*objects));
    external = pphp_alloc(count * sizeof(*external));
    reachable = pphp_alloc(count * sizeof(*reachable));
    if (objects == NULL || external == NULL || reachable == NULL) {
        pphp_free(objects);
        pphp_free(external);
        pphp_free(reachable);
        return 0U;
    }
    i = 0U;
    for (cursor = state->gc_objects; cursor != NULL; cursor = cursor->gc_next) {
        objects[i] = cursor;
        external[i] = cursor->header.refcnt;
        i++;
    }
    memset(reachable, 0, count * sizeof(*reachable));
    for (i = 0U; i < count; i++) {
        size_t slot;
        for (slot = 0U; slot < objects[i]->class_entry->property_count; slot++) {
            pvalue value = objects[i]->slots[slot];
            if (value.type == PT_OBJECT && value.as.gc != NULL) {
                size_t target = object_index(
                    objects, count, (const pobject *)value.as.gc);
                if (target < count && external[target] != 0U) {
                    external[target]--;
                }
            }
        }
    }
    for (i = 0U; i < count; i++) {
        if (external[i] != 0U) mark_reachable(objects, count, reachable, i);
    }
    for (i = 0U; i < count; i++) {
        if (!reachable[i]) {
            objects[i]->header.flags |= POBJECT_GC_COLLECTING;
            pv_retain(pv_heap(PT_OBJECT, &objects[i]->header));
            collected++;
        }
    }
    for (i = 0U; i < count; i++) {
        size_t slot;
        if (reachable[i]) continue;
        for (slot = 0U; slot < objects[i]->class_entry->property_count; slot++) {
            pvalue value = objects[i]->slots[slot];
            objects[i]->slots[slot] = pv_null();
            pv_release(value);
        }
    }
    for (i = 0U; i < count; i++) {
        if (!reachable[i]) {
            pv_release(pv_heap(PT_OBJECT, &objects[i]->header));
        }
    }
    pphp_free(reachable);
    pphp_free(external);
    pphp_free(objects);
    return collected;
#else
    (void)state;
    return 0U;
#endif
}
