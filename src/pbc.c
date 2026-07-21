#include "pbc.h"

#include "opcode.h"
#include "pphp/fs.h"
#include "pphp/pphp.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int grow_array(void **array, size_t element_size, size_t *capacity,
                      size_t minimum) {
    size_t next = *capacity == 0U ? 8U : *capacity * 2U;
    void *resized;
    if (next < minimum) {
        next = minimum;
    }
    if (next > SIZE_MAX / element_size) {
        return 0;
    }
    resized = pphp_realloc(*array, next * element_size);
    if (resized == NULL) {
        return 0;
    }
    *array = resized;
    *capacity = next;
    return 1;
}

pproto *pproto_new(const char *name, size_t length) {
    pproto *proto = pphp_alloc(sizeof(*proto));
    if (proto == NULL) {
        return NULL;
    }
    memset(proto, 0, sizeof(*proto));
    proto->name = ps_new(name == NULL ? "" : name, name == NULL ? 0U : length);
    if (proto->name == NULL) {
        pphp_free(proto);
        return NULL;
    }
    return proto;
}

void pproto_destroy(pproto *proto) {
    size_t i;
    if (proto == NULL) {
        return;
    }
    for (i = 0U; i < proto->constant_count; i++) {
        pv_release(proto->constants[i]);
    }
    if (proto->locals != NULL) {
        for (i = 0U; i < proto->n_locals; i++) {
            ps_destroy(proto->locals[i]);
        }
    }
    ps_destroy(proto->name);
    pphp_free(proto->locals);
    pphp_free(proto->constants);
    pphp_free(proto->code);
    pphp_free(proto->catches);
    pphp_free(proto);
}

int pproto_emit_u8(pproto *proto, uint8_t value) {
    if (proto->code_length == proto->code_capacity &&
        !grow_array((void **)&proto->code, sizeof(*proto->code),
                    &proto->code_capacity, proto->code_length + 1U)) {
        return 0;
    }
    proto->code[proto->code_length++] = value;
    return 1;
}

int pproto_emit_u16(pproto *proto, uint16_t value) {
    return pproto_emit_u8(proto, (uint8_t)(value & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)(value >> 8U));
}

int pproto_emit_i32(pproto *proto, int32_t value) {
    uint32_t bits = (uint32_t)value;
    return pproto_emit_u8(proto, (uint8_t)(bits & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)((bits >> 8U) & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)((bits >> 16U) & 0xffU)) &&
           pproto_emit_u8(proto, (uint8_t)((bits >> 24U) & 0xffU));
}

int pproto_patch_i16(pproto *proto, size_t operand_offset, size_t target) {
    ptrdiff_t relative;
    int16_t encoded;
    if (operand_offset + 2U > proto->code_length) {
        return 0;
    }
    relative = (ptrdiff_t)target - (ptrdiff_t)(operand_offset + 2U);
    if (relative < INT16_MIN || relative > INT16_MAX) {
        return 0;
    }
    encoded = (int16_t)relative;
    proto->code[operand_offset] = (uint8_t)((uint16_t)encoded & 0xffU);
    proto->code[operand_offset + 1U] = (uint8_t)((uint16_t)encoded >> 8U);
    return 1;
}

int pproto_add_constant(pproto *proto, pvalue value, uint16_t *index) {
    if (proto->constant_count >= UINT16_MAX) {
        return 0;
    }
    if (proto->constant_count == proto->constant_capacity &&
        !grow_array((void **)&proto->constants, sizeof(*proto->constants),
                    &proto->constant_capacity, proto->constant_count + 1U)) {
        return 0;
    }
    pv_retain(value);
    proto->constants[proto->constant_count] = value;
    *index = (uint16_t)proto->constant_count;
    proto->constant_count++;
    return 1;
}

