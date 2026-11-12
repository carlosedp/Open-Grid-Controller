// grid_hw.h for FREEPOET's Open Grid kit: two 8x8 keypad boards on I2C
// (keypad.c), plus the family's own Lua calls, pt_keypads() and
// pt_i2c_stats().

#include "grid_hw.h"

#include "hardware/timer.h"
#include "keypad.h"
#include "lua.h"
#include "serial.h"

// Durations of the bus work on core 1, for pt_i2c_stats(): a full key scan,
// and an LED write that sent at least one board. avg is a running average
// (1/16 per sample), so it can't overflow.
typedef struct {
  volatile uint32_t count;
  volatile uint32_t max_us;
  volatile uint32_t avg_us;
} timing_t;

static timing_t scan_timing, led_timing;
static uint32_t stats_since_us;

static void record(timing_t *t, uint32_t us) {
  if (us > t->max_us) t->max_us = us;
  t->avg_us =
      t->count ? (uint32_t)((int32_t)t->avg_us + ((int32_t)us - (int32_t)t->avg_us) / 16) : us;
  t->count++;
}

void og_hw_init(void) { og_keypad_init(); }

bool og_hw_wait_ready(uint32_t timeout_ms) { return og_keypad_wait_ready(timeout_ms); }

bool og_hw_present(void) { return og_keypad_connected_mask() != 0; }

bool og_hw_read_key_raw(uint8_t x, uint8_t y, bool *pressed) {
  return og_keypad_read_key_raw(x, y, pressed);
}

void og_hw_scan(og_key_cb_t cb) {
  const uint32_t start = time_us_32();
  og_keypad_scan(cb);
  if (og_keypad_connected_mask()) record(&scan_timing, time_us_32() - start);
}

// The keypads have no global brightness: scale each level instead.
void og_hw_write_leds(const uint8_t *levels, uint8_t intensity) {
  uint8_t frame[OG_KEY_COUNT];
  for (int i = 0; i < OG_KEY_COUNT; i++) frame[i] = (uint8_t)((levels[i] * intensity + 7) / 15);
  const uint32_t start = time_us_32();
  if (og_keypad_write_leds(frame)) record(&led_timing, time_us_32() - start);
}

bool og_hw_needs_led_sync(void) { return og_keypad_needs_led_sync(); }

// pt_keypads(): bitmask of the keypad boards online (bit 0 = left, bit 1 = right).
int og_l_keypads(lua_State *l) {
  lua_pushinteger(l, og_keypad_connected_mask());
  return 1;
}

// pt_i2c_stats(reset): prints the keypad bus figures since boot (or the last
// reset) and returns them: clock Hz, scans per second, scan avg/max us, LED
// write avg/max us, transfers, failed attempts, failed transfers, boards
// dropped. pt_i2c_stats(true) prints, then starts counting again.
int og_l_i2c_stats(lua_State *l) {
  og_keypad_bus_stats_t bus;
  og_keypad_bus_stats(&bus);
  const uint32_t now = time_us_32();
  const uint32_t elapsed_ms = (now - stats_since_us) / 1000;
  const uint32_t scans_per_s =
      elapsed_ms ? (uint32_t)((uint64_t)scan_timing.count * 1000 / elapsed_ms) : 0;

  serial(
      "i2c %lu Hz, keypads %u: %lu scans/s, scan avg %lu max %lu us, "
      "led write avg %lu max %lu us (%lu writes)\r\n"
      "transfers %lu, failed attempts %lu, failed transfers %lu, boards dropped %lu, "
      "over %lu s\r\n",
      (unsigned long)bus.hz, (unsigned)og_keypad_connected_mask(), (unsigned long)scans_per_s,
      (unsigned long)scan_timing.avg_us, (unsigned long)scan_timing.max_us,
      (unsigned long)led_timing.avg_us, (unsigned long)led_timing.max_us,
      (unsigned long)led_timing.count, (unsigned long)bus.transfers,
      (unsigned long)bus.failed_attempts, (unsigned long)bus.failed_transfers,
      (unsigned long)bus.drops, (unsigned long)(elapsed_ms / 1000));

  const lua_Integer values[] = {
      bus.hz,
      scans_per_s,
      scan_timing.avg_us,
      scan_timing.max_us,
      led_timing.avg_us,
      led_timing.max_us,
      bus.transfers,
      bus.failed_attempts,
      bus.failed_transfers,
      bus.drops,
  };
  const int n = (int)(sizeof(values) / sizeof(values[0]));
  for (int i = 0; i < n; i++) lua_pushinteger(l, values[i]);

  if (lua_toboolean(l, 1)) {
    og_keypad_bus_stats_reset();
    scan_timing.count = scan_timing.max_us = scan_timing.avg_us = 0;
    led_timing.count = led_timing.max_us = led_timing.avg_us = 0;
    stats_since_us = now;
  }
  return n;
}
