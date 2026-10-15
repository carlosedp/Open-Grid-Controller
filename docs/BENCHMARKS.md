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
| og-0.3.3        | 125 MHz | -      | 41.9  | 173.3 / 91.8        | 59.2 of 223  | 4.60 µs | 48.5 µs     | 54.4 µs  | 2.89 ms | 1.00 ms |
| og-0.3.3        | 125 MHz | kria   | 110.7 | 99.2 / 53.7         | 147.2 of 223 | 4.60 µs | 48.4 µs     | 55.6 µs  | 2.20 ms | 0.98 ms |
| og-0.3.0        | 125 MHz | -      | 41.7  | 173.8 / 89.3        | 151.4 of 223 | 4.59 µs | 47.0 µs     | 53.4 µs  | 2.95 ms | 0.98 ms |
| og-0.3.0        | 125 MHz | kria   | 110.7 | 99.4 / 57.9         | 203.4 of 223 | 4.59 µs | 46.3 µs     | 54.4 µs  | 2.22 ms | 1.00 ms |
| og-0.2.10-vmram | 125 MHz | -      | 41.7  | 158.3 / 83.8        | 151.9 of 208 | 4.60 µs | 47.1 µs     | 53.6 µs  | 3.00 ms | 0.98 ms |
| og-0.2.10-vmram | 125 MHz | kria   | 110.7 | 83.9 / 50.7         | 195.9 of 208 | 4.60 µs | 46.9 µs     | 53.6 µs  | 5.49 ms | 1.00 ms |
| og-0.2.10       | 125 MHz | -      | 41.7  | 170.4 / 90.0        | 152.0 of 220 | 8.44 µs | 51.8 µs     | 58.0 µs  | 2.97 ms | 0.95 ms |
| og-0.2.10       | 125 MHz | kria   | 110.7 | 96.0 / 58.4         | 204.0 of 220 | 8.45 µs | 51.1 µs     | 59.2 µs  | 2.45 ms | 0.97 ms |
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
- **Flash clock differs per board** (pico-sdk board headers): the XIAO runs flash at clk/2
  (62.5 MHz at 125 MHz, 100 MHz at 200 MHz), the RP2040-Zero in the Open Grid kit at clk/4
  (31.25 / 50 MHz). Since code fetch dominates, expect the Zero to be noticeably slower than
  these XIAO numbers on the same build; measure on a Zero before drawing conclusions for the kit.
- **Toolchain** (og-0.3.3, PlatformIO's Arm GCC 14.2 and pico-sdk 2.2.0, vs og-0.3.0 built with
  the Arduino-Pico GCC): loop, gc and redraw unchanged; midi_to_msg +3–5%, alloc+gc +2%, within
  what code layout alone moves. The lower high water is og-0.3.1's probe fix (the og-0.3.0 rows
  were inflated by it), not the toolchain.
- **og-0.3.0 default** (interpreter in RAM, 4 KB serial buffers): same speed as the -vmram
  experiment with 223.4 KB of heap instead of 208 (and 220 before either change).
- **High water caveat:** until og-0.3.1, og_mem()'s largest-block probe could raise the high
  water mark itself, so values from a second og_mem()/og_bench() call in the same boot may be too
  high (one og-0.3.0 run showed 223.4 of 223.4 KB that way).
- **Interpreter loop in RAM** (og-0.2.10-vmram vs og-0.2.10, 125 MHz): pure-Lua `loop` 1.8×
  faster (8.44 → 4.60 µs); C-API-heavy paths −8–9% (midi_to_msg, alloc+gc); redraw and gc
  unchanged (gc with kria 5.49 ms vs 2.45 ms, single sample, to re-check). Heap total 220 → 208 KB;
  kria still loads (peak 196 of 208 KB).
- **Core 1 in RAM** (og-0.2.10 vs og-0.2.9, 125 MHz): midi_to_msg −45%, alloc+gc −42%, gc and
  redraw −8%, loop −5%, for 268 bytes of RAM. Core 1's wait loop had been evicting core 0's
  code from the shared XIP cache.
- **Code fetch from flash dominates.** `midi_to_msg` costs ~95 µs at 125 MHz with the collector
  stopped (~12,000 cycles for a few Lua API calls), so the C port of the MIDI helpers (og-0.2.7)
  gained nothing measurable: the time goes to XIP cache misses, not to interpreting Lua.
  `luaV_execute` alone is 11.4 KB and shares the 16 KB XIP cache with all other code.
- **Layout-sensitive:** the plain `loop` went from 6.1 µs (og-0.2.1 to 0.2.6) to 8.9 µs (og-0.2.8)
  without any change to the interpreter. Compare builds by several metrics, not one.
- **Memory:** loading kria peaks at ~204–208 KB of 220 KB (compiler working memory).
