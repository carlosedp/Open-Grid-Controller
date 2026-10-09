#include "heap_stats.h"

#include <malloc.h>
#include <stdlib.h>

#include "pico/time.h"

#define SAMPLE_INTERVAL_US 10000

extern char end;           // start of the heap (linker)
extern char __StackLimit;  // the heap may grow up to here (see _sbrk in pico-sdk)
extern void *_sbrk(int incr);

static uint32_t peak_used = 0;

static uint32_t sample_used(void) {
  const struct mallinfo mi = mallinfo();
  const uint32_t used = (uint32_t)mi.uordblks;
  if (used > peak_used) peak_used = used;
  return used;
}

// Largest block malloc can hand out, found by binary search.
static uint32_t probe_largest(uint32_t upper) {
  uint32_t lo = 0, hi = upper;
  while (hi - lo > 64) {
    const uint32_t mid = lo + (hi - lo) / 2;
    void *p = malloc(mid);
    if (p) {
      free(p);
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return lo;
}

void og_heap_task(void) {
  static absolute_time_t next;
  if (!time_reached(next)) return;
  next = make_timeout_time_us(SAMPLE_INTERVAL_US);
  sample_used();
}

void og_heap_stats(og_heap_stats_t *out) {
  const struct mallinfo mi = mallinfo();
  const uint32_t top = (uint32_t)(uintptr_t)_sbrk(0);
  const uint32_t never_claimed = (uint32_t)(uintptr_t)&__StackLimit - top;

  out->total = (uint32_t)(&__StackLimit - &end);
  out->used = sample_used();
  out->free = (uint32_t)mi.fordblks + never_claimed;
  out->largest = probe_largest(out->free);
  out->peak = peak_used;
}
