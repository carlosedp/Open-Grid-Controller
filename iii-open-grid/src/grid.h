#pragma once

// Grid state shared by both firmware modes.
//
// Core 0 (USB, Lua or monome serial) writes the LED framebuffer and drains
// key events. Core 1 owns the I2C bus: it scans keys, queues events and
// flushes the framebuffer to the keypad boards when it is marked dirty.
//
// Framebuffer layout: row-major, index = y * 16 + x, values 0-15.

#include <stdbool.h>
#include <stdint.h>

#include "keypad.h"

typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t z;
} og_key_event_t;

// Init the keypad bus and buffers. Does not start core 1.
void og_grid_init(void);

// Start the core 1 scan/refresh loop.
void og_grid_start(void);

void og_grid_led(uint8_t x, uint8_t y, uint8_t z);
uint8_t og_grid_led_get(uint8_t x, uint8_t y);
void og_grid_led_all(uint8_t z);
void og_grid_set_intensity(uint8_t z);
void og_grid_refresh(void);

// Write the framebuffer to the boards right away, from core 0.
// Only for use before og_grid_start().
void og_grid_flush_now(void);

bool og_grid_pop_event(og_key_event_t *event);
