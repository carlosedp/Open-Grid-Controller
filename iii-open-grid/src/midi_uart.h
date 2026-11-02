#pragma once

// TRS MIDI out on a UART (see midi_uart.c).

#include <stdint.h>

void og_midi_uart_init(void);

// Queues one midi_tx() call (3 bytes, iii's zero padding included).
void og_midi_uart_tx(uint8_t data1, uint8_t data2, uint8_t data3);
