#pragma once

#include <stdint.h>

extern void repl_init();
extern void repl_handle_bytes(uint8_t *data, uint32_t len);
