#include "tusb.h"

#include "midi.h"
#include "serial.h"

void midi_rx_noop(uint8_t data1, uint8_t data2, uint8_t data3) {
  (void)data1;
  (void)data2;
  (void)data3;
}
static midi_rx_callback_t rx_callback = &midi_rx_noop;

void midi_task() {
  uint8_t packet[4];
  while (tud_midi_available()) {
    tud_midi_packet_read(packet);
#if 0
        rx_callback(packet[0] & 0xf, packet[0] & 0xf0, packet[1], packet[2]);
#else
    // is this expected? or are we off by 1B?
    rx_callback(packet[1], packet[2], packet[3]);
#endif
  }
}

void midi_tx(uint8_t data1, uint8_t data2, uint8_t data3) {
#if 1
  //-- stream interface does more work - buffering on cable streams, sysex
  //filter, etc
  static const uint8_t cable_number = 0;
  uint8_t packet[3] = {data1, data2, data3};
  tud_midi_stream_write(cable_number, packet, 3);
#else
  //-- seems more efficient for limited use cases
  // (i guess a trailing zero in this packet?)
  /// but actually, i've done something amiss and it does not work
  /// (maybe need to zero-pad first byte instead, as appears in RX)
  uint8_t packet[4] = {status | (channel & 0xf), data1, data2, 0};
  tud_midi_packet_write(packet);
#endif
}

void midi_set_rx_callback(midi_rx_callback_t callback) {
  rx_callback = callback;
}