int pproto_find_local(const pproto *proto, const char *name, size_t length,
                      uint8_t *slot) {
    size_t i;
    for (i = 0U; i < proto->n_locals; i++) {
        if (ps_equal_bytes(proto->locals[i], name, length)) {
            *slot = (uint8_t)i;
            return 1;
        }
    }
    return 0;
}

int pproto_add_local(pproto *proto, const char *name, size_t length, uint8_t *slot) {
    pstring **resized;
    pstring *string;
    if (pproto_find_local(proto, name, length, slot)) {
        return 1;
    }
    if (proto->n_locals == UINT8_MAX) {
        return 0;
    }
    resized = pphp_realloc(proto->locals,
                           ((size_t)proto->n_locals + 1U) * sizeof(*proto->locals));
    if (resized == NULL) {
        return 0;
    }
    proto->locals = resized;
    string = ps_new(name, length);
    if (string == NULL) {
        return 0;
    }
    *slot = proto->n_locals;
    proto->locals[proto->n_locals++] = string;
    return 1;
}

int pproto_add_catch(pproto *proto, pcatch entry) {
    if (proto->catch_count >= UINT16_MAX) return 0;
    if (proto->catch_count == proto->catch_capacity &&
        !grow_array((void **)&proto->catches, sizeof(*proto->catches),
                    &proto->catch_capacity, proto->catch_count + 1U)) return 0;
    proto->catches[proto->catch_count++] = entry;
    return 1;
}

int pmodule_init(pmodule *module) {
    memset(module, 0, sizeof(*module));
    return 1;
}

void pmodule_destroy(pmodule *module) {
    size_t i;
    for (i = 0U; i < module->count; i++) {
        pproto_destroy(module->protos[i]);
    }
    pphp_free(module->protos);
    memset(module, 0, sizeof(*module));
}

int pmodule_add(pmodule *module, pproto *proto) {
    if (module->count == module->capacity &&
        !grow_array((void **)&module->protos, sizeof(*module->protos),
                    &module->capacity, module->count + 1U)) {
        return 0;
    }
    module->protos[module->count++] = proto;
    return 1;
}

const pproto *pmodule_find(const pmodule *module, const pstring *name) {
    size_t i;
    size_t j;
    for (i = 1U; i < module->count; i++) {
        const pstring *candidate = module->protos[i]->name;
        if (module->protos[i]->conditional) continue;
        int equal = candidate->length == name->length;
        for (j = 0U; equal && j < name->length; j++) {
            unsigned char left = (unsigned char)candidate->data[j];
            unsigned char right = (unsigned char)name->data[j];
            if (left >= 'A' && left <= 'Z') {
                left = (unsigned char)(left + ('a' - 'A'));
            }
            if (right >= 'A' && right <= 'Z') {
                right = (unsigned char)(right + ('a' - 'A'));
            }
            equal = left == right;
        }
        if (equal) {
            return module->protos[i];
        }
    }
    return NULL;
}

typedef struct string_list {
    pstring **items;
    size_t count;
    size_t capacity;
} string_list;

static size_t align4(size_t value) {
    return (value + 3U) & ~(size_t)3U;
}

static void put_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)(value & 0xffU);
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)(value & 0xffU);
    bytes[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
    bytes[offset + 2U] = (uint8_t)((value >> 16U) & 0xffU);
    bytes[offset + 3U] = (uint8_t)((value >> 24U) & 0xffU);
}

static uint16_t get_u16(const uint8_t *bytes, size_t offset) {
    return (uint16_t)((uint16_t)bytes[offset] |
                      (uint16_t)((uint16_t)bytes[offset + 1U] << 8U));
}

static uint32_t get_u32(const uint8_t *bytes, size_t offset) {
    return (uint32_t)bytes[offset] |
           ((uint32_t)bytes[offset + 1U] << 8U) |
           ((uint32_t)bytes[offset + 2U] << 16U) |
           ((uint32_t)bytes[offset + 3U] << 24U);
}

