#include "grid.h"

#include <string.h>

#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/util/queue.h"

#ifndef OG_SCAN_INTERVAL_US
#define OG_SCAN_INTERVAL_US 2000
#endif
#ifndef OG_LED_INTERVAL_US
#define OG_LED_INTERVAL_US 4000
#endif

#define EVENT_QUEUE_LEN 64

static volatile uint8_t leds[OG_KEY_COUNT];
static volatile uint8_t intensity = 15;
static volatile bool dirty = true;
static volatile bool changed = true;  // for og_grid_take_changed(), core 0 only
static volatile uint32_t core1_beats = 0;
static queue_t events;

static void push_event(uint8_t x, uint8_t y, uint8_t z) {
  const og_key_event_t e = {x, y, z};
  queue_try_add(&events, &e);  // drop on overflow, like upstream iii
}

// Snapshot the framebuffer with global intensity applied.
static void render(uint8_t *out) {
  const uint8_t k = intensity;
  for (int i = 0; i < OG_KEY_COUNT; i++) {
    out[i] = (uint8_t)((leds[i] * k + 7) / 15);
  }
}

// Runs from RAM. Both cores fetch code from flash through one shared 16 KB
// XIP cache, and Lua on core 0 is limited by those fetches; a core 1 loop
// running from flash (sleep_us() every 100 us) kept evicting core 0's code.
// Now core 1 only touches flash for the scan and LED work, every 2-4 ms: the
// waiting and the deadline checks below are inline timer register reads.
static void __not_in_flash_func(core1_entry)(void) {
  // iii's flash writes pause this core; it must not run from flash meanwhile.
  multicore_lockout_victim_init();

  uint8_t frame[OG_KEY_COUNT];
  absolute_time_t next_scan = get_absolute_time();
  absolute_time_t next_led = get_absolute_time();

  while (true) {
    core1_beats++;
    if (time_reached(next_scan)) {
      next_scan = make_timeout_time_us(OG_SCAN_INTERVAL_US);
      og_keypad_scan(push_event);
    }
    if (time_reached(next_led) && (dirty || og_keypad_needs_led_sync())) {
      next_led = make_timeout_time_us(OG_LED_INTERVAL_US);
      // Clear first so writes that land during render() trigger another flush.
      dirty = false;
      render(frame);
      og_keypad_write_leds(frame);
    }
    const uint32_t wake = time_us_32() + 100;
    while ((int32_t)(time_us_32() - wake) < 0) tight_loop_contents();
  }
}

void og_grid_init(void) {
  queue_init(&events, sizeof(og_key_event_t), EVENT_QUEUE_LEN);
  memset((void *)leds, 0, sizeof(leds));
  og_keypad_init();
}

// Core 0's stack (Lua, USB) sits at the top of SCRATCH_Y and grows down into
// SCRATCH_X, where core 1's stack would normally be; deep Lua then silently
// corrupts both. Core 1 gets its own stack in main RAM instead, and with
// PICO_CORE1_STACK_SIZE=0 nothing is placed in SCRATCH_X, giving core 0 8 KB.
#define CORE1_STACK_WORDS 512  // 2 KB; the scan/flush loop needs well under 1 KB
static uint32_t core1_stack[CORE1_STACK_WORDS];

void og_grid_start(void) {
  multicore_launch_core1_with_stack(core1_entry, core1_stack, sizeof(core1_stack));
}

uint32_t og_grid_core1_beat(void) { return core1_beats; }

void og_grid_led(uint8_t x, uint8_t y, uint8_t z) {
  if (x >= OG_GRID_W || y >= OG_GRID_H) return;
  leds[y * OG_GRID_W + x] = z > 15 ? 15 : z;
}

uint8_t og_grid_led_get(uint8_t x, uint8_t y) {
  if (x >= OG_GRID_W || y >= OG_GRID_H) return 0;
  return leds[y * OG_GRID_W + x];
}

void og_grid_led_all(uint8_t z) {
  if (z > 15) z = 15;
  for (int i = 0; i < OG_KEY_COUNT; i++) leds[i] = z;
}

void og_grid_set_intensity(uint8_t z) {
  intensity = z > 15 ? 15 : z;
  dirty = true;
  changed = true;
}

void og_grid_refresh(void) {
  dirty = true;
  changed = true;
}

bool og_grid_take_changed(void) {
  const bool was = changed;
  changed = false;
  return was;
}

void og_grid_snapshot(uint8_t *out) {
  for (int i = 0; i < OG_KEY_COUNT; i++) out[i] = leds[i];
}

void og_grid_inject_key(uint8_t x, uint8_t y, uint8_t z) {
  if (x >= OG_GRID_W || y >= OG_GRID_H) return;
  push_event(x, y, z ? 1 : 0);
}

void og_grid_flush_now(void) {
  uint8_t frame[OG_KEY_COUNT];
  render(frame);
  og_keypad_write_leds(frame);
  dirty = false;
}

bool og_grid_pop_event(og_key_event_t *event) { return queue_try_remove(&events, event); }
