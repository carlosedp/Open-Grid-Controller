#pragma once

// monome serial protocol over USB CDC (the non-iii mode, for norns and
// serialosc). Command set and replies follow monome's iii-grid-2022 firmware.

// Call from the main loop: parses host commands and sends queued key events.
void monome_serial_task(void);
