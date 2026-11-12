#pragma once

// MIDI activity for diii's MIDI monitor (see midi_monitor.c).

#include "lua.h"

int og_l_midi_monitor(lua_State *l);
int og_l_midi_counts(lua_State *l);

// Sends the counts summary when it's due; call from the main loop.
void og_midi_monitor_task(void);
