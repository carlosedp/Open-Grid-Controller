#ifndef PICO_WEAVE_METRO_H
#define PICO_WEAVE_METRO_H

// seems there are 15 hardware timers
#define METRO_COUNT 15

#include "pico/stdlib.h"

// call once to initialize SDK components like locks
extern void metro_init();

// set the metro at given index to run every s seconds
extern void metro_set(int index, double s);

// set with initial count value (repeats N times)
extern void metro_set_with_count(int index, double s, int count);

extern void metro_task();

// clears all metros
extern void metro_cleanup();

#endif // PICO_WEAVE_METRO_H
