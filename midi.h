#ifndef PICO_WEAVE_MIDI_H
#define PICO_WEAVE_MIDI_H

#include "pico/stdlib.h"

extern void midi_task();

extern void midi_tx(uint8_t data1, uint8_t data2, uint8_t data3);
typedef void (*midi_rx_callback_t)(uint8_t data1, uint8_t data2, uint8_t data3);
extern void midi_set_rx_callback(midi_rx_callback_t callback);

#endif // PICO_WEAVE_MIDI_H
