# og_bench history

Results of [`scripts/og_bench.lua`](../scripts/og_bench.lua) on a Seeed XIAO RP2040 without keypads,
newest first. Each run: fresh boot, `og_bench()` then `og_bench("kria_iii.lua")` (kria_iii v1.6.3,
29 KB). Memory in KB, times per call.

Since og-0.2.9 the timings are split: `midi_to_msg` with the collector stopped (the function
itself), `alloc+gc` the same in a long loop including the garbage collection it causes, `gc` one
full collection. Before that, `midi_to_msg` meant what is now `alloc+gc`.

## Results

| Build           | Clock   | Script | Lua   | Heap free / largest | High water   | loop    | midi_to_msg | alloc+gc | gc      | redraw  |
| --------------- | ------- | ------ | ----- | ------------------- | ------------ | ------- | ----------- | -------- | ------- | ------- |
| og-0.2.9-200mhz | 200 MHz | -      | 41.7  | 170.8 / 90.6        | 152.3 of 220 | 3.83 µs | 49.3 µs     | 53.4 µs  | 2.00 ms | 0.66 ms |
| og-0.2.9-200mhz | 200 MHz | kria   | 110.7 | 96.3 / 58.8         | 204.3 of 220 | 3.83 µs | 49.8 µs     | 54.1 µs  | 1.60 ms | 0.67 ms |
| og-0.2.9        | 125 MHz | -      | 41.7  | 170.8 / 90.6        | 152.3 of 220 | 8.93 µs | 95.0 µs     | 100.6 µs | 3.24 ms | 1.03 ms |
| og-0.2.9        | 125 MHz | kria   | 110.7 | 96.3 / 58.8         | 204.3 of 220 | 8.92 µs | 94.2 µs     | 102.3 µs | 2.63 ms | 1.05 ms |
| og-0.2.8        | 125 MHz | -      | 41.0  | 171.4 / 89.7        | 156.3 of 220 | 8.94 µs | —           | 100.7 µs | —       | 1.03 ms |
| og-0.2.8        | 125 MHz | kria   | 110.2 | 96.9 / 57.9         | 208.3 of 220 | 8.92 µs | —           | 102.0 µs | —       | 1.05 ms |
| og-0.2.6        | 125 MHz | -      | 43.3  | 168.9 / 90.0        | 160.3 of 220 | 6.08 µs | —           | 83.5 µs  | —       | 1.03 ms |
| og-0.2.6        | 125 MHz | kria   | 112.8 | 94.1 / 61.4         | 208.3 of 220 | 6.07 µs | —           | 80.9 µs  | —       | 1.04 ms |
| og-0.2.4        | 125 MHz | -      | 43.3  | 169.5 / 92.1        | 160.9 of 221 | 6.07 µs | —           | 80.8 µs  | —       | 1.03 ms |
| og-0.2.1        | 125 MHz | -      | 59.0  | 156.1 / 82.6        | (sampled)    | 6.25 µs | —           | 98.3 µs  | —       | 1.35 ms |
| og-0.2.1        | 125 MHz | kria   | —     | —                   | —            | —       | —           | —        | —       | —       |

og-0.2.1 + kria: out of memory while loading (fixed by the streaming loader in og-0.2.2).

## Findings

- **200 MHz** (og-0.2.9-200mhz vs og-0.2.9): 1.6× (gc, redraw) to 2.3× (loop) faster; memory
  unchanged.
- **Code fetch from flash dominates.** `midi_to_msg` costs ~95 µs at 125 MHz with the collector
  stopped (~12,000 cycles for a few Lua API calls), so the C port of the MIDI helpers (og-0.2.7)
  gained nothing measurable: the time goes to XIP cache misses, not to interpreting Lua.
  `luaV_execute` alone is 11.4 KB and shares the 16 KB XIP cache with all other code.
- **Layout-sensitive:** the plain `loop` went from 6.1 µs (og-0.2.1 to 0.2.6) to 8.9 µs (og-0.2.8)
  without any change to the interpreter. Compare builds by several metrics, not one.
- **Memory:** loading kria peaks at ~204–208 KB of 220 KB (compiler working memory).
