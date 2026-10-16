#include "watchdog.h"

#include "hardware/watchdog.h"
#include "pico/time.h"

#include "grid.h"
#include "serial.h"

// The RP2040's longest timeout is ~8.3 s. Anything this long is a hang: the
// REPL, USB and metros all stop while core 0 is away from the main loop, and
// compiling a big script (kria_iii) takes well under a second.
#define WATCHDOG_TIMEOUT_MS 8000
// Core 1 loops every 100 us; flash writes pause it for up to ~50 ms.
#define CORE1_STALL_US 1000000
// A reset within this long after boot counts towards a boot loop.
#define SETTLE_US 30000000
// Resets in a row, each soon after boot, before init.lua is skipped.
#define BOOT_LOOP_RESETS 2

// scratch[0..3] are free (the SDK uses 4..7) and survive a watchdog reset.
#define SCRATCH_STRIKES 0
#define STRIKES_MAGIC 0x4f470000u  // "OG"
#define STRIKES_MASK 0x0000ffffu

static uint32_t resets = 0;
static bool skipped_init = false;
static bool settled = false;
static bool reported = false;
static uint32_t core1_beat = 0;
static absolute_time_t core1_seen;

bool og_watchdog_boot(void) {
  const uint32_t saved = watchdog_hw->scratch[SCRATCH_STRIKES];
  if (watchdog_enable_caused_reboot()) {
    const bool valid = (saved & ~STRIKES_MASK) == STRIKES_MAGIC;
    resets = (valid ? saved & STRIKES_MASK : 0) + 1;
    if (resets > STRIKES_MASK) resets = STRIKES_MASK;
  }
  watchdog_hw->scratch[SCRATCH_STRIKES] = STRIKES_MAGIC | resets;
  skipped_init = resets >= BOOT_LOOP_RESETS;
  return skipped_init;
}

void og_watchdog_start(void) {
  core1_beat = og_grid_core1_beat();
  core1_seen = get_absolute_time();
  watchdog_enable(WATCHDOG_TIMEOUT_MS, true);  // paused while a debugger halts the cores
}

void og_watchdog_task(bool report) {
  const absolute_time_t now = get_absolute_time();
  const uint32_t beat = og_grid_core1_beat();
  if (beat != core1_beat) {
    core1_beat = beat;
    core1_seen = now;
  }
  // Starve the watchdog when core 1 is stuck, so a hang on either core resets.
  if (absolute_time_diff_us(core1_seen, now) < CORE1_STALL_US) watchdog_update();

  // Running for a while: the next reset starts a new count.
  if (!settled && to_us_since_boot(now) > SETTLE_US) {
    settled = true;
    watchdog_hw->scratch[SCRATCH_STRIKES] = STRIKES_MAGIC;
  }

  if (report && !reported && resets > 0) {
    reported = true;  // serial output waits in the buffer until a host connects
    serial("watchdog: the device stopped responding and was reset%s\r\n",
           skipped_init ? "; init.lua skipped (it hung again right after boot)" : "");
  }
}
