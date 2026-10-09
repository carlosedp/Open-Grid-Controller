#include "keypad.h"

#include <string.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"

#include "board.h"

#define BOARD_COUNT 2
#define BOARD_COLS 8
#define KEYS_PER_BOARD (BOARD_COLS * OG_GRID_H)

#define PROTOCOL_VERSION 0x02
#define READ_BLOCK_MAX 32
#define PACKED_LEVEL_BLOCK_MAX 58
#define RETRY_COUNT 3
#define I2C_TIMEOUT_US 10000

#ifndef OG_DEBOUNCE_SAMPLES
#define OG_DEBOUNCE_SAMPLES 3
#endif
#define WARMUP_FRAMES 3
#define WARMUP_MS 200
#define PROBE_INTERVAL_MS 2000
#define PROBE_INTERVAL_DISCONNECTED_MS 500

#define REG_PROTOCOL_VERSION 0x00
#define REG_DEVICE_ADDRESS 0x01
#define REG_KEY_COUNT_L 0x02
#define REG_KEY_COUNT_H 0x03
#define REG_LED_SHOW 0x13
#define REG_LED_SET_LEVEL_BLOCK_PACKED 0x17
#define REG_KEY_READ_BLOCK 0x21

// Board 0 holds columns 0-7, board 1 columns 8-15. Address jumpers: see README.
static const struct {
  uint8_t address;
  uint8_t x_offset;
} boards[BOARD_COUNT] = {
    {0x2E, 0},
    {0x2D, 8},
};

// Per-key state below is indexed row-major (y * 16 + x), like the LED buffer.
static bool stable_pressed[OG_KEY_COUNT];
static bool candidate_pressed[OG_KEY_COUNT];
static uint8_t candidate_count[OG_KEY_COUNT];
static uint8_t last_sent_levels[OG_KEY_COUNT];

static bool board_connected[BOARD_COUNT];
static uint8_t warmup_frames[BOARD_COUNT];
static uint32_t warmup_until_ms[BOARD_COUNT];
static bool full_led_sync[BOARD_COUNT];
static uint32_t last_probe_ms;

static inline uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// The boards address keys column-major: local index = local_x * 8 + y.
static inline uint8_t local_to_xy_index(uint8_t board, uint8_t local) {
  const uint8_t x = (uint8_t)(boards[board].x_offset + local / OG_GRID_H);
  const uint8_t y = local % OG_GRID_H;
  return (uint8_t)(y * OG_GRID_W + x);
}

//------------------------------------------------------------------------
// I2C helpers

static bool write_command(uint8_t address, const uint8_t *data, size_t len) {
  for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
    if (i2c_write_timeout_us(OG_I2C_INST, address, data, len, false, I2C_TIMEOUT_US) == (int)len) {
      return true;
    }
  }
  return false;
}

// Write a request with a repeated start, then read the response.
static bool read_registers(uint8_t address, const uint8_t *request, size_t request_len,
                           uint8_t *response, size_t response_len) {
  for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
    if (i2c_write_timeout_us(OG_I2C_INST, address, request, request_len, true, I2C_TIMEOUT_US) !=
        (int)request_len) {
      continue;
    }
    if (i2c_read_timeout_us(OG_I2C_INST, address, response, response_len, false, I2C_TIMEOUT_US) ==
        (int)response_len) {
      return true;
    }
  }
  return false;
}

static bool read_register(uint8_t address, uint8_t reg, uint8_t *value) {
  return read_registers(address, &reg, 1, value, 1);
}

static bool read_key_block(uint8_t address, uint8_t start, uint8_t count, uint8_t *out) {
  const uint8_t request[3] = {REG_KEY_READ_BLOCK, start, count};
  return read_registers(address, request, sizeof(request), out, count);
}

static bool probe(uint8_t address) {
  uint8_t version;
  return read_register(address, REG_PROTOCOL_VERSION, &version);
}

static bool read_device_info(uint8_t address) {
  uint8_t version, device_address, count_l, count_h;
  if (!read_register(address, REG_PROTOCOL_VERSION, &version) ||
      !read_register(address, REG_DEVICE_ADDRESS, &device_address) ||
      !read_register(address, REG_KEY_COUNT_L, &count_l) ||
      !read_register(address, REG_KEY_COUNT_H, &count_h)) {
    return false;
  }
  const uint16_t key_count = (uint16_t)(count_l | (count_h << 8));
  return version == PROTOCOL_VERSION && device_address == address && key_count >= KEYS_PER_BOARD;
}

static bool write_packed_level_block(uint8_t address, uint8_t start, uint8_t count,
                                     const uint8_t *levels) {
  uint8_t command[3 + (PACKED_LEVEL_BLOCK_MAX + 1) / 2];
  command[0] = REG_LED_SET_LEVEL_BLOCK_PACKED;
  command[1] = start;
  command[2] = count;
  for (uint8_t i = 0; i < count; i += 2) {
    const uint8_t first = levels[i] & 0x0F;
    const uint8_t second = (i + 1 < count) ? (levels[i + 1] & 0x0F) : 0;
    command[3 + i / 2] = (uint8_t)((first << 4) | second);
  }
  return write_command(address, command, (size_t)(3 + (count + 1) / 2));
}

static bool show_leds(uint8_t address) {
  const uint8_t command = REG_LED_SHOW;
  return write_command(address, &command, 1);
}

//------------------------------------------------------------------------
// Connection tracking

static void reset_board_state(uint8_t board) {
  for (uint8_t local = 0; local < KEYS_PER_BOARD; local++) {
    const uint8_t i = local_to_xy_index(board, local);
    stable_pressed[i] = false;
    candidate_pressed[i] = false;
    candidate_count[i] = 0;
    last_sent_levels[i] = 0xFF;
  }
  full_led_sync[board] = true;
}

