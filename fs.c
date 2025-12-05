
#include "hardware/flash.h"
#include "pico/multicore.h"

#include "flash.h"
#include "fs.h"

// debug
#include "serial.h"

static uint32_t flash_fs_start;
static uint32_t flash_fs_offset;

lfs_t lfs;
struct lfs_config cfg;

//--------------------------------------------------------------------
// --- user-defined flash operations for LFS

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// note that read operation takes a direct address, write/erase take offsets
// from XIP_BASE

int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
             void *buffer, lfs_size_t size) {
  memcpy(buffer, (void *)(flash_fs_start + block * c->block_size + off), size);
  return LFS_ERR_OK;
}

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
             const void *buffer, lfs_size_t size) {
  multicore_lockout_start_blocking();
  uint32_t ints = save_and_disable_interrupts();
  flash_range_program(flash_fs_offset + block * c->block_size + off, buffer,
                      size);
  restore_interrupts(ints);
  multicore_lockout_end_blocking();
  return LFS_ERR_OK;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes are
// propagated to the user. May return LFS_ERR_CORRUPT if the block should be
// considered bad.
int lfs_erase(const struct lfs_config *c, lfs_block_t block) {
  multicore_lockout_start_blocking();
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_fs_offset + block * c->block_size, c->block_size);
  restore_interrupts(ints);
  multicore_lockout_end_blocking();
  return LFS_ERR_OK;
}

// Sync the state of the underlying block device. Negative error codes are
// propagated to the user.
int lfs_sync(const struct lfs_config *c) {
  // N/A - no backing store
  (void)c;
  return LFS_ERR_OK;
}

void fs_init() {
  flash_fs_offset = flash_get_fs_offset();
  flash_fs_start = flash_get_fs_start();
  cfg.block_count = flash_get_fs_num_blocks();
  cfg.block_size = FLASH_SECTOR_SIZE;

  // block device operations
  cfg.read = lfs_read, cfg.prog = lfs_prog, cfg.erase = lfs_erase,
  cfg.sync = lfs_sync,

  // most of these values taken from arduino library or pico-filesystem
      cfg.read_size = 1, cfg.prog_size = FLASH_PAGE_SIZE,
  cfg.cache_size = FLASH_PAGE_SIZE, cfg.block_cycles = 256,
  cfg.lookahead_size = 32;

  cfg.read_buffer = NULL;
  cfg.prog_buffer = NULL;
  cfg.lookahead_buffer = NULL;

  cfg.name_max = 0;
  cfg.file_max = 0;
  cfg.attr_max = 0;
}

void fs_mount() {
  int err = lfs_mount(&lfs, &cfg);

  // reformat if we can't mount the filesystem
  // this should only happen on the first boot
  if (err) {
    lfs_format(&lfs, &cfg);
    lfs_mount(&lfs, &cfg);
  }

  lfs_fs_gc(&lfs);
}

void fs_unmount() { lfs_unmount(&lfs); }

char **fs_get_file_list(uint32_t *num_files) {
  char **file_list = NULL;
  uint32_t num_files_found = 0;
  struct lfs_info info;
  fs_stat("/", &info);
  if (info.type == LFS_TYPE_DIR) {
    lfs_dir_t dir;
    lfs_dir_open(&lfs, &dir, "/");
    while (true) {
      const int res = lfs_dir_read(&lfs, &dir, &info);
      if (res != 1) {
        if (res != 0) {
          serial("dir read error: %d\r\n", res);
        }
        break;
      }

      if (info.type == LFS_TYPE_REG) {
        if (num_files_found == 0) {
          file_list = malloc(sizeof(char *));
        } else {
          char **tmp =
              realloc(file_list, sizeof(char *) * (num_files_found + 1));
          if (tmp == NULL) {
            // realloc has failed; we are out of memory or something
            // i think best we can do is return what we have and avoid leaking
            break;
          }
          file_list = tmp;
        }

        file_list[num_files_found] = malloc(strlen(info.name) + 1);
        strcpy(file_list[num_files_found], info.name);
        num_files_found += 1;
      }
    }
    lfs_dir_close(&lfs, &dir);
  }
  *num_files = num_files_found;
  return file_list;
}

uint32_t fs_get_free_space() {
  int32_t blocks = fs_fs_size();
  if (blocks < 0) {
    // error!
    return 0;
  }
  uint32_t ublocks = (uint32_t)blocks;
  uint32_t free_blocks = cfg.block_count - ublocks;
  return free_blocks * cfg.block_size;
}

void fs_reformat() {
  lfs_unmount(&lfs);
  lfs_format(&lfs, &cfg);
  lfs_mount(&lfs, &cfg);
}

int fs_file_open(lfs_file_t *file, const char *path, int flags) {
  return lfs_file_open(&lfs, file, path, flags);
}

int fs_file_close(lfs_file_t *file) { return lfs_file_close(&lfs, file); }

int32_t fs_file_read(lfs_file_t *file, void *buffer, uint32_t size) {
  return lfs_file_read(&lfs, file, buffer, size);
}

int32_t fs_file_write(lfs_file_t *file, const void *buffer, uint32_t size) {
  return lfs_file_write(&lfs, file, buffer, size);
}

int fs_remove(const char *path) { return lfs_remove(&lfs, path); }

int fs_rename(const char *oldpath, const char *newpath) {
  return lfs_rename(&lfs, oldpath, newpath);
}

int fs_stat(const char *path, struct lfs_info *info) {
  return lfs_stat(&lfs, path, info);
}

int fs_file_sync(lfs_file_t *file) { return lfs_file_sync(&lfs, file); }

int32_t fs_file_seek(lfs_file_t *file, int32_t off, int whence) {
  return lfs_file_seek(&lfs, file, off, whence);
}

int fs_file_truncate(lfs_file_t *file, uint32_t size) {
  return lfs_file_truncate(&lfs, file, size);
}

int32_t fs_file_tell(lfs_file_t *file) { return lfs_file_tell(&lfs, file); }

int fs_file_rewind(lfs_file_t *file) { return lfs_file_rewind(&lfs, file); }

int32_t fs_file_size(lfs_file_t *file) { return lfs_file_size(&lfs, file); }

int32_t fs_fs_size() { return lfs_fs_size(&lfs); }

#if 0 // NYI / not used


int fs_mkdir(const char *path)
{
    return lfs_mkdir(&lfs, path);
}

int fs_dir_open(lfs_dir_t *dir, const char *path)
{
    return lfs_dir_open(&lfs, dir, path);
}

int fs_dir_close(lfs_dir_t *dir)
{
    return lfs_dir_close(&lfs, dir);
}

int fs_dir_read(lfs_dir_t *dir, struct lfs_info *info)
{
    return lfs_dir_read(&lfs, dir, info);
}

int fs_dir_seek(lfs_dir_t *dir, uint32_t off)
{
    return lfs_dir_seek(&lfs, dir, off);
}

int32_t fs_dir_tell(lfs_dir_t *dir)
{
    return lfs_dir_tell(&lfs, dir);
}

int fs_dir_rewind(lfs_dir_t *dir)
{
    return lfs_dir_rewind(&lfs, dir);
}

fs_fs_stat(struct lfs_fsinfo *fsinfo)
{
    return lfs_fs_stat(&lfs, fsinfo);
}

#endif
