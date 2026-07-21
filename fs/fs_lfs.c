#include "pphp/fs.h"

#include "pphp/hal.h"
#include "pphp/pphp.h"
#include "lfs.h"

#include <limits.h>
#include <string.h>

#define PPHP_LFS_BLOCK_SIZE 4096U
#define PPHP_LFS_CACHE_SIZE 256U
#define PPHP_LFS_LOOKAHEAD_SIZE 32U

struct pphp_file {
    lfs_file_t handle;
    struct lfs_file_config config;
    uint8_t cache[PPHP_LFS_CACHE_SIZE];
    int eof;
};

struct pphp_dir {
    lfs_dir_t handle;
};

static lfs_t filesystem;
static uint8_t read_cache[PPHP_LFS_CACHE_SIZE];
static uint8_t program_cache[PPHP_LFS_CACHE_SIZE];
static uint8_t lookahead[PPHP_LFS_LOOKAHEAD_SIZE];
static int mounted;

static int device_read(const struct lfs_config *config, lfs_block_t block,
                       lfs_off_t offset, void *buffer, lfs_size_t size) {
    uint32_t address;
    (void)config;
    if (block > UINT32_MAX / PPHP_LFS_BLOCK_SIZE) return LFS_ERR_INVAL;
    address = block * PPHP_LFS_BLOCK_SIZE + offset;
    return hal_flash_read(address, buffer, size) == PPHP_HAL_OK
               ? LFS_ERR_OK : LFS_ERR_IO;
}

static int device_program(const struct lfs_config *config, lfs_block_t block,
                          lfs_off_t offset, const void *buffer,
                          lfs_size_t size) {
    uint32_t address;
    (void)config;
    if (block > UINT32_MAX / PPHP_LFS_BLOCK_SIZE) return LFS_ERR_INVAL;
    address = block * PPHP_LFS_BLOCK_SIZE + offset;
    return hal_flash_prog(address, buffer, size) == PPHP_HAL_OK
               ? LFS_ERR_OK : LFS_ERR_IO;
}

static int device_erase(const struct lfs_config *config, lfs_block_t block) {
    (void)config;
    return hal_flash_erase(block) == PPHP_HAL_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

static int device_sync(const struct lfs_config *config) {
    (void)config;
    return LFS_ERR_OK;
}

static const struct lfs_config filesystem_config = {
    .read = device_read,
    .prog = device_program,
    .erase = device_erase,
    .sync = device_sync,
    .read_size = 1U,
    .prog_size = PPHP_LFS_CACHE_SIZE,
    .block_size = PPHP_LFS_BLOCK_SIZE,
    .block_count = PPHP_FLASH_FS_SIZE / PPHP_LFS_BLOCK_SIZE,
    .block_cycles = 500,
    .cache_size = PPHP_LFS_CACHE_SIZE,
    .lookahead_size = PPHP_LFS_LOOKAHEAD_SIZE,
    .read_buffer = read_cache,
    .prog_buffer = program_cache,
    .lookahead_buffer = lookahead
};

static const char *lfs_path(const char *path) {
    if (path == NULL || *path == '\0') return NULL;
    if (strcmp(path, "/home") == 0 || strcmp(path, "/home/") == 0) {
        return "/";
    }
    if (strncmp(path, "/home/", 6U) == 0) return path + 5U;
    if (strncmp(path, "/lib", 4U) == 0 &&
        (path[4] == '\0' || path[4] == '/')) return NULL;
    return path;
}

static int ensure_mounted(void) {
    return mounted || pphp_fs_mount();
}

int pphp_fs_mount(void) {
    if (mounted) return 1;
    if (lfs_mount(&filesystem, &filesystem_config) != LFS_ERR_OK) {
        if (lfs_format(&filesystem, &filesystem_config) != LFS_ERR_OK ||
            lfs_mount(&filesystem, &filesystem_config) != LFS_ERR_OK) {
            return 0;
        }
    }
    mounted = 1;
    return 1;
}

int pphp_fs_unmount(void) {
    if (!mounted) return 1;
    if (lfs_unmount(&filesystem) != LFS_ERR_OK) return 0;
    mounted = 0;
    return 1;
}

int pphp_fs_format(void) {
    int was_mounted = mounted;
    if (mounted && !pphp_fs_unmount()) return 0;
    if (lfs_format(&filesystem, &filesystem_config) != LFS_ERR_OK) return 0;
    return was_mounted ? pphp_fs_mount() : 1;
}

static int open_flags(const char *mode) {
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) return LFS_O_RDONLY;
    if (strcmp(mode, "r+") == 0 || strcmp(mode, "r+b") == 0 ||
        strcmp(mode, "rb+") == 0) return LFS_O_RDWR;
    if (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) {
        return LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC;
    }
    if (strcmp(mode, "w+") == 0 || strcmp(mode, "w+b") == 0 ||
        strcmp(mode, "wb+") == 0) {
        return LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC;
    }
    if (strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0) {
        return LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND;
    }
    if (strcmp(mode, "a+") == 0 || strcmp(mode, "a+b") == 0 ||
        strcmp(mode, "ab+") == 0) {
        return LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND;
    }
    return -1;
}

