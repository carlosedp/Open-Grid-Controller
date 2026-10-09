#include "monome_serial.h"

#include <string.h>

#include "tusb.h"

#include "grid.h"

enum {
  SYS_QUERY = 0x00,
  SYS_QUERY_ID = 0x01,
  SYS_GET_SIZE = 0x05,
  LED_OFF = 0x10,
  LED_ON = 0x11,
  LED_ALL_OFF = 0x12,
  LED_ALL_ON = 0x13,
  LED_MAP = 0x14,
  LED_ROW = 0x15,
  LED_COL = 0x16,
  LED_INTENSITY = 0x17,
  LED_LEVEL = 0x18,
  LED_LEVEL_ALL = 0x19,
  LED_LEVEL_MAP = 0x1A,
  LED_LEVEL_ROW = 0x1B,
  LED_LEVEL_COL = 0x1C,
};

#define RX_BUF_LEN 256

static uint8_t rx[RX_BUF_LEN];
static uint16_t rx_len = 0;

static bool have_pending_event = false;
static og_key_event_t pending_event;

// Length including the command byte; 0 = unknown command.
static uint8_t command_length(uint8_t cmd) {
  switch (cmd) {
    case SYS_QUERY: return 1;
    case SYS_QUERY_ID: return 1;
    case SYS_GET_SIZE: return 1;
    case LED_OFF: return 3;
    case LED_ON: return 3;
    case LED_ALL_OFF: return 1;
    case LED_ALL_ON: return 1;
    case LED_MAP: return 11;
    case LED_ROW: return 4;
    case LED_COL: return 4;
    case LED_INTENSITY: return 2;
    case LED_LEVEL: return 4;
    case LED_LEVEL_ALL: return 2;
    case LED_LEVEL_MAP: return 35;
    case LED_LEVEL_ROW: return 7;
    case LED_LEVEL_COL: return 7;
  }
  return 0;
}

static void send(const uint8_t *data, uint32_t len) {
  tud_cdc_n_write(0, data, len);
  tud_cdc_n_write_flush(0);
}

static void handle_command(const uint8_t *b) {
  switch (b[0]) {
    case SYS_QUERY: {
      // two 8x8 led quads, two 8x8 key quads
      const uint8_t leds[3] = {0x00, 1, 2};
      const uint8_t keys[3] = {0x00, 2, 2};
      send(leds, 3);
      send(keys, 3);
      return;
    }
    case SYS_QUERY_ID: {
      uint8_t packet[33] = {0x01};
      strcpy((char *)&packet[1], "monome one");
      send(packet, sizeof(packet));
      return;
    }
    case SYS_GET_SIZE: {
      const uint8_t packet[3] = {0x03, OG_GRID_W, OG_GRID_H};
      send(packet, 3);
      return;
    }
    case LED_OFF:
      og_grid_led(b[1], b[2], 0);
      break;
    case LED_ON:
      og_grid_led(b[1], b[2], 15);
      break;
    case LED_ALL_OFF:
      og_grid_led_all(0);
      break;
    case LED_ALL_ON:
      og_grid_led_all(15);
      break;
    case LED_MAP: {
      const uint8_t x_off = b[1] & 0x8;
      if (b[2] & 0x8) return;  // only one quad row on a 16x8 grid
      for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) og_grid_led(x_off + x, y, ((b[3 + y] >> x) & 1) * 15);
      }
      break;
    }
    case LED_ROW: {
      const uint8_t x_off = b[1] & 0x8;
      for (uint8_t i = 0; i < 8; i++) og_grid_led(x_off + i, b[2], ((b[3] >> i) & 1) * 15);
      break;
    }
    case LED_COL: {
      if (b[2] & 0x8) return;
      for (uint8_t i = 0; i < 8; i++) og_grid_led(b[1], i, ((b[3] >> i) & 1) * 15);
      break;
    }
    case LED_INTENSITY:
      og_grid_set_intensity(b[1]);
      return;
    case LED_LEVEL:
      og_grid_led(b[1], b[2], b[3]);
      break;
    case LED_LEVEL_ALL:
      og_grid_led_all(b[1]);
      break;
    case LED_LEVEL_MAP: {
      const uint8_t x_off = b[1] & 0x8;
      if (b[2] & 0x8) return;
      for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t i = 0; i < 4; i++) {
          const uint8_t z = b[3 + y * 4 + i];
          og_grid_led(x_off + i * 2, y, z >> 4);
          og_grid_led(x_off + i * 2 + 1, y, z & 0xF);
        }
      }
      break;
    }
    case LED_LEVEL_ROW: {
      const uint8_t x_off = b[1] & 0x8;
      for (uint8_t i = 0; i < 4; i++) {
        og_grid_led(x_off + i * 2, b[2], b[3 + i] >> 4);
        og_grid_led(x_off + i * 2 + 1, b[2], b[3 + i] & 0xF);
      }
      break;
    }
    case LED_LEVEL_COL: {
      if (b[2] & 0x8) return;
      for (uint8_t i = 0; i < 4; i++) {
        og_grid_led(b[1], i * 2, b[3 + i] >> 4);
        og_grid_led(b[1], i * 2 + 1, b[3 + i] & 0xF);
      }
      break;
    }
  }
  og_grid_refresh();
}

static void parse_rx(void) {
  uint16_t pos = 0;
  while (pos < rx_len) {
    const uint8_t len = command_length(rx[pos]);
    if (len == 0) {
      // Unknown command: the stream is out of sync, drop everything.
      rx_len = 0;
      return;
    }
    if (len > rx_len - pos) break;  // wait for the rest of the packet
    handle_command(&rx[pos]);
    pos += len;
  }
  if (pos > 0) {
    memmove(rx, rx + pos, rx_len - pos);
    rx_len -= pos;
  }
}

void monome_serial_task(void) {
  if (tud_cdc_n_available(0) && rx_len < RX_BUF_LEN) {
    rx_len += (uint16_t)tud_cdc_n_read(0, rx + rx_len, RX_BUF_LEN - rx_len);
    parse_rx();
  }

  while (true) {
    if (!have_pending_event) {
      if (!og_grid_pop_event(&pending_event)) break;
      have_pending_event = true;
    }
    if (tud_cdc_n_write_available(0) < 3) break;
    const uint8_t packet[3] = {pending_event.z ? 0x21 : 0x20, pending_event.x, pending_event.y};
    send(packet, 3);
    have_pending_event = false;
  }
}
