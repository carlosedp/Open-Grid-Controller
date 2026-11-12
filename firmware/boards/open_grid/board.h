#pragma once

// The open_grid family: FREEPOET's Open Grid kit, two 8x8 keypad boards on
// I2C forming a 16x8 grid, on the controller boards we build for. Pick one
// with -DPT_BOARD=<name> at CMake configure time (see CMakeLists.txt);
// OG_BOARD_NAME matches the UF2 names in dist/ (greenhouse picks the update
// file with it). What a family's board.h provides: src/grid_hw.h.

#include "hardware/i2c.h"

#define OG_GRID_W 16
#define OG_GRID_H 8

#if defined(OG_BOARD_RP2040_ZERO)
// Waveshare RP2040-Zero, as shipped in the Open Grid kit.
#define OG_BOARD_NAME "rp2040_zero"
#define OG_I2C_INST i2c0
#define OG_I2C_SDA_PIN 4
#define OG_I2C_SCL_PIN 5

#elif defined(OG_BOARD_XIAO_RP2040)
// Seeed XIAO RP2040: SDA = D4 (GPIO6), SCL = D5 (GPIO7).
#define OG_BOARD_NAME "xiao_rp2040"
#define OG_I2C_INST i2c1
#define OG_I2C_SDA_PIN 6
#define OG_I2C_SCL_PIN 7

#else
#error "no board selected: configure with -DPT_BOARD=rp2040_zero or -DPT_BOARD=xiao_rp2040"
#endif

// TRS MIDI out (midi_uart.c), GPIO0 on both boards (XIAO: D6). Wired as TRS
// type B, like Polyend's gear, so a plain stereo cable connects them:
// tip <- 33 ohm <- 3V3, ring <- 10 ohm <- GPIO0, sleeve to GND.
// (Type A swaps tip and ring.)
#define OG_MIDI_UART uart0
#define OG_MIDI_TX_PIN 0

// The family's own Lua calls (grid_hw.c), added to the device library.
struct lua_State;
int og_l_keypads(struct lua_State *l);
#define OG_BOARD_LUA_FUNCS {"og_keypads", og_l_keypads},
#define OG_BOARD_HELP "  og_keypads()    bitmask of keypad boards online\n"

#ifndef OG_I2C_HZ
#define OG_I2C_HZ 100000
#endif
