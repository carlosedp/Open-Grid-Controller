#include <stdlib.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"

#include "flash.h"
#include "serial.h"

#define FLASH_OK 0x33

#define FLASH_INDEX_SIZE FLASH_SECTOR_SIZE
// FLASH_SECTOR_SIZE on rp2040 is 4096

static const uint32_t flash_start = XIP_BASE;
extern char __flash_binary_end;

static uint32_t flash_program_end_offset;
static uint32_t flash_status_offset;

static uint32_t flash_fs_offset;
static uint32_t flash_fs_start;
static uint32_t flash_fs_num_blocks;

uint32_t flash_get_fs_start() { return flash_fs_start; }
uint32_t flash_get_fs_offset() { return flash_fs_offset; }
uint32_t flash_get_fs_num_blocks() { return flash_fs_num_blocks; }
uint32_t flash_get_status_offset() { return flash_status_offset; }

void flash_init() {
  flash_program_end_offset = (uint32_t)&__flash_binary_end - flash_start;
  flash_status_offset = flash_program_end_offset + 1;

  // block alignment makes prog/erase faster
  if (flash_status_offset % FLASH_BLOCK_SIZE) {
    flash_status_offset +=
        FLASH_BLOCK_SIZE - (flash_status_offset % FLASH_BLOCK_SIZE);
  }

  flash_fs_offset = flash_status_offset + FLASH_BLOCK_SIZE;
  flash_fs_start = XIP_BASE + flash_fs_offset;

  uint32_t flash_fs_size = flash_fs_size =
      PICO_FLASH_SIZE_BYTES - (flash_fs_start - flash_start);
  // TEST: use smaller (1MB) fixed FS size
  // uint32_t flash_fs_size = 1 << 20;
  // NB: confusingly, a "sector" in pico-world should be a "block" in LFS-land
  flash_fs_num_blocks = (flash_fs_size / FLASH_SECTOR_SIZE) - 1;

  // TODO: not sure about this warning comment wrt FS
  // need to clear the script on first run of a new/"blank" rp2040
  // otherwise the vm will choke on the noise and fail usb enum
  uint8_t *v = (uint8_t *)(flash_status_offset + flash_start);
  if (*v != FLASH_OK) {
    flash_write_mode(0); // also writes FLASH_OK status field
  }
}

void flash_write_mode(uint8_t m) {
  // !!!
  // no multicore lockout because this must happen before multicore enabled
  uint8_t b[256]; // write must be 256 bytes
  b[0] = FLASH_OK;
  b[1] = m;
  flash_range_erase(flash_status_offset, FLASH_SECTOR_SIZE);
  flash_range_program(flash_status_offset, (const uint8_t *)b, 256);
}

uint8_t flash_read_mode() {
  uint8_t *v = (uint8_t *)(flash_status_offset + flash_start + 1);
  uint8_t m = *v;
  return m;
}
