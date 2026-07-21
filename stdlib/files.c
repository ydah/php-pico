#define _POSIX_C_SOURCE 200809L

#include "files.h"

#include "parray.h"
#include "pphp/fs.h"
#include "resource.h"
#include "value_ops.h"

#include <string.h>

typedef struct pfile_resource {
    presource resource;
    pphp_file *file;
} pfile_resource;

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int invalid_arguments(pphp_state *state, const pstring *name) {
    pphp_runtime_error(state, 0U, "%.*s() received invalid arguments",
                       (int)name->length, name->data);
    return -1;
}

static const char *path_argument(const pvalue *arguments, size_t count,
                                 size_t index) {
    if (index >= count || arguments[index].type != PT_STRING ||
        arguments[index].as.gc == NULL) return NULL;
    return ((const pstring *)arguments[index].as.gc)->data;
}

static pfile_resource *file_argument(pvalue value) {
    presource *resource;
    if (value.type != PT_RESOURCE || value.as.gc == NULL) return NULL;
    resource = (presource *)value.as.gc;
    return resource->kind == PRESOURCE_FILE ? (pfile_resource *)resource : NULL;
}

static void file_destroy(presource *resource) {
    pfile_resource *file = (pfile_resource *)resource;
    if (file->file != NULL) (void)pphp_fs_close(file->file);
    pphp_free(file);
}

int pphp_file_read_all(const char *path, char **bytes, size_t *length) {
    pphp_file *file;
    int64_t measured;
    int64_t read_count;
    char *buffer;
    if (path == NULL || bytes == NULL || length == NULL) return 0;
    *bytes = NULL;
    *length = 0U;
    file = pphp_fs_open(path, "rb");
    if (file == NULL) return 0;
    measured = pphp_fs_seek(file, 0, PPHP_FS_SEEK_END);
    if (measured < 0 || (uint64_t)measured > (uint64_t)PPHP_STR_MAX ||
        pphp_fs_seek(file, 0, PPHP_FS_SEEK_SET) < 0) {
        (void)pphp_fs_close(file);
        return 0;
    }
    buffer = pphp_alloc((size_t)measured + 1U);
    if (buffer == NULL) {
        (void)pphp_fs_close(file);
        return 0;
    }
    read_count = pphp_fs_read(file, buffer, (size_t)measured);
    if (read_count != measured) {
        pphp_free(buffer);
        (void)pphp_fs_close(file);
        return 0;
    }
    buffer[(size_t)read_count] = '\0';
    (void)pphp_fs_close(file);
    *bytes = buffer;
    *length = (size_t)read_count;
    return 1;
}

static int call_whole_file(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    const char *path = path_argument(arguments, count, 0U);
    if (name_is(name, "file_get_contents")) {
        char *bytes;
        size_t length;
        pstring *string;
        if (count != 1U || path == NULL) return invalid_arguments(state, name);
        if (!pphp_file_read_all(path, &bytes, &length)) {
            *result = pv_bool(0);
            return 1;
        }
        string = ps_new(bytes, length);
        pphp_free(bytes);
        if (string == NULL) {
            pphp_runtime_error(state, 0U,
                               "out of memory reading file %s", path);
            return -1;
        }
        *result = pv_heap(PT_STRING, &string->header);
        return 1;
    }
    if (name_is(name, "file_put_contents")) {
        pstring *data;
        pphp_file *file;
        int append = 0;
        int64_t written;
        if (count < 2U || count > 3U || path == NULL) {
            return invalid_arguments(state, name);
        }
        data = pv_to_string(arguments[1]);
        if (data == NULL) return invalid_arguments(state, name);
        if (count == 3U) {
            if (arguments[2].type != PT_INT) {
                ps_destroy(data);
                return invalid_arguments(state, name);
            }
            append = (arguments[2].as.i & 8) != 0;
        }
        file = pphp_fs_open(path, append ? "ab" : "wb");
        if (file == NULL) {
            ps_destroy(data);
            *result = pv_bool(0);
            return 1;
        }
        written = pphp_fs_write(file, data->data, data->length);
        if (!pphp_fs_close(file) || written != (int64_t)data->length) {
            ps_destroy(data);
            *result = pv_bool(0);
            return 1;
        }
        ps_destroy(data);
        *result = pv_int((pphp_int)written);
        return 1;
    }
    return 0;
}