static int strings_add(string_list *list, pstring *string, uint16_t *index) {
    size_t i;
    for (i = 0U; i < list->count; i++) {
        if (ps_equal(list->items[i], string)) {
            *index = (uint16_t)i;
            return 1;
        }
    }
    if (list->count >= UINT16_MAX) {
        return 0;
    }
    if (list->count == list->capacity &&
        !grow_array((void **)&list->items, sizeof(*list->items), &list->capacity,
                    list->count + 1U)) {
        return 0;
    }
    list->items[list->count] = string;
    *index = (uint16_t)list->count;
    list->count++;
    return 1;
}

static int collect_strings(const pmodule *module, string_list *strings) {
    size_t i;
    memset(strings, 0, sizeof(*strings));
    for (i = 0U; i < module->count; i++) {
        size_t j;
        uint16_t ignored;
        if (!strings_add(strings, module->protos[i]->name, &ignored)) {
            return 0;
        }
        for (j = 0U; j < module->protos[i]->constant_count; j++) {
            pvalue value = module->protos[i]->constants[j];
            if (value.type == PT_STRING &&
                !strings_add(strings, (pstring *)value.as.gc, &ignored)) {
                return 0;
            }
        }
        for (j = 0U; j < module->protos[i]->n_locals; j++) {
            if (!strings_add(strings, module->protos[i]->locals[j], &ignored)) {
                return 0;
            }
        }
    }
    return 1;
}

static size_t serialized_constant_size(pvalue value) {
    return value.type == PT_FLOAT && sizeof(pphp_float) == 8U ? 16U : 8U;
}

static int string_index(const string_list *strings, const pstring *string,
                        uint16_t *index) {
    size_t i;
    for (i = 0U; i < strings->count; i++) {
        if (ps_equal(strings->items[i], string)) {
            *index = (uint16_t)i;
            return 1;
        }
    }
    return 0;
}

