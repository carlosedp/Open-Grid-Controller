#ifndef PICO_WEAVE_SERIAL_H
#define PICO_WEAVE_SERIAL_H

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#if 0
#define SERIAL_PRINT_BUF_SIZE 256
extern char serial_print_buf[];

#define print_serial(port, format, args...)                                    \
  snprintf(serial_print_buf, SERIAL_PRINT_BUF_SIZE, format, ##args);           \
  serial_tx(port, (uint8_t *)serial_print_buf, strlen(serial_print_buf));
#endif

extern void serial_task();
extern void serial_tx(const uint8_t *data, uint32_t len);
extern void serial_tx_str(const char *str);
extern int serial(const char *fmt, ...);

typedef void (*serial_rx_callback_t)(uint8_t *data, uint32_t len);
extern void serial_set_rx_callback(serial_rx_callback_t callback);

typedef void (*serial_line_state_callback_t)(bool dtr, bool rts);
extern void
serial_set_line_state_callback(serial_line_state_callback_t callback);

#endif // PICO_WEAVE_SERIAL_H