static int call_path_operation(pphp_state *state, const pstring *name,
                               const pvalue *arguments, size_t count,
                               pvalue *result) {
    const char *first = path_argument(arguments, count, 0U);
    if (name_is(name, "file_exists")) {
        if (count != 1U || first == NULL) return invalid_arguments(state, name);
        *result = pv_bool(pphp_fs_exists(first));
        return 1;
    }
    if (name_is(name, "unlink") || name_is(name, "rmdir")) {
        int status;
        if (count != 1U || first == NULL) return invalid_arguments(state, name);
        status = name_is(name, "unlink") ? pphp_fs_remove(first)
                                          : pphp_fs_rmdir(first);
        *result = pv_bool(status);
        return 1;
    }
    if (name_is(name, "mkdir")) {
        pphp_int mode = 0777;
        if (count < 1U || count > 2U || first == NULL ||
            (count == 2U && arguments[1].type != PT_INT)) {
            return invalid_arguments(state, name);
        }
        if (count == 2U) mode = arguments[1].as.i;
        *result = pv_bool(pphp_fs_mkdir(first, (unsigned)(mode & 0777)));
        return 1;
    }
    if (name_is(name, "rename")) {
        const char *second = path_argument(arguments, count, 1U);
        if (count != 2U || first == NULL || second == NULL) {
            return invalid_arguments(state, name);
        }
        *result = pv_bool(pphp_fs_rename(first, second));
        return 1;
    }
    if (name_is(name, "filesize")) {
        int64_t size;
        if (count != 1U || first == NULL) return invalid_arguments(state, name);
        size = pphp_fs_size(first);
        if (size < 0) {
            *result = pv_bool(0);
        } else {
#if PPHP_INT64
            *result = pv_int((pphp_int)size);
#else
            *result = (uint64_t)size > (uint64_t)INT32_MAX
                          ? pv_float((pphp_float)size)
                          : pv_int((pphp_int)size);
#endif
        }
        return 1;
    }
    return 0;
}

static int call_stream(pphp_state *state, const pstring *name,
                       const pvalue *arguments, size_t count, pvalue *result) {
    if (name_is(name, "fopen")) {
        const char *path = path_argument(arguments, count, 0U);
        const char *mode = path_argument(arguments, count, 1U);
        pfile_resource *resource;
        pphp_file *file;
        if (count != 2U || path == NULL || mode == NULL) {
            return invalid_arguments(state, name);
        }
        file = pphp_fs_open(path, mode);
        if (file == NULL) {
            *result = pv_bool(0);
            return 1;
        }
        resource = pphp_alloc(sizeof(*resource));
        if (resource == NULL) {
            (void)pphp_fs_close(file);
            pphp_runtime_error(state, 0U, "out of memory opening file");
            return -1;
        }
        resource->resource.header.refcnt = 1U;
        resource->resource.header.type = PT_RESOURCE;
        resource->resource.header.flags = 0U;
        resource->resource.destroy = file_destroy;
        resource->resource.kind = PRESOURCE_FILE;
        resource->file = file;
        *result = pv_heap(PT_RESOURCE, &resource->resource.header);
        return 1;
    }
    if (count == 0U) return invalid_arguments(state, name);
    {
        pfile_resource *resource = file_argument(arguments[0]);
        if (resource == NULL || resource->file == NULL) {
            return invalid_arguments(state, name);
        }
        if (name_is(name, "fclose")) {
            int status;
            if (count != 1U) return invalid_arguments(state, name);
            status = pphp_fs_close(resource->file);
            resource->file = NULL;
            *result = pv_bool(status);
            return 1;
        }
        if (name_is(name, "fread")) {
            pphp_int requested;
            char *buffer;
            int64_t got;
            pstring *string;
            if (count != 2U || arguments[1].type != PT_INT ||
                arguments[1].as.i < 0) return invalid_arguments(state, name);
            requested = arguments[1].as.i;
            if ((uint64_t)requested > (uint64_t)PPHP_STR_MAX) {
                return invalid_arguments(state, name);
            }
            buffer = pphp_alloc((size_t)requested + 1U);
            if (buffer == NULL) {
                pphp_runtime_error(state, 0U, "out of memory reading stream");
                return -1;
            }
            got = pphp_fs_read(resource->file, buffer, (size_t)requested);
            if (got < 0) {
                pphp_free(buffer);
                *result = pv_bool(0);
                return 1;
            }
            string = ps_new(buffer, (size_t)got);
            pphp_free(buffer);
            if (string == NULL) {
                pphp_runtime_error(state, 0U, "out of memory returning stream data");
                return -1;
            }
            *result = pv_heap(PT_STRING, &string->header);
            return 1;
        }
        if (name_is(name, "fwrite")) {
            pstring *data;
            int64_t written;
            if (count != 2U) return invalid_arguments(state, name);
            data = pv_to_string(arguments[1]);
            if (data == NULL) return invalid_arguments(state, name);
            written = pphp_fs_write(resource->file, data->data, data->length);
            ps_destroy(data);
            *result = written < 0 ? pv_bool(0) : pv_int((pphp_int)written);
            return 1;
        }
        if (name_is(name, "fgets")) {
            pphp_int limit = 256;
            char *buffer;
            pstring *string;
            if (count > 2U ||
                (count == 2U && (arguments[1].type != PT_INT ||
                                 arguments[1].as.i <= 0))) {
                return invalid_arguments(state, name);
            }
            if (count == 2U) limit = arguments[1].as.i;
            if ((uint64_t)limit > (uint64_t)PPHP_STR_MAX) {
                limit = (pphp_int)PPHP_STR_MAX;
            }
            buffer = pphp_alloc((size_t)limit + 1U);
            if (buffer == NULL) {
                pphp_runtime_error(state, 0U, "out of memory reading line");
                return -1;
            }
            {
                size_t used = 0U;
                while (used < (size_t)limit) {
                    int64_t got = pphp_fs_read(resource->file,
                                               buffer + used, 1U);
                    if (got <= 0) break;
                    if (buffer[used++] == '\n') break;
                }
                if (used == 0U) {
                    pphp_free(buffer);
                    *result = pv_bool(0);
                    return 1;
                }
                buffer[used] = '\0';
            }
            string = ps_new(buffer, strlen(buffer));
            pphp_free(buffer);
            if (string == NULL) {
                pphp_runtime_error(state, 0U, "out of memory returning line");
                return -1;
            }
            *result = pv_heap(PT_STRING, &string->header);
            return 1;
        }
        if (name_is(name, "feof")) {
            if (count != 1U) return invalid_arguments(state, name);
            *result = pv_bool(pphp_fs_eof(resource->file));
            return 1;
        }
        if (name_is(name, "fseek")) {
            int whence = PPHP_FS_SEEK_SET;
            int64_t position;
            if (count < 2U || count > 3U || arguments[1].type != PT_INT ||
                (count == 3U && arguments[2].type != PT_INT)) {
                return invalid_arguments(state, name);
            }
            if (count == 3U) whence = (int)arguments[2].as.i;
            position = pphp_fs_seek(resource->file, arguments[1].as.i,
                                    whence);
            *result = pv_int(position < 0 ? -1 : 0);
            return 1;
        }
        if (name_is(name, "ftell")) {
            int64_t position;
            if (count != 1U) return invalid_arguments(state, name);
            position = pphp_fs_tell(resource->file);
            *result = position < 0 ? pv_bool(0)
                                    : pv_int((pphp_int)position);
            return 1;
        }
    }
    return 0;
}