int pphp_pbc_write_file(const pmodule *module, const char *path) {
    string_list strings;
    uint32_t *string_offsets = NULL;
    uint32_t *proto_offsets = NULL;
    size_t total;
    size_t offset;
    size_t i;
    uint8_t *bytes = NULL;
    pphp_file *file = NULL;
    int result = PPHP_E_NOMEM;
    if (module == NULL || module->count == 0U || module->count > UINT16_MAX ||
        path == NULL || !collect_strings(module, &strings)) {
        return PPHP_E_NOMEM;
    }
    string_offsets = pphp_alloc(strings.count * sizeof(*string_offsets));
    proto_offsets = pphp_alloc(module->count * sizeof(*proto_offsets));
    if ((strings.count != 0U && string_offsets == NULL) || proto_offsets == NULL) {
        goto done;
    }
    total = align4(16U + strings.count * 4U + module->count * 4U);
    for (i = 0U; i < strings.count; i++) {
        if (total > UINT32_MAX) goto done;
        string_offsets[i] = (uint32_t)total;
        total = align4(total + 2U + strings.items[i]->length + 1U);
    }
    for (i = 0U; i < module->count; i++) {
        size_t j;
        const pproto *proto = module->protos[i];
        if (proto->code_length > UINT16_MAX || proto->constant_count > UINT16_MAX ||
            total > UINT32_MAX) goto done;
        proto_offsets[i] = (uint32_t)total;
        total += 16U + align4(proto->code_length);
        for (j = 0U; j < proto->constant_count; j++) {
            total += serialized_constant_size(proto->constants[j]);
        }
        total += (size_t)proto->n_locals * 2U;
        total += proto->catch_count * 10U;
        total = align4(total);
    }
    if (total > UINT32_MAX) goto done;
    bytes = pphp_alloc(total);
    if (bytes == NULL) goto done;
    memset(bytes, 0, total);
    memcpy(bytes, "PPBC", 4U);
    put_u16(bytes, 4U, (uint16_t)PPHP_PBC_FORMAT_VERSION);
    put_u16(bytes, 6U, (uint16_t)((PPHP_INT64 ? 1U : 0U) |
                                  (PPHP_USE_DOUBLE ? 2U : 0U) |
                                  (PPHP_LINE_INFO ? 4U : 0U)));
    put_u32(bytes, 8U, (uint32_t)total);
    put_u16(bytes, 12U, (uint16_t)module->count);
    put_u16(bytes, 14U, (uint16_t)strings.count);
    offset = 16U;
    for (i = 0U; i < strings.count; i++, offset += 4U) {
        put_u32(bytes, offset, string_offsets[i]);
    }
    for (i = 0U; i < module->count; i++, offset += 4U) {
        put_u32(bytes, offset, proto_offsets[i]);
    }
    for (i = 0U; i < strings.count; i++) {
        offset = string_offsets[i];
        put_u16(bytes, offset, strings.items[i]->length);
        memcpy(bytes + offset + 2U, strings.items[i]->data, strings.items[i]->length);
    }
    for (i = 0U; i < module->count; i++) {
        const pproto *proto = module->protos[i];
        uint16_t name_sid;
        size_t j;
        offset = proto_offsets[i];
        if (!string_index(&strings, proto->name, &name_sid)) goto done;
        bytes[offset] = proto->n_params;
        bytes[offset + 1U] = proto->n_required;
        bytes[offset + 2U] = (uint8_t)((proto->variadic ? 1U : 0U) |
                                        (proto->is_method ? 2U : 0U) |
                                        (proto->conditional ? 8U : 0U));
        bytes[offset + 3U] = proto->n_locals;
        put_u16(bytes, offset + 6U, proto->max_stack);
        put_u16(bytes, offset + 8U, (uint16_t)proto->code_length);
        put_u16(bytes, offset + 10U, (uint16_t)proto->constant_count);
        put_u16(bytes, offset + 12U, (uint16_t)proto->catch_count);
        put_u16(bytes, offset + 14U, name_sid);
        offset += 16U;
        memcpy(bytes + offset, proto->code, proto->code_length);
        offset += align4(proto->code_length);
        for (j = 0U; j < proto->constant_count; j++) {
            pvalue value = proto->constants[j];
            if (value.type == PT_INT) {
                bytes[offset] = 0U;
                put_u32(bytes, offset + 4U, (uint32_t)(int32_t)value.as.i);
                offset += 8U;
            } else if (value.type == PT_FLOAT && sizeof(pphp_float) == 4U) {
                uint32_t bits;
                float number = (float)value.as.f;
                memcpy(&bits, &number, sizeof(bits));
                bytes[offset] = 1U;
                put_u32(bytes, offset + 4U, bits);
                offset += 8U;
            } else if (value.type == PT_STRING) {
                uint16_t sid;
                if (!string_index(&strings, (pstring *)value.as.gc, &sid)) goto done;
                bytes[offset] = 2U;
                put_u32(bytes, offset + 4U, sid);
                offset += 8U;
            } else if (value.type == PT_FLOAT) {
                uint64_t bits;
                double number = (double)value.as.f;
                memcpy(&bits, &number, sizeof(bits));
                bytes[offset] = 3U;
                put_u32(bytes, offset + 8U, (uint32_t)(bits & UINT32_MAX));
                put_u32(bytes, offset + 12U, (uint32_t)(bits >> 32U));
                offset += 16U;
            } else {
                goto done;
            }
        }
        for (j = 0U; j < proto->n_locals; j++) {
            uint16_t local_sid;
            if (!string_index(&strings, proto->locals[j], &local_sid)) goto done;
            put_u16(bytes, offset, local_sid);
            offset += 2U;
        }
        for (j = 0U; j < proto->catch_count; j++) {
            const pcatch *entry = &proto->catches[j];
            uint16_t class_sid = UINT16_MAX;
            if (entry->class_constant != UINT16_MAX) {
                if (entry->class_constant >= proto->constant_count ||
                    proto->constants[entry->class_constant].type != PT_STRING ||
                    !string_index(&strings,
                                  (pstring *)proto->constants[entry->class_constant].as.gc,
                                  &class_sid)) {
                    goto done;
                }
            }
            put_u16(bytes, offset, entry->try_start);
            put_u16(bytes, offset + 2U, entry->try_end);
            put_u16(bytes, offset + 4U, entry->handler_pc);
            put_u16(bytes, offset + 6U, class_sid);
            bytes[offset + 8U] = entry->variable_slot;
            bytes[offset + 9U] = 0U;
            offset += 10U;
        }
    }
    file = pphp_fs_open(path, "wb");
    if (file == NULL) {
        result = PPHP_E_IO;
        goto done;
    }
    result = pphp_fs_write(file, bytes, total) == (int64_t)total
                 ? PPHP_OK : PPHP_E_IO;
done:
    if (file != NULL && !pphp_fs_close(file) && result == PPHP_OK) {
        result = PPHP_E_IO;
    }
    pphp_free(bytes);
    pphp_free(string_offsets);
    pphp_free(proto_offsets);
    pphp_free(strings.items);
    return result;
}

