// grid_hw.h for FREEPOET's Open Grid kit: two 8x8 keypad boards on I2C
// (keypad.c), plus the family's own Lua call, og_keypads().

#include "grid_hw.h"

#include "keypad.h"
#include "lua.h"

void og_hw_init(void) { og_keypad_init(); }

bool og_hw_wait_ready(uint32_t timeout_ms) { return og_keypad_wait_ready(timeout_ms); }

bool og_hw_present(void) { return og_keypad_connected_mask() != 0; }

bool og_hw_read_key_raw(uint8_t x, uint8_t y, bool *pressed) {
  return og_keypad_read_key_raw(x, y, pressed);
}

void og_hw_scan(og_key_cb_t cb) { og_keypad_scan(cb); }

// The keypads have no global brightness: scale each level instead.
void og_hw_write_leds(const uint8_t *levels, uint8_t intensity) {
  uint8_t frame[OG_KEY_COUNT];
  for (int i = 0; i < OG_KEY_COUNT; i++) frame[i] = (uint8_t)((levels[i] * intensity + 7) / 15);
  og_keypad_write_leds(frame);
}

bool og_hw_needs_led_sync(void) { return og_keypad_needs_led_sync(); }

// og_keypads(): bitmask of the keypad boards online (bit 0 = left, bit 1 = right).
int og_l_keypads(lua_State *l) {
  lua_pushinteger(l, og_keypad_connected_mask());
  return 1;
}
