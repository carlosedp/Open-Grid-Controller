// og_midi_monitor(on): reports every USB MIDI message the device sends or
// receives as a serial line, "^^midi(<i|o>,<b1>,<b2>,<b3>)" with the bytes in
// hex, for diii's MIDI monitor. Off by default; when off it costs one check
// per message.
//
// Hooked in at link time (--wrap, see CMakeLists.txt) instead of changing
// iii: every send goes through midi_tx() (vm.c), and incoming messages reach
// Lua through vm_handle_midi() (set as the receive callback in iii.c).

#include <stdbool.h>
#include <stdint.h>

#include "lua.h"
#include "serial.h"

static bool monitor_on = false;

static void report(char dir, uint8_t b1, uint8_t b2, uint8_t b3) {
  static const char hex[] = "0123456789abcdef";
  char line[] = "^^midi(x,00,00,00)\r\n";
  line[7] = dir;
  const uint8_t bytes[3] = {b1, b2, b3};
  for (int i = 0; i < 3; i++) {
    line[9 + i * 3] = hex[bytes[i] >> 4];
    line[10 + i * 3] = hex[bytes[i] & 0xF];
  }
  serial_tx((const uint8_t *)line, sizeof(line) - 1);
}

void __real_midi_tx(uint8_t data1, uint8_t data2, uint8_t data3);
void __wrap_midi_tx(uint8_t data1, uint8_t data2, uint8_t data3) {
  __real_midi_tx(data1, data2, data3);
  if (monitor_on) report('o', data1, data2, data3);
}

void __real_vm_handle_midi(uint8_t data1, uint8_t data2, uint8_t data3);
void __wrap_vm_handle_midi(uint8_t data1, uint8_t data2, uint8_t data3) {
  // before the script handles it, so the line comes ahead of what it prints
  if (monitor_on) report('i', data1, data2, data3);
  __real_vm_handle_midi(data1, data2, data3);
}

int og_l_midi_monitor(lua_State *l) {
  monitor_on = lua_toboolean(l, 1);
  return 0;
}
