#pragma once

// Onboard WS2812 status LED (RP2040-Zero GPIO16, XIAO RP2040 GPIO12).
//
// A base color shows the mode; short flashes show activity and errors.
// Scripts can take over the LED with og_led(r,g,b) from Lua.

#include <stdint.h>

#define OG_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

// Dim on purpose: the LED sits right next to the grid.
#define OG_COLOR_OFF OG_RGB(0, 0, 0)
#define OG_COLOR_III OG_RGB(0, 16, 0)            // green: iii mode
#define OG_COLOR_III_CLEAN OG_RGB(20, 10, 0)     // amber: iii, init.lua skipped
#define OG_COLOR_SERIAL OG_RGB(0, 0, 20)         // blue: monome serial mode
#define OG_COLOR_KEY OG_RGB(12, 12, 12)          // white flash: key event
#define OG_COLOR_ERROR OG_RGB(40, 0, 0)          // red flash: Lua error
#define OG_COLOR_MODE_SWITCH OG_RGB(20, 0, 20)   // magenta: BOOT held, switching mode
#define OG_COLOR_BOOT_WINDOW OG_RGB(0, 14, 14)   // cyan: press BOOT now for a clean boot

void og_status_init(void);

// Color shown when nothing else is going on.
void og_status_set_base(uint32_t rgb);

// Show a color for a while, then return to the base (or user) color.
void og_status_flash(uint32_t rgb, uint32_t ms);

// Lua override: while set, replaces the base color. og_status_clear_user()
// goes back to the mode color.
void og_status_set_user(uint32_t rgb);
void og_status_clear_user(void);

// Call often from the main loop; ends flashes and writes changes.
void og_status_task(void);
