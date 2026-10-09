#pragma once

// Pin maps for the controller boards we build for. Pick one with
// -DOG_BOARD=<name> at CMake configure time (see CMakeLists.txt).

#include "hardware/i2c.h"

#if defined(OG_BOARD_RP2040_ZERO)
// Waveshare RP2040-Zero, as shipped in the Open Grid kit.
#define OG_I2C_INST i2c0
#define OG_I2C_SDA_PIN 4
#define OG_I2C_SCL_PIN 5

#elif defined(OG_BOARD_XIAO_RP2040)
// Seeed XIAO RP2040: SDA = D4 (GPIO6), SCL = D5 (GPIO7).
#define OG_I2C_INST i2c1
#define OG_I2C_SDA_PIN 6
#define OG_I2C_SCL_PIN 7

#else
#error "no board selected: configure with -DOG_BOARD=rp2040_zero or -DOG_BOARD=xiao_rp2040"
#endif

#ifndef OG_I2C_HZ
#define OG_I2C_HZ 100000
#endif
