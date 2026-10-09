#include "status_led.h"

#include <stdbool.h>

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/time.h"

#include "generated/ws2812.pio.h"

static PIO pio = pio0;
static int sm = -1;

static uint32_t base_color = OG_COLOR_OFF;
static uint32_t user_color;
static bool user_active = false;
static uint32_t flash_color;
static absolute_time_t flash_until;
static bool flash_active = false;
static uint32_t shown = 0xFFFFFFFF;

static void write_color(uint32_t rgb) {
  if (sm < 0 || rgb == shown) return;
  shown = rgb;
  // WS2812 takes GRB, MSB first, left-aligned in the 32-bit FIFO word.
  const uint32_t grb = ((rgb & 0x00FF00) << 8) | ((rgb & 0xFF0000) >> 8) | (rgb & 0xFF);
  pio_sm_put_blocking(pio, (uint)sm, grb << 8);
}

void og_status_init(void) {
#ifdef PICO_DEFAULT_WS2812_PIN
#ifdef PICO_DEFAULT_WS2812_POWER_PIN
  // The XIAO RP2040 powers its LED from a GPIO.
  gpio_init(PICO_DEFAULT_WS2812_POWER_PIN);
  gpio_set_dir(PICO_DEFAULT_WS2812_POWER_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_WS2812_POWER_PIN, 1);
#endif
  if (!pio_can_add_program(pio, &ws2812_program)) return;
  sm = pio_claim_unused_sm(pio, false);
  if (sm < 0) return;
  const uint offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, (uint)sm, offset, PICO_DEFAULT_WS2812_PIN, 800000, false);
  write_color(OG_COLOR_OFF);
#endif
}

void og_status_set_base(uint32_t rgb) { base_color = rgb; }

void og_status_flash(uint32_t rgb, uint32_t ms) {
  flash_color = rgb;
  flash_until = make_timeout_time_ms(ms);
  flash_active = true;
}

void og_status_set_user(uint32_t rgb) {
  user_color = rgb;
  user_active = true;
}

void og_status_clear_user(void) { user_active = false; }

void og_status_task(void) {
  if (flash_active && time_reached(flash_until)) flash_active = false;
  write_color(flash_active ? flash_color : user_active ? user_color : base_color);
}
