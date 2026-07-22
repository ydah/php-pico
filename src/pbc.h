#ifndef PPHP_PBC_H
#define PPHP_PBC_H

#include <stddef.h>
#include <stdint.h>

#include "pstring.h"
#include "value.h"

typedef struct pcatch {
    uint16_t try_start;
    uint16_t try_end;
    uint16_t handler_pc;
    uint16_t class_constant;
    uint8_t variable_slot;
    uint8_t reserved;
} pcatch;

typedef struct pproto {
    pstring *name;
    const void *declaration;
    uint8_t n_params;
    uint8_t n_required;
    uint8_t variadic;
    uint8_t is_method;
    uint8_t conditional;
    uint8_t owns_code;
    uint8_t n_locals;
    uint16_t max_stack;
    uint8_t *code;
    size_t code_length;
    size_t code_capacity;
    pvalue *constants;
    size_t constant_count;
    size_t constant_capacity;
    pstring **locals;
    pcatch *catches;
    size_t catch_count;
    size_t catch_capacity;
} pproto;

typedef struct pmodule {
    pproto **protos;
    size_t count;
    size_t capacity;
    pro_string *ro_strings;
    size_t ro_string_count;
    const uint8_t *image;
    size_t image_length;
    void *backing;
    uint16_t refcnt;
    uint8_t owns_backing;
    uint8_t heap_allocated;
} pmodule;

pproto *pproto_new(const char *name, size_t length);
void pproto_destroy(pproto *proto);
int pproto_emit_u8(pproto *proto, uint8_t value);
int pproto_emit_u16(pproto *proto, uint16_t value);
int pproto_emit_i32(pproto *proto, int32_t value);
int pproto_patch_i16(pproto *proto, size_t operand_offset, size_t target);
int pproto_add_constant(pproto *proto, pvalue value, uint16_t *index);
int pproto_add_local(pproto *proto, const char *name, size_t length, uint8_t *slot);
int pproto_find_local(const pproto *proto, const char *name, size_t length,
                      uint8_t *slot);
int pproto_add_catch(pproto *proto, pcatch entry);

int pmodule_init(pmodule *module);
void pmodule_destroy(pmodule *module);
void pmodule_retain(pmodule *module);
void pmodule_release(pmodule *module);
int pmodule_add(pmodule *module, pproto *proto);
const pproto *pmodule_find(const pmodule *module, const pstring *name);

int pphp_pbc_write_file(const pmodule *module, const char *path);
int pphp_pbc_read_file(const char *path, pmodule *module);
int pphp_pbc_load(const void *bytes, size_t length, pmodule *module);

#endif
