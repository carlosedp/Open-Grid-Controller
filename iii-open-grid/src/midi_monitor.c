// MIDI activity for diii's MIDI monitor.
//
// og_midi_monitor(on): reports every USB MIDI message the device sends or
// receives as a serial line, "^^midi(<i|o>,<b1>,<b2>,<b3>)" with the bytes in
// hex (diii's list). Off by default; when off it costs one check per message.
//
// og_midi_counts(on): every message is counted anyway (cheap); while on, a
// summary "^^midic(<in>,<out>,<bpm x10>)" (totals since boot, decimal; bpm 0
// without a recent clock) goes out at most twice a second, only when it
// changed, so diii can show activity with its list closed.
//
// Hooked in at link time (--wrap, see CMakeLists.txt) instead of changing
// iii: every send goes through midi_tx() (vm.c), and incoming messages reach
// Lua through vm_handle_midi() (set as the receive callback in iii.c).

#include "midi_monitor.h"

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"
#include "serial.h"

#define SUMMARY_INTERVAL_US 500000
// no clock for this long: tempo unknown again
#define CLOCK_TIMEOUT_US 1000000
// tempo over the last quarter note (24 clocks)
#define CLOCK_SPAN 24

static bool monitor_on = false;
static bool counts_on = false;
static uint32_t count_in = 0;
static uint32_t count_out = 0;

// times of the last CLOCK_SPAN + 1 incoming clocks (a ring)
static uint64_t clock_times[CLOCK_SPAN + 1];
static uint32_t clock_count = 0;

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
  count_out++;
  if (monitor_on) report('o', data1, data2, data3);
}

void __real_vm_handle_midi(uint8_t data1, uint8_t data2, uint8_t data3);
void __wrap_vm_handle_midi(uint8_t data1, uint8_t data2, uint8_t data3) {
  count_in++;
  if (data1 == 0xF8) clock_times[clock_count++ % (CLOCK_SPAN + 1)] = time_us_64();
  // before the script handles it, so the line comes ahead of what it prints
  if (monitor_on) report('i', data1, data2, data3);
  __real_vm_handle_midi(data1, data2, data3);
}

// tempo x10 from the last CLOCK_SPAN incoming clocks; 0 if there's no recent clock
static uint32_t bpm_x10(uint64_t now) {
  if (clock_count < CLOCK_SPAN + 1) return 0;
  const uint64_t newest = clock_times[(clock_count - 1) % (CLOCK_SPAN + 1)];
  const uint64_t oldest = clock_times[clock_count % (CLOCK_SPAN + 1)];
  if (now - newest > CLOCK_TIMEOUT_US || newest <= oldest) return 0;
  // CLOCK_SPAN clocks = one quarter note
  return (uint32_t)((600000000ull + (newest - oldest) / 2) / (newest - oldest));
}

void og_midi_monitor_task(void) {
  static uint64_t next = 0;
  static uint32_t sent_in = UINT32_MAX, sent_out = UINT32_MAX, sent_bpm = UINT32_MAX;
  if (!counts_on) return;
  const uint64_t now = time_us_64();
  if (now < next) return;
  next = now + SUMMARY_INTERVAL_US;

  const uint32_t bpm = bpm_x10(now);
  if (count_in == sent_in && count_out == sent_out && bpm == sent_bpm) return;
  sent_in = count_in;
  sent_out = count_out;
  sent_bpm = bpm;
  serial("^^midic(%lu,%lu,%lu)\r\n", (unsigned long)sent_in, (unsigned long)sent_out,
         (unsigned long)bpm);
}

int og_l_midi_monitor(lua_State *l) {
  monitor_on = lua_toboolean(l, 1);
  return 0;
}

int og_l_midi_counts(lua_State *l) {
  counts_on = lua_toboolean(l, 1);
  return 0;
}
