#pragma once

#include <stdint.h>

extern void flash_init();

extern void flash_write_mode(uint8_t m);
extern uint8_t flash_read_mode();

extern uint32_t flash_get_status_offset();

extern uint32_t flash_get_fs_start();
extern uint32_t flash_get_fs_offset();
extern uint32_t flash_get_fs_num_blocks();
