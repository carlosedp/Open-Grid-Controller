#pragma once

// I2C driver for two FREEPOET 8x8 keypad boards (protocol 0x02) forming a
// 16x8 grid. C port of sw/src/keypad.cpp for the Pico SDK; register
// definitions follow third_party/FREEPOET_Keypad.
//
// Coordinates: x = column 0..15 (left to right), y = row 0..7 (top to bottom).
// LED buffers passed to this driver are row-major: index = y * 16 + x.

#include <stdbool.h>
#include <stdint.h>

#define OG_GRID_W 16
#define OG_GRID_H 8
#define OG_KEY_COUNT (OG_GRID_W * OG_GRID_H)

typedef void (*og_key_cb_t)(uint8_t x, uint8_t y, uint8_t z);

// Set up the I2C bus and probe the boards once.
void og_keypad_init(void);

// Poll until at least one board answers or the timeout expires.
bool og_keypad_wait_ready(uint32_t timeout_ms);

// Single raw (non-debounced) key read, for boot-time checks.
bool og_keypad_read_key_raw(uint8_t x, uint8_t y, bool *pressed);

// Read all keys, debounce, and call cb for every press/release edge.
// Also re-probes the boards periodically to handle hot-plug.
void og_keypad_scan(og_key_cb_t cb);

// Push LED levels (0-15, row-major) to the boards. Only boards whose
// content changed since the last write (or that need a resync) are sent.
void og_keypad_write_leds(const uint8_t *levels);

// True when a board (re)connected and needs a full LED write.
bool og_keypad_needs_led_sync(void);

// Bit n set = board n is online.
uint8_t og_keypad_connected_mask(void);
