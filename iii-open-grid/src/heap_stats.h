#pragma once

// C heap statistics. Lua's mem() only counts what Lua itself holds; these
// numbers show what is left for everything (Lua, iii's buffers, uploads).

#include <stdint.h>

typedef struct {
  uint32_t total;    // heap region size (end of .bss to the stack limit)
  uint32_t used;     // bytes currently allocated
  uint32_t free;     // bytes still allocatable in total (may be fragmented)
  uint32_t largest;  // largest single block malloc can return right now
  uint32_t peak;     // highest `used` seen since boot (sampled)
} og_heap_stats_t;

// Sample usage for the peak value; call often from the main loop.
void og_heap_task(void);

// Full snapshot. Finding `largest` does a few trial allocations, so this is
// for on-demand use, not for every loop.
void og_heap_stats(og_heap_stats_t *out);
