#include "pclass.h"

#include "pphp/pphp.h"

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
    pphp_free(class_entry);
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
                      uint8_t flags, const pproto *proto) {
    pmethod *method;
    size_t i;
    if (class_entry == NULL) return 0;
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

pobject *pobject_new(pclass *class_entry) {
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
    copy = pobject_new(source->class_entry);
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
        if (class_entry == expected) return 1;
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
    for (i = 0U; i < object->class_entry->property_count; i++) {
        pv_release(object->slots[i]);
    }
    pphp_free(object);
}
