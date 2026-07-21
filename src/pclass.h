#ifndef PPHP_PCLASS_H
#define PPHP_PCLASS_H

#include <stddef.h>
#include <stdint.h>

#include "pbc.h"
#include "parray.h"
#include "pphp/pphp.h"
#include "value.h"

enum {
    PC_PUBLIC = 1U << 0,
    PC_PROTECTED = 1U << 1,
    PC_PRIVATE = 1U << 2,
    PC_STATIC = 1U << 3,
    PC_ABSTRACT = 1U << 4,
    PC_FINAL = 1U << 5,
    PC_READONLY = 1U << 6,
    PC_INTERFACE = 1U << 7
};

typedef struct pproperty {
    pstring *name;
    uint8_t slot;
    uint8_t flags;
    pvalue default_value;
    pclass *owner;
} pproperty;

typedef struct pmethod {
    pstring *name;
    uint8_t flags;
    const pproto *proto;
    const pmodule *module;
    pclass *owner;
} pmethod;

struct pclass {
    pstring *name;
    struct pclass *parent;
    pproperty *properties;
    size_t property_count;
    size_t property_capacity;
    pmethod *methods;
    size_t method_count;
    size_t method_capacity;
    parray *static_properties;
    parray *constants;
    pclass **interfaces;
    size_t interface_count;
    size_t interface_capacity;
    uint8_t flags;
};

struct pobject {
    pheader header;
    pclass *class_entry;
    pphp_state *owner_state;
    struct pobject *gc_prev;
    struct pobject *gc_next;
    pvalue slots[];
};

pclass *pclass_new(const char *name, size_t length, pclass *parent, uint8_t flags);
void pclass_destroy(pclass *class_entry);
int pclass_add_property(pclass *class_entry, const char *name, size_t length,
                        uint8_t flags, pvalue default_value);
int pclass_add_method(pclass *class_entry, const char *name, size_t length,
                      uint8_t flags, const pproto *proto,
                      const pmodule *module);
int pclass_add_static_property(pclass *class_entry, const char *name,
                               size_t length, pvalue default_value);
int pclass_get_static_property(const pclass *class_entry, const char *name,
                               size_t length, pvalue *value);
int pclass_set_static_property(pclass *class_entry, const char *name,
                               size_t length, pvalue value);
int pclass_add_constant(pclass *class_entry, const char *name, size_t length,
                        pvalue value);
int pclass_get_constant(const pclass *class_entry, const char *name,
                        size_t length, pvalue *value);
int pclass_add_interface(pclass *class_entry, pclass *interface_entry);
int pclass_is_complete(const pclass *class_entry, const pmethod **missing);
const pproperty *pclass_find_property(const pclass *class_entry,
                                      const char *name, size_t length);
const pmethod *pclass_find_method(const pclass *class_entry,
                                  const char *name, size_t length);
pobject *pobject_new(pphp_state *state, pclass *class_entry);
pobject *pobject_clone(const pobject *source);
void pobject_destroy(pobject *object);
int pclass_is_a(const pclass *class_entry, const pclass *expected);
int pclass_member_visible(uint8_t flags, const pclass *owner,
                          const pclass *scope);
int pobject_property_written(const pobject *object, uint8_t slot);
void pobject_mark_property_written(pobject *object, uint8_t slot);
int pphp_register_exception_classes(pphp_state *state);
pobject *pphp_exception_new(pphp_state *state, const char *class_name,
                            const char *message);
int pphp_object_is_throwable(const pphp_state *state, const pobject *object);
const pstring *pphp_exception_message(const pobject *object);
const pvalue *pphp_exception_field(const pobject *object, const char *name,
                                   size_t length);
void pphp_exception_set_location(pobject *object, const char *file,
                                 uint32_t line);
void pphp_exception_set_code(pobject *object, pphp_int code);
void pphp_exception_capture_trace(pphp_state *state, pobject *object);

#endif