pphp_file *pphp_fs_open(const char *path, const char *mode) {
    const char *normalized = lfs_path(path);
    pphp_file *file;
    int flags = mode == NULL ? -1 : open_flags(mode);
    if (normalized == NULL || flags < 0 || !ensure_mounted()) return NULL;
    file = pphp_alloc(sizeof(*file));
    if (file == NULL) return NULL;
    memset(file, 0, sizeof(*file));
    file->config.buffer = file->cache;
    if (lfs_file_opencfg(&filesystem, &file->handle, normalized, flags,
                         &file->config) != LFS_ERR_OK) {
        pphp_free(file);
        return NULL;
    }
    return file;
}

int pphp_fs_close(pphp_file *file) {
    int result;
    if (file == NULL) return 0;
    result = lfs_file_close(&filesystem, &file->handle) == LFS_ERR_OK;
    pphp_free(file);
    return result;
}

int64_t pphp_fs_read(pphp_file *file, void *buffer, size_t length) {
    lfs_ssize_t count;
    if (file == NULL || (buffer == NULL && length != 0U) ||
        length > (size_t)UINT32_MAX) return -1;
    count = lfs_file_read(&filesystem, &file->handle, buffer,
                          (lfs_size_t)length);
    if (count >= 0) file->eof = count == 0;
    return count;
}

int64_t pphp_fs_write(pphp_file *file, const void *buffer, size_t length) {
    if (file == NULL || (buffer == NULL && length != 0U) ||
        length > (size_t)UINT32_MAX) return -1;
    return lfs_file_write(&filesystem, &file->handle, buffer,
                          (lfs_size_t)length);
}

int64_t pphp_fs_seek(pphp_file *file, int64_t offset, int whence) {
    lfs_soff_t position;
    if (file == NULL || offset < INT32_MIN || offset > INT32_MAX ||
        whence < PPHP_FS_SEEK_SET || whence > PPHP_FS_SEEK_END) return -1;
    position = lfs_file_seek(&filesystem, &file->handle, (lfs_soff_t)offset,
                             whence);
    if (position >= 0) file->eof = 0;
    return position;
}

int64_t pphp_fs_tell(pphp_file *file) {
    return file == NULL ? -1 : lfs_file_tell(&filesystem, &file->handle);
}

int pphp_fs_eof(pphp_file *file) { return file != NULL && file->eof; }

static int stat_path(const char *path, struct lfs_info *info) {
    const char *normalized = lfs_path(path);
    return normalized != NULL && ensure_mounted() &&
           lfs_stat(&filesystem, normalized, info) == LFS_ERR_OK;
}

int pphp_fs_exists(const char *path) {
    struct lfs_info info;
    return stat_path(path, &info);
}

int pphp_fs_is_dir(const char *path) {
    struct lfs_info info;
    return stat_path(path, &info) && info.type == LFS_TYPE_DIR;
}

int64_t pphp_fs_size(const char *path) {
    struct lfs_info info;
    return stat_path(path, &info) && info.type == LFS_TYPE_REG
               ? (int64_t)info.size : -1;
}

int pphp_fs_remove(const char *path) {
    struct lfs_info info;
    const char *normalized = lfs_path(path);
    return normalized != NULL && stat_path(path, &info) &&
           info.type == LFS_TYPE_REG &&
           lfs_remove(&filesystem, normalized) == LFS_ERR_OK;
}

int pphp_fs_mkdir(const char *path, unsigned mode) {
    const char *normalized = lfs_path(path);
    (void)mode;
    return normalized != NULL && ensure_mounted() &&
           lfs_mkdir(&filesystem, normalized) == LFS_ERR_OK;
}

int pphp_fs_rmdir(const char *path) {
    struct lfs_info info;
    const char *normalized = lfs_path(path);
    return normalized != NULL && stat_path(path, &info) &&
           info.type == LFS_TYPE_DIR && strcmp(normalized, "/") != 0 &&
           lfs_remove(&filesystem, normalized) == LFS_ERR_OK;
}

int pphp_fs_rename(const char *old_path, const char *new_path) {
    const char *old_normalized = lfs_path(old_path);
    const char *new_normalized = lfs_path(new_path);
    return old_normalized != NULL && new_normalized != NULL &&
           ensure_mounted() &&
           lfs_rename(&filesystem, old_normalized, new_normalized) == LFS_ERR_OK;
}

pphp_dir *pphp_fs_dir_open(const char *path) {
    const char *normalized = lfs_path(path);
    pphp_dir *directory;
    if (normalized == NULL || !ensure_mounted()) return NULL;
    directory = pphp_alloc(sizeof(*directory));
    if (directory == NULL) return NULL;
    if (lfs_dir_open(&filesystem, &directory->handle, normalized) != LFS_ERR_OK) {
        pphp_free(directory);
        return NULL;
    }
    return directory;
}

int pphp_fs_dir_read(pphp_dir *directory, char *name, size_t capacity,
                     int *is_directory) {
    struct lfs_info info;
    size_t length;
    int result;
    if (directory == NULL || name == NULL || capacity == 0U) return -1;
    result = lfs_dir_read(&filesystem, &directory->handle, &info);
    if (result <= 0) return result;
    length = strlen(info.name);
    if (length + 1U > capacity) return -1;
    memcpy(name, info.name, length + 1U);
    if (is_directory != NULL) *is_directory = info.type == LFS_TYPE_DIR;
    return 1;
}

int pphp_fs_dir_close(pphp_dir *directory) {
    int result;
    if (directory == NULL) return 0;
    result = lfs_dir_close(&filesystem, &directory->handle) == LFS_ERR_OK;
    pphp_free(directory);
    return result;
}