static void refresh_connection(bool force) {
  const uint32_t now = now_ms();
  if (!force) {
    const bool any = board_connected[0] || board_connected[1];
    const uint32_t interval = any ? PROBE_INTERVAL_MS : PROBE_INTERVAL_DISCONNECTED_MS;
    if (now - last_probe_ms < interval) return;
  }
  last_probe_ms = now;

  for (uint8_t b = 0; b < BOARD_COUNT; b++) {
    const bool was_present = board_connected[b];
    const bool present =
        was_present ? probe(boards[b].address) : read_device_info(boards[b].address);

    if (present && !was_present) {
      board_connected[b] = true;
      warmup_frames[b] = WARMUP_FRAMES;
      warmup_until_ms[b] = now + WARMUP_MS;
      reset_board_state(b);
    } else if (!present && was_present) {
      board_connected[b] = false;
      reset_board_state(b);
    }
  }
}

//------------------------------------------------------------------------
// Public API

void og_keypad_init(void) {
  i2c_init(OG_I2C_INST, OG_I2C_HZ);
  gpio_set_function(OG_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(OG_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(OG_I2C_SDA_PIN);
  gpio_pull_up(OG_I2C_SCL_PIN);

  memset(last_sent_levels, 0xFF, sizeof(last_sent_levels));
  for (uint8_t b = 0; b < BOARD_COUNT; b++) full_led_sync[b] = true;
  refresh_connection(true);
}

bool og_keypad_wait_ready(uint32_t timeout_ms) {
  const uint32_t deadline = now_ms() + timeout_ms;
  while (true) {
    if (og_keypad_connected_mask()) return true;
    if ((int32_t)(now_ms() - deadline) >= 0) return false;
    sleep_ms(20);
    refresh_connection(true);
  }
}

bool og_keypad_read_key_raw(uint8_t x, uint8_t y, bool *pressed) {
  if (x >= OG_GRID_W || y >= OG_GRID_H) return false;
  const uint8_t b = x / BOARD_COLS;
  if (!board_connected[b]) return false;
  const uint8_t local = (uint8_t)((x % BOARD_COLS) * OG_GRID_H + y);
  uint8_t state;
  if (!read_key_block(boards[b].address, local, 1, &state)) return false;
  *pressed = state != 0;
  return true;
}

void og_keypad_scan(og_key_cb_t cb) {
  refresh_connection(false);

  const uint32_t now = now_ms();
  uint8_t raw[KEYS_PER_BOARD];

  for (uint8_t b = 0; b < BOARD_COUNT; b++) {
    if (!board_connected[b]) continue;

    bool failed = false;
    for (uint8_t start = 0; start < KEYS_PER_BOARD; start += READ_BLOCK_MAX) {
      if (!read_key_block(boards[b].address, start, READ_BLOCK_MAX, raw + start)) {
        failed = true;
        full_led_sync[b] = true;
        break;
      }
    }
    if (failed) continue;

    const bool in_warmup = warmup_frames[b] > 0 || (int32_t)(now - warmup_until_ms[b]) < 0;
    if (warmup_frames[b] > 0) warmup_frames[b]--;
    if (in_warmup) continue;

    for (uint8_t local = 0; local < KEYS_PER_BOARD; local++) {
      const uint8_t i = local_to_xy_index(b, local);
      const bool pressed = raw[local] != 0;

      if (pressed == stable_pressed[i]) {
        candidate_count[i] = 0;
        continue;
      }
      if (pressed == candidate_pressed[i]) {
        if (candidate_count[i] < 0xFF) candidate_count[i]++;
      } else {
        candidate_pressed[i] = pressed;
        candidate_count[i] = 1;
      }
      if (candidate_count[i] >= OG_DEBOUNCE_SAMPLES) {
        stable_pressed[i] = pressed;
        candidate_count[i] = 0;
        cb(i % OG_GRID_W, i / OG_GRID_W, pressed ? 1 : 0);
      }
    }
  }
}

void og_keypad_write_leds(const uint8_t *levels) {
  uint8_t block[KEYS_PER_BOARD];

  for (uint8_t b = 0; b < BOARD_COUNT; b++) {
    if (!board_connected[b]) continue;

    bool changed = full_led_sync[b];
    for (uint8_t local = 0; local < KEYS_PER_BOARD; local++) {
      const uint8_t i = local_to_xy_index(b, local);
      block[local] = levels[i] > 15 ? 15 : levels[i];
      if (block[local] != last_sent_levels[i]) changed = true;
    }
    if (!changed) continue;

    bool ok = true;
    for (uint8_t start = 0; ok && start < KEYS_PER_BOARD; start += PACKED_LEVEL_BLOCK_MAX) {
      const uint8_t count = (uint8_t)((KEYS_PER_BOARD - start) < PACKED_LEVEL_BLOCK_MAX
                                          ? (KEYS_PER_BOARD - start)
                                          : PACKED_LEVEL_BLOCK_MAX);
      ok = write_packed_level_block(boards[b].address, start, count, block + start);
    }
    if (!ok || !show_leds(boards[b].address)) {
      full_led_sync[b] = true;
      continue;
    }

    for (uint8_t local = 0; local < KEYS_PER_BOARD; local++) {
      last_sent_levels[local_to_xy_index(b, local)] = block[local];
    }
    full_led_sync[b] = false;
  }
}

bool og_keypad_needs_led_sync(void) {
  for (uint8_t b = 0; b < BOARD_COUNT; b++) {
    if (board_connected[b] && full_led_sync[b]) return true;
  }
  return false;
}

uint8_t og_keypad_connected_mask(void) {
  return (uint8_t)((board_connected[0] ? 1 : 0) | (board_connected[1] ? 2 : 0));
}
