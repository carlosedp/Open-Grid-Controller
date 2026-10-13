#include <stdlib.h>

#include "tusb.h"

#include "serial.h"

// output too large for the TX buffer is pushed to the host while it is being
// written (see serial_tx_drain), so the buffer only needs to smooth out
// bursts; any size of output (e.g. cat() of a large file) gets through
#define SERIAL_TX_BUFSIZE 4096
// a read takes at most what the CDC FIFO holds (CFG_TUD_CDC_RX_BUFSIZE)
#define SERIAL_RX_BUFSIZE 512
// give up on output if the host takes nothing for this long
#define SERIAL_TX_STALL_US 250000

static char tx_buf[SERIAL_TX_BUFSIZE];
static uint16_t tx_w = 0;
static uint16_t tx_r = 0;

static char rx_buf[SERIAL_RX_BUFSIZE];

static void serial_rx_noop(uint8_t *data, uint32_t len) {
  (void)data;
  (void)len;
}

static serial_rx_callback_t rx_callback = &serial_rx_noop;

void serial_task() {
  // incoming
  if (tud_cdc_n_available(0)) {
    uint32_t count = tud_cdc_n_read(0, rx_buf, sizeof(rx_buf));
    rx_callback((uint8_t *)rx_buf, count);
  }
  // outgoing
  if (tx_w != tx_r) {
    uint32_t available = tud_cdc_n_write_available(0);
    uint32_t ready = tx_w - tx_r;
    uint32_t count = (available < ready) ? available : ready;
    for (uint32_t i = 0; i < count; i++) {
      tud_cdc_n_write_char(0, tx_buf[tx_r]);
      tx_r++;
    }
    if (tx_r == tx_w) {
      tx_r = 0;
      tx_w = 0;
    }
    tud_cdc_n_write_flush(0);
  }
}

void serial_set_rx_callback(serial_rx_callback_t callback) {
  rx_callback = callback;
}

void serial_tx_noop(const uint8_t *data, uint32_t len) {
  (void)data;
  (void)len;
}

// send buffered output to the host now, running the usb stack meanwhile,
// until it is all out or the host stops taking it; then move what is left
// to the front of the buffer
static void serial_tx_drain() {
  absolute_time_t deadline = make_timeout_time_us(SERIAL_TX_STALL_US);
  while (tx_r != tx_w && tud_cdc_n_connected(0)) {
    const uint32_t available = tud_cdc_n_write_available(0);
    if (available > 0) {
      const uint32_t ready = tx_w - tx_r;
      const uint32_t count = (available < ready) ? available : ready;
      tud_cdc_n_write(0, tx_buf + tx_r, count);
      tud_cdc_n_write_flush(0);
      tx_r += count;
      deadline = make_timeout_time_us(SERIAL_TX_STALL_US);
    } else if (time_reached(deadline)) {
      break;
    }
    tud_task();
  }
  if (tx_r == tx_w) {
    tx_r = 0;
    tx_w = 0;
  } else if (tx_r > 0) {
    memmove(tx_buf, tx_buf + tx_r, tx_w - tx_r);
    tx_w -= tx_r;
    tx_r = 0;
  }
}

// free space in tx_buf, draining it first if it holds less than `want`
static uint32_t serial_tx_space(uint32_t want) {
  // tx_w stays below SERIAL_TX_BUFSIZE (one byte kept for vsnprintf's \0)
  uint32_t space = SERIAL_TX_BUFSIZE - 1u - tx_w;
  if (space < want) {
    serial_tx_drain();
    space = SERIAL_TX_BUFSIZE - 1u - tx_w;
  }
  return space;
}

void serial_tx_send(const uint8_t *data, uint32_t len) {
  while (len > 0) {
    uint32_t space = serial_tx_space(len);
    if (space == 0)
      return; // host isn't taking output: drop the rest
    const uint32_t count = (len < space) ? len : space;
    memcpy(tx_buf + tx_w, data, count);
    tx_w += (uint16_t)count;
    data += count;
    len -= count;
  }
}

// void (*serial_tx_func)(const uint8_t *data, uint32_t len) = &serial_tx_noop;
void (*serial_tx_func)(const uint8_t *data, uint32_t len) = &serial_tx_send;

void serial_tx(const uint8_t *data, uint32_t len) {
  (*serial_tx_func)(data, len);
}

void serial_tx_str(const char *str) {
  serial_tx((const uint8_t *)str, strlen(str));
}

#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
int serial(const char *fmt, ...) {
  va_list myargs, sizing;
  va_start(myargs, fmt);
  va_copy(sizing, myargs);
  const int l = vsnprintf(NULL, 0, fmt, sizing);
  va_end(sizing);
  if (l > 0) {
    if (serial_tx_space((uint32_t)l) >= (uint32_t)l) {
      // fits (after draining if needed): format straight into the buffer
      vsnprintf(tx_buf + tx_w, (size_t)l + 1, fmt, myargs);
      tx_w += (uint16_t)l;
    } else {
      // larger than the whole buffer: format separately, send in chunks
      char *tmp = malloc((size_t)l + 1);
      if (tmp != NULL) {
        vsnprintf(tmp, (size_t)l + 1, fmt, myargs);
        serial_tx_send((const uint8_t *)tmp, (uint32_t)l);
        free(tmp);
      }
    }
  }
  va_end(myargs);
  return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+
void line_state_cb_state(bool dtr, bool rts) {
  (void)rts;
  if (dtr)
    serial_tx_func = &serial_tx_send;
  else
    serial_tx_func = &serial_tx_noop;
}

static serial_line_state_callback_t line_state_callback = &line_state_cb_state;

void serial_set_line_state_callback(serial_line_state_callback_t callback) {
  line_state_callback = callback;
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf;
  line_state_callback(dtr, rts);
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) { (void)itf; }
