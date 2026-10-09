#include "tusb.h"

#include "serial.h"

// FIXME: just sortof eating ram here
// big TX so lua can send big blocks of text to host
#define SERIAL_RX_BUFSIZE 4096
#define SERIAL_TX_BUFSIZE 16384

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

void serial_tx_send(const uint8_t *data, uint32_t len) {
  if ((len + tx_w) < SERIAL_TX_BUFSIZE) {
    for (uint32_t i = 0; i < len; i++) {
      tx_buf[tx_w] = data[i];
      tx_w++;
    }
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
  int l;
  va_list myargs;
  va_start(myargs, fmt);
  l = vsnprintf(tx_buf + tx_w, SERIAL_TX_BUFSIZE - tx_w, fmt, myargs);
  va_end(myargs);
  tx_w += (uint16_t)l;
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
