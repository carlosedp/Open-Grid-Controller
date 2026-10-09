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

static void core1_entry(void) {
  // iii's flash writes pause this core; it must not run from flash meanwhile.
  multicore_lockout_victim_init();

  uint8_t frame[OG_KEY_COUNT];
  absolute_time_t next_scan = get_absolute_time();
  absolute_time_t next_led = get_absolute_time();

  while (true) {
    if (time_reached(next_scan)) {
      next_scan = make_timeout_time_us(OG_SCAN_INTERVAL_US);
      og_keypad_scan(push_event);
    }
    if ((dirty || og_keypad_needs_led_sync()) && time_reached(next_led)) {
      next_led = make_timeout_time_us(OG_LED_INTERVAL_US);
      // Clear first so writes that land during render() trigger another flush.
      dirty = false;
      render(frame);
      og_keypad_write_leds(frame);
    }
    sleep_us(100);
  }
}

void og_grid_init(void) {
  queue_init(&events, sizeof(og_key_event_t), EVENT_QUEUE_LEN);
  memset((void *)leds, 0, sizeof(leds));
  og_keypad_init();
}

void og_grid_start(void) { multicore_launch_core1(core1_entry); }

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
}

void og_grid_refresh(void) { dirty = true; }

void og_grid_flush_now(void) {
  uint8_t frame[OG_KEY_COUNT];
  render(frame);
  og_keypad_write_leds(frame);
  dirty = false;
}

bool og_grid_pop_event(og_key_event_t *event) { return queue_try_remove(&events, event); }
