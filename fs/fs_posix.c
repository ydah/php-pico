#define _POSIX_C_SOURCE 200809L

#include "pphp/fs.h"

#include "pphp/pphp.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct pphp_file {
    FILE *handle;
};

struct pphp_dir {
    DIR *handle;
};

int pphp_fs_mount(void) { return 1; }
int pphp_fs_unmount(void) { return 1; }
int pphp_fs_format(void) { return 0; }

pphp_file *pphp_fs_open(const char *path, const char *mode) {
    pphp_file *file;
    FILE *handle;
    if (path == NULL || mode == NULL) return NULL;
    handle = fopen(path, mode);
    if (handle == NULL) return NULL;
    file = pphp_alloc(sizeof(*file));
    if (file == NULL) {
        (void)fclose(handle);
        return NULL;
    }
    file->handle = handle;
    return file;
}

int pphp_fs_close(pphp_file *file) {
    int result;
    if (file == NULL) return 0;
    result = fclose(file->handle) == 0;
    pphp_free(file);
    return result;
}

int64_t pphp_fs_read(pphp_file *file, void *buffer, size_t length) {
    size_t count;
    if (file == NULL || (buffer == NULL && length != 0U)) return -1;
    count = fread(buffer, 1U, length, file->handle);
    if (count == 0U && ferror(file->handle)) return -1;
    return (int64_t)count;
}

int64_t pphp_fs_write(pphp_file *file, const void *buffer, size_t length) {
    size_t count;
    if (file == NULL || (buffer == NULL && length != 0U)) return -1;
    count = fwrite(buffer, 1U, length, file->handle);
    return count == 0U && length != 0U && ferror(file->handle)
               ? -1 : (int64_t)count;
}

int64_t pphp_fs_seek(pphp_file *file, int64_t offset, int whence) {
    int system_whence;
    long position;
    if (file == NULL || offset < (int64_t)LONG_MIN ||
        offset > (int64_t)LONG_MAX) return -1;
    if (whence == PPHP_FS_SEEK_SET) system_whence = SEEK_SET;
    else if (whence == PPHP_FS_SEEK_CUR) system_whence = SEEK_CUR;
    else if (whence == PPHP_FS_SEEK_END) system_whence = SEEK_END;
    else return -1;
    if (fseek(file->handle, (long)offset, system_whence) != 0) return -1;
    position = ftell(file->handle);
    return position < 0L ? -1 : (int64_t)position;
}

int64_t pphp_fs_tell(pphp_file *file) {
    long position;
    if (file == NULL) return -1;
    position = ftell(file->handle);
    return position < 0L ? -1 : (int64_t)position;
}

int pphp_fs_eof(pphp_file *file) {
    return file != NULL && feof(file->handle);
}

int pphp_fs_exists(const char *path) {
    struct stat info;
    return path != NULL && stat(path, &info) == 0;
}

int pphp_fs_is_dir(const char *path) {
    struct stat info;
    return path != NULL && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

int64_t pphp_fs_size(const char *path) {
    struct stat info;
    if (path == NULL || stat(path, &info) != 0 || info.st_size < 0) return -1;
    return (int64_t)info.st_size;
}

int pphp_fs_remove(const char *path) {
    return path != NULL && unlink(path) == 0;
}

int pphp_fs_mkdir(const char *path, unsigned mode) {
    return path != NULL && mkdir(path, (mode_t)(mode & 0777U)) == 0;
}

int pphp_fs_rmdir(const char *path) {
    return path != NULL && rmdir(path) == 0;
}

int pphp_fs_rename(const char *old_path, const char *new_path) {
    return old_path != NULL && new_path != NULL &&
           rename(old_path, new_path) == 0;
}

int pphp_fs_canonicalize(const char *path, char *resolved, size_t capacity) {
    char absolute[PATH_MAX];
    size_t length;
    if (path == NULL || resolved == NULL || capacity == 0U ||
        realpath(path, absolute) == NULL) return 0;
    length = strlen(absolute);
    if (length + 1U > capacity) return 0;
    memcpy(resolved, absolute, length + 1U);
    return 1;
}

pphp_dir *pphp_fs_dir_open(const char *path) {
    pphp_dir *directory;
    DIR *handle;
    if (path == NULL) return NULL;
    handle = opendir(path);
    if (handle == NULL) return NULL;
    directory = pphp_alloc(sizeof(*directory));
    if (directory == NULL) {
        (void)closedir(handle);
        return NULL;
    }
    directory->handle = handle;
    return directory;
}

int pphp_fs_dir_read(pphp_dir *directory, char *name, size_t capacity,
                     int *is_directory) {
    struct dirent *entry;
    size_t length;
    if (directory == NULL || name == NULL || capacity == 0U) return -1;
    entry = readdir(directory->handle);
    if (entry == NULL) return 0;
    length = strlen(entry->d_name);
    if (length + 1U > capacity) return -1;
    memcpy(name, entry->d_name, length + 1U);
    if (is_directory != NULL) {
#ifdef DT_DIR
        *is_directory = entry->d_type == DT_DIR;
#else
        *is_directory = 0;
#endif
    }
    return 1;
}

int pphp_fs_dir_close(pphp_dir *directory) {
    int result;
    if (directory == NULL) return 0;
    result = closedir(directory->handle) == 0;
    pphp_free(directory);
    return result;
}
