// TRS MIDI out: everything a script sends with midi_tx() also goes out
// OG_MIDI_TX_PIN at 31250 baud (wiring in board.h). Out only; MIDI in stays
// USB.
//
// midi_tx() always takes 3 bytes and iii pads shorter messages with zeros
// (a clock is F8 00 00). USB's stream parser sends the padding as stray
// bytes; here it would cost 0.64 ms of line time per clock and confuse
// receivers, so a small parser keeps only the bytes that belong to a message
// (sysex passes through until F7).
//
// The line is slow (320 us per byte), so bytes go into a ring that the UART
// interrupt drains; a script only waits when the ring is full.

#include "midi_uart.h"

#include <stdbool.h>

#include "board.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"

// A board without the jack leaves OG_MIDI_TX_PIN out (on monome's grid 2022,
// GPIO0 is the LED driver's I2C); then this does nothing.
#ifdef OG_MIDI_TX_PIN

#define MIDI_BAUD 31250
// ~160 ms of line time
#define RING_SIZE 512

static uint8_t ring[RING_SIZE];
static volatile uint32_t head = 0;  // written by tx (core 0)
static volatile uint32_t tail = 0;  // written by the interrupt
static bool ready = false;

// parser state, across midi_tx() calls: data bytes still expected for the
// current message, and whether we're inside a sysex
static uint8_t data_left = 0;
static bool in_sysex = false;

static void fill_fifo(void) {
  uart_hw_t *hw = uart_get_hw(OG_MIDI_UART);
  while (tail != head && uart_is_writable(OG_MIDI_UART)) {
    hw->dr = ring[tail % RING_SIZE];
    tail++;
  }
  // The TX interrupt fires when the FIFO drains past its trigger level, so
  // it's only enabled while there's more to send (an empty ring would keep
  // it asserted).
  if (tail != head) {
    hw_set_bits(&hw->imsc, UART_UARTIMSC_TXIM_BITS);
  } else {
    hw_clear_bits(&hw->imsc, UART_UARTIMSC_TXIM_BITS);
  }
}

static void on_uart_irq(void) { fill_fifo(); }

static void put(uint8_t b) {
  while (head - tail >= RING_SIZE) tight_loop_contents();  // the interrupt makes room
  const uint32_t ints = save_and_disable_interrupts();
  ring[head % RING_SIZE] = b;
  head++;
  fill_fifo();
  restore_interrupts(ints);
}

// data bytes after a status byte (sysex: none, its bytes pass while in_sysex)
static uint8_t data_len(uint8_t status) {
  switch (status & 0xF0) {
    case 0xC0:
    case 0xD0: return 1;
    case 0xF0:
      if (status == 0xF2) return 2;                    // song position
      if (status == 0xF1 || status == 0xF3) return 1;  // time code, song select
      return 0;
    default: return 2;
  }
}

static void parse(uint8_t b) {
  if (b >= 0xF8) {  // real time: may go anywhere, even inside other messages
    put(b);
  } else if (b >= 0x80) {
    if (b == 0xF7 && !in_sysex) return;  // stray end of sysex
    put(b);
    in_sysex = b == 0xF0;  // any other status byte ends a sysex too
    data_left = data_len(b);
  } else if (in_sysex) {
    put(b);
  } else if (data_left > 0) {
    put(b);
    data_left--;
  }
  // else: padding, or data without a status byte; dropped
}

void og_midi_uart_init(void) {
  uart_init(OG_MIDI_UART, MIDI_BAUD);
  uart_set_format(OG_MIDI_UART, 8, 1, UART_PARITY_NONE);
  uart_set_fifo_enabled(OG_MIDI_UART, true);
  gpio_set_function(OG_MIDI_TX_PIN, UART_FUNCSEL_NUM(OG_MIDI_UART, OG_MIDI_TX_PIN));
  const uint irq = UART_IRQ_NUM(OG_MIDI_UART);
  irq_set_exclusive_handler(irq, on_uart_irq);
  irq_set_enabled(irq, true);
  ready = true;
}

void og_midi_uart_tx(uint8_t data1, uint8_t data2, uint8_t data3) {
  if (!ready) return;
  parse(data1);
  parse(data2);
  parse(data3);
}

#else

void og_midi_uart_init(void) {}

void og_midi_uart_tx(uint8_t data1, uint8_t data2, uint8_t data3) {
  (void)data1;
  (void)data2;
  (void)data3;
}

#endif
