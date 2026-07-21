#include "pclass.h"

#include "pphp/pphp.h"
#include "state.h"

#include <stdio.h>
#include <string.h>

static int grow(void **array, size_t element_size, size_t *capacity) {
    size_t next = *capacity == 0U ? 4U : *capacity * 2U;
    void *resized = pphp_realloc(*array, next * element_size);
    if (resized == NULL) return 0;
    *array = resized;
    *capacity = next;
    return 1;
}

static int name_equal_ci(const pstring *name, const char *other, size_t length) {
    size_t i;
    if (name->length != length) return 0;
    for (i = 0U; i < length; i++) {
        unsigned char left = (unsigned char)name->data[i];
        unsigned char right = (unsigned char)other[i];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

pclass *pclass_new(const char *name, size_t length, pclass *parent, uint8_t flags) {
    pclass *class_entry = pphp_alloc(sizeof(*class_entry));
    size_t i;
    if (class_entry == NULL) return NULL;
    memset(class_entry, 0, sizeof(*class_entry));
    class_entry->name = ps_new(name, length);
    class_entry->parent = parent;
    class_entry->flags = flags;
    if (class_entry->name == NULL) {
        pphp_free(class_entry);
        return NULL;
    }
    if (parent != NULL) {
        for (i = 0U; i < parent->property_count; i++) {
            const pproperty *property = &parent->properties[i];
            if (!pclass_add_property(class_entry, property->name->data,
                                     property->name->length, property->flags,
                                     property->default_value)) {
                pclass_destroy(class_entry);
                return NULL;
            }
            class_entry->properties[class_entry->property_count - 1U].owner =
                property->owner;
        }
    }
    return class_entry;
}

void pclass_destroy(pclass *class_entry) {
    size_t i;
    if (class_entry == NULL) return;
    for (i = 0U; i < class_entry->property_count; i++) {
        ps_destroy(class_entry->properties[i].name);
        pv_release(class_entry->properties[i].default_value);
    }
    for (i = 0U; i < class_entry->method_count; i++) {
        ps_destroy(class_entry->methods[i].name);
    }
    ps_destroy(class_entry->name);
    pphp_free(class_entry->properties);
    pphp_free(class_entry->methods);
    pphp_free(class_entry->interfaces);
    pa_destroy(class_entry->static_properties);
    pa_destroy(class_entry->constants);
    pphp_free(class_entry);
}

static int class_table_add(parray **table, const char *name, size_t length,
                           pvalue value) {
    pstring *key;
    pvalue existing = pv_null();
    int added;
    if (*table == NULL) {
        *table = pa_new(4U);
        if (*table == NULL) return 0;
    }
    key = ps_new(name, length);
    if (key == NULL) return 0;
    if (pa_get(*table, pv_heap(PT_STRING, &key->header), &existing)) {
        pv_release(existing);
        ps_destroy(key);
        return 0;
    }
    added = pa_set(*table, pv_heap(PT_STRING, &key->header), value);
    pv_release(pv_heap(PT_STRING, &key->header));
    return added;
}

static int class_table_get(const pclass *class_entry, int constants,
                           const char *name, size_t length, pvalue *value) {
    pstring *key = ps_new(name, length);
    const pclass *cursor = class_entry;
    if (key == NULL) return 0;
    while (cursor != NULL) {
        const parray *table = constants ? cursor->constants
                                        : cursor->static_properties;
        if (table != NULL &&
            pa_get(table, pv_heap(PT_STRING, &key->header), value)) {
            ps_destroy(key);
            return 1;
        }
        cursor = cursor->parent;
    }
    ps_destroy(key);
    return 0;
}

int pclass_add_static_property(pclass *class_entry, const char *name,
                               size_t length, pvalue default_value) {
    return class_entry != NULL &&
           class_table_add(&class_entry->static_properties, name, length,
                           default_value);
}

int pclass_get_static_property(const pclass *class_entry, const char *name,
                               size_t length, pvalue *value) {
    return class_entry != NULL && value != NULL &&
           class_table_get(class_entry, 0, name, length, value);
}

int pclass_set_static_property(pclass *class_entry, const char *name,
                               size_t length, pvalue value) {
    pstring *key;
    pclass *cursor = class_entry;
    if (class_entry == NULL) return 0;
    key = ps_new(name, length);
    if (key == NULL) return 0;
    while (cursor != NULL) {
        pvalue existing = pv_null();
        if (cursor->static_properties != NULL &&
            pa_get(cursor->static_properties,
                   pv_heap(PT_STRING, &key->header), &existing)) {
            int set;
            pv_release(existing);
            set = pa_set(cursor->static_properties,
                         pv_heap(PT_STRING, &key->header), value);
            ps_destroy(key);
            return set;
        }
        cursor = cursor->parent;
    }
    ps_destroy(key);
    return 0;
}

int pclass_add_constant(pclass *class_entry, const char *name, size_t length,
                        pvalue value) {
    return class_entry != NULL &&
           class_table_add(&class_entry->constants, name, length, value);
}

int pclass_get_constant(const pclass *class_entry, const char *name,
                        size_t length, pvalue *value) {
    return class_entry != NULL && value != NULL &&
           class_table_get(class_entry, 1, name, length, value);
}

int pclass_add_interface(pclass *class_entry, pclass *interface_entry) {
    pclass **resized;
    size_t capacity;
    size_t i;
    if (class_entry == NULL || interface_entry == NULL ||
        (interface_entry->flags & PC_INTERFACE) == 0U) return 0;
    for (i = 0U; i < class_entry->interface_count; i++) {
        if (class_entry->interfaces[i] == interface_entry) return 0;
    }
    if (class_entry->interface_count == class_entry->interface_capacity) {
        capacity = class_entry->interface_capacity == 0U
                       ? 2U : class_entry->interface_capacity * 2U;
        resized = pphp_realloc(class_entry->interfaces,
                               capacity * sizeof(*resized));
        if (resized == NULL) return 0;
        class_entry->interfaces = resized;
        class_entry->interface_capacity = capacity;
    }
    class_entry->interfaces[class_entry->interface_count++] = interface_entry;
    return 1;
}

static int method_implements(const pclass *class_entry,
                             const pmethod *requirement) {
    const pmethod *implementation = pclass_find_method(
        class_entry, requirement->name->data, requirement->name->length);
    if (implementation == NULL ||
        (implementation->flags & PC_ABSTRACT) != 0U) return 0;
    if ((requirement->flags & PC_STATIC) !=
        (implementation->flags & PC_STATIC)) return 0;
    if ((requirement->owner->flags & PC_INTERFACE) != 0U &&
        (implementation->flags & PC_PUBLIC) == 0U) return 0;
    return 1;
}

static int interfaces_complete(const pclass *class_entry,
                               const pclass *interface_entry,
                               const pmethod **missing) {
    size_t i;
    for (i = 0U; i < interface_entry->method_count; i++) {
        if (!method_implements(class_entry, &interface_entry->methods[i])) {
            if (missing != NULL) *missing = &interface_entry->methods[i];
            return 0;
        }
    }
    for (i = 0U; i < interface_entry->interface_count; i++) {
        if (!interfaces_complete(class_entry, interface_entry->interfaces[i],
                                 missing)) return 0;
    }
    return 1;
}

int pclass_is_complete(const pclass *class_entry, const pmethod **missing) {
    const pclass *cursor;
    size_t i;
    if (missing != NULL) *missing = NULL;
    if (class_entry == NULL) return 0;
    for (cursor = class_entry; cursor != NULL; cursor = cursor->parent) {
        for (i = 0U; i < cursor->method_count; i++) {
            if ((cursor->methods[i].flags & PC_ABSTRACT) != 0U &&
                !method_implements(class_entry, &cursor->methods[i])) {
                if (missing != NULL) *missing = &cursor->methods[i];
                return 0;
            }
        }
        for (i = 0U; i < cursor->interface_count; i++) {
            if (!interfaces_complete(class_entry, cursor->interfaces[i],
                                     missing)) return 0;
        }
    }
    return 1;
}

int pclass_add_property(pclass *class_entry, const char *name, size_t length,
                        uint8_t flags, pvalue default_value) {
    pproperty *property;
    if (class_entry == NULL || class_entry->property_count >= UINT8_MAX ||
        pclass_find_property(class_entry, name, length) != NULL) return 0;
    if (class_entry->property_count == class_entry->property_capacity &&
        !grow((void **)&class_entry->properties, sizeof(*class_entry->properties),
              &class_entry->property_capacity)) return 0;
    property = &class_entry->properties[class_entry->property_count];
    property->name = ps_new(name, length);
    if (property->name == NULL) return 0;
    property->slot = (uint8_t)class_entry->property_count;
    property->flags = flags;
    property->default_value = default_value;
    property->owner = class_entry;
    pv_retain(default_value);
    class_entry->property_count++;
    return 1;
}

int pclass_add_method(pclass *class_entry, const char *name, size_t length,
                      uint8_t flags, const pproto *proto,
                      const pmodule *module) {
    pmethod *method;
    size_t i;
    const pmethod *inherited;
    if (class_entry == NULL) return 0;
    inherited = class_entry->parent == NULL
                    ? NULL : pclass_find_method(class_entry->parent,
                                                name, length);
    if (inherited != NULL && (inherited->flags & PC_FINAL) != 0U) return 0;
    for (i = 0U; i < class_entry->method_count; i++) {
        if (name_equal_ci(class_entry->methods[i].name, name, length)) return 0;
    }
    if (class_entry->method_count == class_entry->method_capacity &&
        !grow((void **)&class_entry->methods, sizeof(*class_entry->methods),
              &class_entry->method_capacity)) return 0;
    method = &class_entry->methods[class_entry->method_count];
    method->name = ps_new(name, length);
    if (method->name == NULL) return 0;
    method->flags = flags;
    method->proto = proto;
    method->module = module;
    method->owner = class_entry;
    class_entry->method_count++;
    return 1;
}

const pproperty *pclass_find_property(const pclass *class_entry,
                                      const char *name, size_t length) {
    size_t i;
    if (class_entry == NULL) return NULL;
    for (i = 0U; i < class_entry->property_count; i++) {
        if (ps_equal_bytes(class_entry->properties[i].name, name, length)) {
            return &class_entry->properties[i];
        }
    }
    return NULL;
}

const pmethod *pclass_find_method(const pclass *class_entry,
                                  const char *name, size_t length) {
    const pclass *cursor = class_entry;
    while (cursor != NULL) {
        size_t i;
        for (i = 0U; i < cursor->method_count; i++) {
            if (name_equal_ci(cursor->methods[i].name, name, length)) {
                return &cursor->methods[i];
            }
        }
        cursor = cursor->parent;
    }
    return NULL;
}

pobject *pobject_new(pphp_state *state, pclass *class_entry) {
    pobject *object;
    size_t i;
    if (class_entry == NULL || (class_entry->flags & (PC_ABSTRACT | PC_INTERFACE)) != 0U) {
        return NULL;
    }
    object = pphp_alloc(sizeof(*object) +
                        class_entry->property_count * sizeof(*object->slots) +
                        class_entry->property_count);
    if (object == NULL) return NULL;
    object->header.refcnt = 1U;
    object->header.type = PT_OBJECT;
    object->header.flags = 0U;
    object->class_entry = class_entry;
    object->owner_state = state;
    object->gc_prev = NULL;
    object->gc_next = state == NULL ? NULL : state->gc_objects;
    if (state != NULL) {
        if (state->gc_objects != NULL) state->gc_objects->gc_prev = object;
        state->gc_objects = object;
    }
    for (i = 0U; i < class_entry->property_count; i++) {
        object->slots[i] = class_entry->properties[i].default_value;
        pv_retain(object->slots[i]);
    }
    memset((uint8_t *)(object->slots + class_entry->property_count), 0,
           class_entry->property_count);
    return object;
}

pobject *pobject_clone(const pobject *source) {
    pobject *copy;
    size_t i;
    uint8_t *written;
    const uint8_t *source_written;
    if (source == NULL) return NULL;
    copy = pobject_new(source->owner_state, source->class_entry);
    if (copy == NULL) return NULL;
    for (i = 0U; i < source->class_entry->property_count; i++) {
        pv_release(copy->slots[i]);
        copy->slots[i] = source->slots[i];
        pv_retain(copy->slots[i]);
    }
    written = (uint8_t *)(copy->slots + source->class_entry->property_count);
    source_written = (const uint8_t *)(
        source->slots + source->class_entry->property_count);
    memcpy(written, source_written, source->class_entry->property_count);
    return copy;
}

int pclass_is_a(const pclass *class_entry, const pclass *expected) {
    while (class_entry != NULL) {
        size_t i;
        if (class_entry == expected) return 1;
        for (i = 0U; i < class_entry->interface_count; i++) {
            if (pclass_is_a(class_entry->interfaces[i], expected)) return 1;
        }
        class_entry = class_entry->parent;
    }
    return 0;
}

int pclass_member_visible(uint8_t flags, const pclass *owner,
                          const pclass *scope) {
    if ((flags & PC_PRIVATE) != 0U) return scope == owner;
    if ((flags & PC_PROTECTED) != 0U) {
        return scope != NULL &&
               (pclass_is_a(scope, owner) || pclass_is_a(owner, scope));
    }
    return 1;
}

int pobject_property_written(const pobject *object, uint8_t slot) {
    const uint8_t *written;
    if (object == NULL || slot >= object->class_entry->property_count) return 0;
    written = (const uint8_t *)(object->slots + object->class_entry->property_count);
    return written[slot] != 0U;
}

void pobject_mark_property_written(pobject *object, uint8_t slot) {
    uint8_t *written;
    if (object == NULL || slot >= object->class_entry->property_count) return;
    written = (uint8_t *)(object->slots + object->class_entry->property_count);
    written[slot] = 1U;
}

void pobject_destroy(pobject *object) {
    size_t i;
    if (object == NULL) return;
    if (object->owner_state != NULL &&
        (object->header.flags & UINT8_C(0xc0)) == 0U) {
        const pmethod *destructor = pclass_find_method(
            object->class_entry, "__destruct", 10U);
        if (destructor != NULL && (destructor->flags & PC_STATIC) == 0U) {
            pstring *method_name = ps_new("__destruct", 10U);
            parray *callable = pa_new(2U);
            pvalue ignored = pv_null();
            char saved_error[256];
            uint32_t saved_line = object->owner_state->error_line;
            (void)snprintf(saved_error, sizeof(saved_error), "%s",
                           object->owner_state->error);
            object->header.flags |= UINT8_C(0x80);
            object->header.refcnt = 1U;
            if (object->owner_state->invoke != NULL &&
                method_name != NULL && callable != NULL &&
                pa_push(callable, pv_heap(PT_OBJECT, &object->header)) &&
                pa_push(callable, pv_heap(PT_STRING, &method_name->header))) {
                (void)object->owner_state->invoke(
                    object->owner_state,
                    pv_heap(PT_ARRAY, &callable->header), NULL, 0U, &ignored);
                pv_release(ignored);
            }
            if (callable != NULL) {
                pv_release(pv_heap(PT_ARRAY, &callable->header));
            }
            pv_release(pv_heap(PT_STRING,
                               method_name == NULL ? NULL : &method_name->header));
            object->header.refcnt = 0U;
            (void)snprintf(object->owner_state->error,
                           sizeof(object->owner_state->error), "%s",
                           saved_error);
            object->owner_state->error_line = saved_line;
        }
    }
    if (object->owner_state != NULL) {
        if (object->gc_prev != NULL) {
            object->gc_prev->gc_next = object->gc_next;
        } else {
            object->owner_state->gc_objects = object->gc_next;
        }
        if (object->gc_next != NULL) object->gc_next->gc_prev = object->gc_prev;
    }
    for (i = 0U; i < object->class_entry->property_count; i++) {
        pv_release(object->slots[i]);
    }
    pphp_free(object);
}