int pphp_pbc_read_file(const char *path, pmodule *module) {
    pphp_file *file = pphp_fs_open(path, "rb");
    int64_t file_size;
    uint8_t *bytes;
    int64_t read_count;
    int result;
    if (file == NULL) return PPHP_E_IO;
    file_size = pphp_fs_seek(file, 0, PPHP_FS_SEEK_END);
    if (file_size < 0 || (uint64_t)file_size > (uint64_t)SIZE_MAX ||
        pphp_fs_seek(file, 0, PPHP_FS_SEEK_SET) < 0) {
        (void)pphp_fs_close(file);
        return PPHP_E_IO;
    }
    bytes = pphp_alloc((size_t)file_size);
    if (bytes == NULL) {
        (void)pphp_fs_close(file);
        return PPHP_E_NOMEM;
    }
    read_count = pphp_fs_read(file, bytes, (size_t)file_size);
    (void)pphp_fs_close(file);
    if (read_count != file_size) {
        pphp_free(bytes);
        return PPHP_E_IO;
    }
    result = pphp_pbc_load(bytes, (size_t)read_count, module);
    pphp_free(bytes);
    return result;
}

int pphp_pbc_load(const void *data, size_t length, pmodule *module) {
    const uint8_t *bytes = data;
    const uint16_t expected_flags =
        (uint16_t)((PPHP_INT64 ? 1U : 0U) |
                   (PPHP_USE_DOUBLE ? 2U : 0U) |
                   (PPHP_LINE_INFO ? 4U : 0U));
    uint16_t n_protos;
    uint16_t n_strings;
    size_t table_end;
    pstring **strings = NULL;
    size_t i;
    int result = PPHP_E_PARSE;
    if (bytes == NULL || length < 16U || memcmp(bytes, "PPBC", 4U) != 0 ||
        get_u16(bytes, 4U) != (uint16_t)PPHP_PBC_FORMAT_VERSION ||
        get_u16(bytes, 6U) != expected_flags ||
        get_u32(bytes, 8U) != length) {
        return PPHP_E_PARSE;
    }
    n_protos = get_u16(bytes, 12U);
    n_strings = get_u16(bytes, 14U);
    table_end = 16U + (size_t)n_strings * 4U + (size_t)n_protos * 4U;
    if (n_protos == 0U || table_end > length) return PPHP_E_PARSE;
    strings = pphp_alloc((size_t)n_strings * sizeof(*strings));
    if (n_strings != 0U && strings == NULL) return PPHP_E_NOMEM;
    if (n_strings != 0U) {
        memset(strings, 0, (size_t)n_strings * sizeof(*strings));
    }
    if (!pmodule_init(module)) {
        result = PPHP_E_NOMEM;
        goto done;
    }
    for (i = 0U; i < n_strings; i++) {
        size_t offset = get_u32(bytes, 16U + i * 4U);
        uint16_t string_length;
        if (offset + 2U > length) goto failed_module;
        string_length = get_u16(bytes, offset);
        if (offset + 2U + string_length + 1U > length) goto failed_module;
        strings[i] = ps_new((const char *)bytes + offset + 2U, string_length);
        if (strings[i] == NULL) {
            result = PPHP_E_NOMEM;
            goto failed_module;
        }
    }
    for (i = 0U; i < n_protos; i++) {
        size_t offset = get_u32(bytes, 16U + (size_t)n_strings * 4U + i * 4U);
        uint16_t code_length;
        uint16_t constant_count;
        uint16_t catch_count;
        uint16_t name_sid;
        uint8_t local_count;
        pproto *proto;
        size_t j;
        if (offset + 16U > length) goto failed_module;
        code_length = get_u16(bytes, offset + 8U);
        constant_count = get_u16(bytes, offset + 10U);
        catch_count = get_u16(bytes, offset + 12U);
        name_sid = get_u16(bytes, offset + 14U);
        if (name_sid >= n_strings || offset + 16U + align4(code_length) > length) {
            goto failed_module;
        }
        proto = pproto_new(strings[name_sid]->data, strings[name_sid]->length);
        if (proto == NULL || !pmodule_add(module, proto)) {
            pproto_destroy(proto);
            result = PPHP_E_NOMEM;
            goto failed_module;
        }
        proto->n_params = bytes[offset];
        proto->n_required = bytes[offset + 1U];
        proto->variadic = (uint8_t)(bytes[offset + 2U] & 1U);
        proto->is_method = (uint8_t)((bytes[offset + 2U] & 2U) != 0U);
        proto->conditional = (uint8_t)((bytes[offset + 2U] & 8U) != 0U);
        local_count = bytes[offset + 3U];
        proto->max_stack = get_u16(bytes, offset + 6U);
        proto->code = pphp_alloc(code_length);
        if (code_length != 0U && proto->code == NULL) {
            result = PPHP_E_NOMEM;
            goto failed_module;
        }
        memcpy(proto->code, bytes + offset + 16U, code_length);
        proto->code_length = code_length;
        proto->code_capacity = code_length;
        offset += 16U + align4(code_length);
        for (j = 0U; j < constant_count; j++) {
            pvalue value;
            uint16_t ignored;
            uint8_t tag;
            if (offset + 8U > length) goto failed_module;
            tag = bytes[offset];
            if (tag == 0U) {
                value = pv_int((pphp_int)(int32_t)get_u32(bytes, offset + 4U));
                offset += 8U;
            } else if (tag == 1U) {
                uint32_t bits = get_u32(bytes, offset + 4U);
                float number;
                memcpy(&number, &bits, sizeof(number));
                value = pv_float((pphp_float)number);
                offset += 8U;
            } else if (tag == 2U) {
                uint32_t sid = get_u32(bytes, offset + 4U);
                if (sid >= n_strings) goto failed_module;
                value = pv_heap(PT_STRING, &strings[sid]->header);
                offset += 8U;
            } else if (tag == 3U) {
                uint64_t bits;
                double number;
                if (offset + 16U > length) goto failed_module;
                bits = get_u32(bytes, offset + 8U);
                bits |= (uint64_t)get_u32(bytes, offset + 12U) << 32U;
                memcpy(&number, &bits, sizeof(number));
                value = pv_float((pphp_float)number);
                offset += 16U;
            } else {
                goto failed_module;
            }
            if (!pproto_add_constant(proto, value, &ignored)) {
                result = PPHP_E_NOMEM;
                goto failed_module;
            }
        }
        for (j = 0U; j < local_count; j++) {
            uint16_t local_sid;
            uint8_t slot;
            if (offset + 2U > length) goto failed_module;
            local_sid = get_u16(bytes, offset);
            if (local_sid >= n_strings ||
                !pproto_add_local(proto, strings[local_sid]->data,
                                  strings[local_sid]->length, &slot) ||
                slot != j) {
                goto failed_module;
            }
            offset += 2U;
        }
        for (j = 0U; j < catch_count; j++) {
            pcatch entry;
            uint16_t class_sid;
            if (offset + 10U > length) goto failed_module;
            entry.try_start = get_u16(bytes, offset);
            entry.try_end = get_u16(bytes, offset + 2U);
            entry.handler_pc = get_u16(bytes, offset + 4U);
            class_sid = get_u16(bytes, offset + 6U);
            entry.class_constant = UINT16_MAX;
            entry.variable_slot = bytes[offset + 8U];
            entry.reserved = 0U;
            if (entry.try_start > entry.try_end || entry.try_end > code_length ||
                entry.handler_pc >= code_length ||
                (entry.variable_slot != UINT8_MAX &&
                 entry.variable_slot >= proto->n_locals)) goto failed_module;
            if (class_sid != UINT16_MAX) {
                pvalue class_name;
                uint16_t class_constant;
                if (class_sid >= n_strings) goto failed_module;
                class_name = pv_heap(PT_STRING, &strings[class_sid]->header);
                if (!pproto_add_constant(proto, class_name, &class_constant)) {
                    result = PPHP_E_NOMEM;
                    goto failed_module;
                }
                entry.class_constant = class_constant;
            }
            if (!pproto_add_catch(proto, entry)) goto failed_module;
            offset += 10U;
        }
    }
    result = PPHP_OK;
    goto done;
failed_module:
    pmodule_destroy(module);
done:
    for (i = 0U; i < n_strings; i++) {
        if (strings[i] != NULL) {
            pv_release(pv_heap(PT_STRING, &strings[i]->header));
        }
    }
    pphp_free(strings);
    return result;
}

