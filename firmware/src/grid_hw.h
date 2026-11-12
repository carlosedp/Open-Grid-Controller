#pragma once

// The grid hardware: keys and LEDs, as each board family has them
// (boards/<family>/, picked by PT_BOARD in CMakeLists.txt). Everything else in
// src/ only goes through this and grid.h.
//
// The family's board.h defines OG_GRID_W, OG_GRID_H and OG_BOARD_NAME (what
// gh_board() reports; the UF2 is named after it), and optionally:
//   OG_MIDI_TX_PIN, OG_MIDI_UART  TRS MIDI out (midi_uart.c); left out, no TRS
//   OG_BOARD_LUA_FUNCS            extra luaL_Reg entries for the family's own
//                                 Lua calls (e.g. pt_keypads)
//
// Threads: init and the *_raw/wait calls run on core 0 before core 1 starts;
// scan and write_leds run on core 1 only (grid.c's loop, from RAM).

#include <stdbool.h>
#include <stdint.h>

#include "board.h"

#define OG_KEY_COUNT (OG_GRID_W * OG_GRID_H)

typedef void (*og_key_cb_t)(uint8_t x, uint8_t y, uint8_t z);

// Set up the bus and pins. Before core 1 starts.
void og_hw_init(void);

// Wait until the hardware answers or the timeout expires; true if it did.
bool og_hw_wait_ready(uint32_t timeout_ms);

// True when the keys and LEDs answer (false, e.g., with the keypads unplugged).
bool og_hw_present(void);

// One raw (not debounced) key read, for boot-time checks; false if it failed.
bool og_hw_read_key_raw(uint8_t x, uint8_t y, bool *pressed);

// Read the keys, debounce, call cb for each press (z = 1) and release (z = 0).
void og_hw_scan(og_key_cb_t cb);

// Show LED levels (0-15, row-major, index = y * OG_GRID_W + x) at a global
// intensity (0-15), however the hardware does that.
void og_hw_write_leds(const uint8_t *levels, uint8_t intensity);

// True when the LEDs need a full write even if nothing changed (a board came back).
bool og_hw_needs_led_sync(void);
