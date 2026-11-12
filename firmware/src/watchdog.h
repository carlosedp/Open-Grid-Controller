#pragma once

// Hardware watchdog: resets the board when core 0 stops returning to the main
// loop (a Lua loop that never ends, a hang in C or USB) or core 1 stops
// scanning. After a reset the firmware boots normally, so a stage setup comes
// back on its own; if it hangs again soon after booting, the next boot skips
// init.lua instead of looping.

#include <stdbool.h>

// Call first thing in main(). Returns true when the previous resets suggest a
// boot loop and init.lua should be skipped.
bool og_watchdog_boot(void);

// Start the watchdog. Call once boot is done and core 1 is running.
void og_watchdog_start(void);

// Feed it; call from the main loop. `report` prints the reset notice (iii mode
// only: in serial mode the port speaks the monome protocol).
void og_watchdog_task(bool report);
