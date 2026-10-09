#pragma once

#include "lfs.h"
#include <stdint.h>

// set constants and configuration
extern void fs_init();
extern void fs_mount();
extern void fs_unmount();
extern void fs_reformat();

// list files in the root directory
extern char **fs_get_file_list(uint32_t *num_files);

// get free space in bytes
// (this works by counting unallocated blocks, so effectively rounds down)
extern uint32_t fs_get_free_space();

// additional littlefs glue
extern int fs_remove(const char *path);
extern int fs_rename(const char *oldpath, const char *newpath);
extern int fs_stat(const char *path, struct lfs_info *info);
extern int fs_file_open(lfs_file_t *file, const char *path, int flags);
extern int fs_file_close(lfs_file_t *file);
extern int fs_file_sync(lfs_file_t *file);
extern int32_t fs_file_read(lfs_file_t *file, void *buffer, uint32_t size);
extern int32_t fs_file_write(lfs_file_t *file, const void *buffer,
                             uint32_t size);
extern int32_t fs_file_seek(lfs_file_t *file, int32_t off, int whence);
extern int fs_file_truncate(lfs_file_t *file, uint32_t size);
extern int32_t fs_file_tell(lfs_file_t *file);
extern int fs_file_rewind(lfs_file_t *file);
extern int32_t fs_file_size(lfs_file_t *file);
extern int32_t fs_fs_size();

#if 0 // NYI / not used

extern int fs_mkdir(const char *path);
extern int fs_dir_open(lfs_dir_t *dir, const char *path);
extern int fs_dir_close(lfs_dir_t *dir);
extern int fs_dir_read(lfs_dir_t *dir, struct lfs_info *info);
extern int fs_dir_seek(lfs_dir_t *dir, uint32_t off);
extern int32_t fs_dir_tell(lfs_dir_t *dir);
extern int fs_dir_rewind(lfs_dir_t *dir);
extern int fs_fs_stat(struct lfs_fsinfo *fsinfo);
#endif