const char *pphp_opcode_name(uint8_t opcode) {
    switch ((pphp_opcode)opcode) {
        case OP_NOP: return "NOP";
        case OP_HALT: return "HALT";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        case OP_SWAP: return "SWAP";
        case OP_LOAD_NULL: return "LOAD_NULL";
        case OP_LOAD_TRUE: return "LOAD_TRUE";
        case OP_LOAD_FALSE: return "LOAD_FALSE";
        case OP_LOAD_I8: return "LOAD_I8";
        case OP_LOAD_I32: return "LOAD_I32";
        case OP_LOAD_CONST: return "LOAD_CONST";
        case OP_LOAD_LOCAL: return "LOAD_LOCAL";
        case OP_STORE_LOCAL: return "STORE_LOCAL";
        case OP_LOAD_ARGC: return "LOAD_ARGC";
        case OP_BIND_GLOBAL: return "BIND_GLOBAL";
        case OP_STATIC_INIT: return "STATIC_INIT";
        case OP_LOAD_NAMED_CONST: return "LOAD_NAMED_CONST";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_POW: return "POW";
        case OP_NEG: return "NEG";
        case OP_CONCAT: return "CONCAT";
        case OP_BAND: return "BAND";
        case OP_BOR: return "BOR";
        case OP_BXOR: return "BXOR";
        case OP_BNOT: return "BNOT";
        case OP_SHL: return "SHL";
        case OP_SHR: return "SHR";
        case OP_EQ: return "EQ";
        case OP_NE: return "NE";
        case OP_IDENT: return "IDENT";
        case OP_NIDENT: return "NIDENT";
        case OP_LT: return "LT";
        case OP_LE: return "LE";
        case OP_GT: return "GT";
        case OP_GE: return "GE";
        case OP_CMP: return "CMP";
        case OP_NOT: return "NOT";
        case OP_CAST: return "CAST";
        case OP_INSTANCEOF_DYNAMIC: return "INSTANCEOF_DYNAMIC";
        case OP_JMP: return "JMP";
        case OP_JMP_IF: return "JMP_IF";
        case OP_JMP_UNLESS: return "JMP_UNLESS";
        case OP_JMP_IF_KEEP: return "JMP_IF_KEEP";
        case OP_JMP_UNLESS_KEEP: return "JMP_UNLESS_KEEP";
        case OP_JMP_NOTNULL_KEEP: return "JMP_NOTNULL_KEEP";
        case OP_JMP_IFNULL_KEEP: return "JMP_IFNULL_KEEP";
        case OP_CALL: return "CALL";
        case OP_CALL_VALUE: return "CALL_VALUE";
        case OP_RET: return "RET";
        case OP_RET_NULL: return "RET_NULL";
        case OP_ECHO: return "ECHO";
        case OP_CALL_ARRAY: return "CALL_ARRAY";
        case OP_CALL_VALUE_ARRAY: return "CALL_VALUE_ARRAY";
        case OP_INCLUDE: return "INCLUDE";
        case OP_NEW_ARRAY: return "NEW_ARRAY";
        case OP_ARR_PUSH: return "ARR_PUSH";
        case OP_ARR_SET: return "ARR_SET";
        case OP_IDX_GET: return "IDX_GET";
        case OP_IDX_SET: return "IDX_SET";
        case OP_IDX_APPEND: return "IDX_APPEND";
        case OP_FE_INIT: return "FE_INIT";
        case OP_FE_NEXT: return "FE_NEXT";
        case OP_FE_FREE: return "FE_FREE";
        case OP_ARR_EXTEND: return "ARR_EXTEND";
        case OP_ARR_SEPARATE: return "ARR_SEPARATE";
        case OP_IDX_UNSET: return "IDX_UNSET";
        case OP_IDX_GET_QUIET: return "IDX_GET_QUIET";
        case OP_NEW_OBJ: return "NEW_OBJ";
        case OP_PROP_GET: return "PROP_GET";
        case OP_PROP_SET: return "PROP_SET";
        case OP_MCALL: return "MCALL";
        case OP_MCALL_ARRAY: return "MCALL_ARRAY";
        case OP_NEW_OBJ_ARRAY: return "NEW_OBJ_ARRAY";
        case OP_SCALL: return "SCALL";
        case OP_SCALL_ARRAY: return "SCALL_ARRAY";
        case OP_INSTANCEOF: return "INSTANCEOF";
        case OP_SPROP_GET: return "SPROP_GET";
        case OP_SPROP_SET: return "SPROP_SET";
        case OP_CLSCONST: return "CLSCONST";
        case OP_PROP_GET_QUIET: return "PROP_GET_QUIET";
        case OP_NEW_OBJ_DYNAMIC: return "NEW_OBJ_DYNAMIC";
        case OP_NEW_OBJ_DYNAMIC_ARRAY: return "NEW_OBJ_DYNAMIC_ARRAY";
        case OP_CLOSURE: return "CLOSURE";
        case OP_THROW: return "THROW";
        case OP_CLONE: return "CLONE";
        case OP_DEF_FUNC: return "DEF_FUNC";
        case OP_DEF_CCONST: return "DEF_CCONST";
        case OP_DEF_CLASS: return "DEF_CLASS";
        case OP_DEF_METHOD: return "DEF_METHOD";
        case OP_DEF_PROP: return "DEF_PROP";
        case OP_DEF_CONST: return "DEF_CONST";
        case OP_DEF_END: return "DEF_END";
        case OP_DEF_INTERFACE: return "DEF_INTERFACE";
        case OP_LINE: return "LINE";
        default: return "UNKNOWN";
    }
}