static int call_scandir(pphp_state *state, const pstring *name,
                        const pvalue *arguments, size_t count,
                        pvalue *result) {
    const char *path = path_argument(arguments, count, 0U);
    pphp_dir *directory;
    char entry[256];
    parray *values;
    int scan_result;
    if (count != 1U || path == NULL) return invalid_arguments(state, name);
    directory = pphp_fs_dir_open(path);
    if (directory == NULL) {
        *result = pv_bool(0);
        return 1;
    }
    values = pa_new(8U);
    if (values == NULL) {
        (void)pphp_fs_dir_close(directory);
        pphp_runtime_error(state, 0U, "out of memory scanning directory");
        return -1;
    }
    while ((scan_result = pphp_fs_dir_read(directory, entry, sizeof(entry),
                                           NULL)) > 0) {
        pstring *item = ps_new(entry, strlen(entry));
        pvalue value;
        if (item == NULL) goto scan_oom;
        value = pv_heap(PT_STRING, &item->header);
        if (!pa_push(values, value)) {
            pv_release(value);
            goto scan_oom;
        }
        pv_release(value);
    }
    if (scan_result < 0) goto scan_oom;
    {
        size_t i;
        for (i = 1U; i < values->used; i++) {
            pvalue current = values->entries[i].value;
            size_t insert = i;
            while (insert > 0U &&
                   strcmp(((pstring *)values->entries[insert - 1U].value.as.gc)->data,
                          ((pstring *)current.as.gc)->data) > 0) {
                values->entries[insert].value =
                    values->entries[insert - 1U].value;
                insert--;
            }
            values->entries[insert].value = current;
        }
    }
    (void)pphp_fs_dir_close(directory);
    *result = pv_heap(PT_ARRAY, &values->header);
    return 1;
scan_oom:
    (void)pphp_fs_dir_close(directory);
    pv_release(pv_heap(PT_ARRAY, &values->header));
    pphp_runtime_error(state, 0U, "out of memory scanning directory");
    return -1;
}

int pphp_file_builtin_exists(const pstring *name) {
    static const char names[] =
        "fclose\0feof\0fgets\0file_exists\0file_get_contents\0"
        "file_put_contents\0filesize\0fopen\0fread\0fseek\0ftell\0fwrite\0"
        "mkdir\0rename\0rmdir\0scandir\0unlink\0";
    const char *candidate = names;
    while (*candidate != '\0') {
        if (name_is(name, candidate)) return 1;
        candidate += strlen(candidate) + 1U;
    }
    return 0;
}

int pphp_call_file_builtin(pphp_state *state, const pstring *name,
                           const pvalue *arguments, size_t count,
                           pvalue *result) {
    int handled;
    if (!pphp_file_builtin_exists(name)) return 0;
    handled = call_whole_file(state, name, arguments, count, result);
    if (handled != 0) return handled;
    handled = call_path_operation(state, name, arguments, count, result);
    if (handled != 0) return handled;
    if (name_is(name, "scandir")) {
        return call_scandir(state, name, arguments, count, result);
    }
    handled = call_stream(state, name, arguments, count, result);
    if (handled != 0) return handled;
    return 0;
}
