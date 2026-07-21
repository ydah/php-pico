#ifndef PPHP_FS_H
#define PPHP_FS_H

#include <stddef.h>
#include <stdint.h>

typedef struct pphp_file pphp_file;
typedef struct pphp_dir pphp_dir;

enum {
    PPHP_FS_SEEK_SET = 0,
    PPHP_FS_SEEK_CUR = 1,
    PPHP_FS_SEEK_END = 2
};

int pphp_fs_mount(void);
int pphp_fs_unmount(void);
int pphp_fs_format(void);

pphp_file *pphp_fs_open(const char *path, const char *mode);
int pphp_fs_close(pphp_file *file);
int64_t pphp_fs_read(pphp_file *file, void *buffer, size_t length);
int64_t pphp_fs_write(pphp_file *file, const void *buffer, size_t length);
int64_t pphp_fs_seek(pphp_file *file, int64_t offset, int whence);
int64_t pphp_fs_tell(pphp_file *file);
int pphp_fs_eof(pphp_file *file);

int pphp_fs_exists(const char *path);
int pphp_fs_is_dir(const char *path);
int64_t pphp_fs_size(const char *path);
int pphp_fs_remove(const char *path);
int pphp_fs_mkdir(const char *path, unsigned mode);
int pphp_fs_rmdir(const char *path);
int pphp_fs_rename(const char *old_path, const char *new_path);

pphp_dir *pphp_fs_dir_open(const char *path);
int pphp_fs_dir_read(pphp_dir *directory, char *name, size_t capacity,
                     int *is_directory);
int pphp_fs_dir_close(pphp_dir *directory);

#endif
