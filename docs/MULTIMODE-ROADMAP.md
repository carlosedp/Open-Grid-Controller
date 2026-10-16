# Open Grid — Multi-Mode Firmware Roadmap

Plan for turning the Open Grid Controller from a monome-only grid into a multi-mode instrument:
**monome grid** (norns-safe), **USB MIDI controller**, and **standalone** instrument (sequencers,
Push-style keyboard, generative scripts), all from one firmware.

**Decision: build on monome [iii](https://monome.org/docs/iii/).** iii runs Lua scripts on the
grid's own RP2040, exposes USB-MIDI + a USB-serial REPL, stores scripts on an onboard file system,
and is managed from the browser with diii. Every mode above becomes either a Lua script or the
built-in monome-serial mode, instead of custom C++ plus a custom SysEx configurator. The earlier
C++ plan is kept, condensed, in [Appendix A](#appendix-a--superseded-c-plan) as a fallback.

Each milestone has a scope and acceptance criteria; do them in order, commit per milestone.

---

## 1. Where things are

### 1.1 Repository layout

```text
iii-open-grid/          iii firmware (Pico SDK + CMake)           ← new, GPL-3.0
  src/main.c            boot, mode switch, iii device glue, grid Lua API
  src/grid.{h,c}        LED framebuffer, key event queue, core1 I/O loop
  src/keypad.{h,c}      FREEPOET keypad I2C driver (C port of sw/src/keypad.cpp)
  src/monome_serial.c   monome serial protocol (norns / serialosc mode)
  src/usb_descriptors.c per-mode USB descriptors
  src/status_led.{h,c}  WS2812 status LED (M2)
  src/heap_stats.{h,c}  C heap statistics for og_mem()
  src/lua_vm_in_ram.h   puts the Lua interpreter loop in RAM (force-included)
  src/generated/        pre-generated PIO program (WS2812)
  tools/pio_cmake.py    PlatformIO → CMake bridge
diii/                   diii web app: fork of monome/web-diii as a git subtree (GPL-3.0)
scripts/og_bench.lua    memory/speed benchmark run on the device (results: docs/BENCHMARKS.md)
Makefile                firmware, upload, diii server, dist/ packaging, iii/diii subtrees (make help)
tools/dist.py           collects UF2s and the diii site into dist/
platformio.ini          PlatformIO project for iii-open-grid (installs the whole toolchain)
third_party/            vendored lua, littlefs, FREEPOET_Keypad; iii as a git subtree with our
                        changes on top (see third_party/README.md)
sw/                     original Arduino firmware (monome serial only), unchanged
docs/                   this file, BENCHMARKS.md
```

### 1.2 Hardware snapshot

| Item         | Value                                                                                                |
| ------------ | ---------------------------------------------------------------------------------------------------- |
| MCU          | RP2040-Zero (Waveshare), 2 MB flash. Dev/test board: Seeed XIAO RP2040 (also 2 MB)                   |
| Grid         | 16 cols × 8 rows = 128 keys, 16 LED levels (0–15)                                                    |
| Keypads      | 2× FREEPOET_Keypad, I2C @ 100 kHz, addresses `0x2E` (x 0–7) and `0x2D` (x 8–15)                      |
| I2C pins     | RP2040-Zero: I2C0 SDA GPIO4 / SCL GPIO5. XIAO: I2C1 SDA GPIO6 (D4) / SCL GPIO7 (D5)                  |
| Key read     | Register `0x21` block read, 1 byte per key, 32 keys per transaction → 2 transactions per board       |
| LED write    | Register `0x17` packed 4-bit levels (≤ 58 keys per write) + `0x13` show                              |
| Extra inputs | BOOT button (readable at runtime, used for mode switching); RGB LED (WS2812 on GPIO16 / GPIO12 XIAO) |

### 1.3 Firmware status (`iii-open-grid`, og-0.3.4)

Tested on a Seeed XIAO RP2040 without keypads (diii, uploads/downloads, USB MIDI, mode switching,
virtual grid, kria_iii). Not yet run with the keypad boards, on an RP2040-Zero, or on norns.
Builds: ~323 KB flash, ~32 KB static RAM, **223 KB heap** (~220 KB with iii's defaults), with Lua
~1.8× faster; see [BENCHMARKS.md](BENCHMARKS.md).

- **Two modes**, stored in iii's flash status byte (same scheme as monome's grids):

  | Mode       | USB                   | VID:PID     | Strings (mfr / product)        | Used by                     |
  | ---------- | --------------------- | ----------- | ------------------------------ | --------------------------- |
  | iii (0)    | CDC (REPL) + USB MIDI | `cafe:1101` | `Open Grid` / `iii open grid`  | diii, DAWs, standalone      |
  | serial (1) | CDC                   | `cafe:1110` | `monome` / `grid`, serial `m…` | norns, serialosc, Max/PD/SC |

  Interface names: `OpenGrid_Serial`, `OpenGrid_MIDI`. `cafe:1101` is required today: monome's
  hosted diii only lists ports with that ID. `0xCAFE` is TinyUSB's shared test VID; see M6.

- **Switching modes and recovery**:
  - hold the top-left key while plugging in → toggles iii ↔ serial (monome's behavior); when
    switching to iii, keep holding ~1 s (row 1 fills up) to skip `init.lua` (clean boot);
  - press BOOT while the status LED is cyan (first second after power-up) → iii, clean boot
    (works without keypads; rescues a board whose `init.lua` hangs);
  - hold BOOT for 2 s while running → toggle and reboot;
  - from Lua: `og_set_mode(0|1)`.
- **Lua grid API** identical to monome's iii grids (`event_grid`, `grid_led`, `grid_led_all`,
  `grid_led_get`, `grid_intensity`, `grid_refresh`, `grid_size_x/y`), so existing iii grid
  scripts run unchanged. `device_id()` returns `"grid"` for the same reason.
- **Open Grid Lua extras** (listed by `help()`):

  | Call             | What it does                                                                    |
  | ---------------- | ------------------------------------------------------------------------------- |
  | `og_set_mode(m)` | 0 = iii, 1 = monome serial; saves and reboots                                   |
  | `og_keypads()`   | bitmask of keypad boards online                                                 |
  | `og_mem(quiet)`  | C heap: prints KB, returns bytes `free, largest, used, peak, total, high_water` |
  | `og_key(x,y,z)`  | inject a key event through the same queue as real keys                          |
  | `og_led(r,g,b)`  | status LED color 0–255; `og_led()` returns it to the mode color                 |
  | `og_mirror(on)`  | stream LED frames as `^^grid(<128 hex levels>)`, ≤ 30 fps, for diii             |
  | `og_dump_file(f)` | print file `f` as base64 lines, then `size crc32`; for diii's backup           |

- **Cores**: core 1 owns I2C (key scan every 2 ms, LED flush at most every 4 ms when dirty) and
  runs from RAM; core 0 runs USB + Lua with 8 KB of stack (both scratch banks). Key events cross
  through a multicore-safe queue; core 1 registers for flash lockout, so file writes are safe.
- **Lua interpreter loop in RAM** by default (`OG_LUA_VM_IN_RAM`); **200 MHz** optional
  (`OG_FAST_CLOCK=1` in the environment).
- **iii itself** carries our fixes and memory/speed changes, tracked commit by commit in
  [third_party/README.md](../third_party/README.md) and exportable as upstream patches (M4).
- **1200-baud touch** reboots into the UF2 bootloader (PlatformIO upload works without BOOT).
- serial mode mirrors monome's iii-grid-2022 command set (and fixes three small bugs found there).

### 1.4 Building

**Make** — `make help` lists everything: `make upload [BOARD=rp2040_zero]`, `make diii` (serve
diii locally), `make dist` (UF2s + diii site), `make iii-patches` (our iii changes as patches).

**PlatformIO (VS Code extension or CLI)** — the root `platformio.ini` installs pico-sdk 2.2.0, Arm
GCC 14.2, CMake and Ninja on first build:

```sh
pio run -e xiao_rp2040 -t upload      # or -e rp2040_zero
```

Builds for comparison set `OG_FAST_CLOCK=1` (200 MHz) or `OG_LUA_VM_IN_RAM=0` (interpreter in
flash) in the environment, e.g. `OG_FAST_CLOCK=1 pio run -e xiao_rp2040`.

**Plain CMake** — with any pico-sdk ≥ 2.0 (e.g. from the Raspberry Pi Pico VS Code extension):

```sh
cmake -S iii-open-grid -B build -G Ninja -DOG_BOARD=xiao_rp2040 -DPICO_SDK_PATH=...
cmake --build build      # → build/iii-open-grid.uf2
```

### 1.5 Keypad protocol facts (FREEPOET_Keypad lib v2.1.0, protocol `0x02`)

Source: [third_party/FREEPOET_Keypad](../third_party/FREEPOET_Keypad) (MIT). Host library only; the
keypad boards' own firmware is not published.

- Each board has its own RP2040 (`RP2-B2` = B2 stepping) acting as I2C slave. RP2040 I2C supports
  100 kHz / 400 kHz / 1 MHz; whether the closed keypad firmware keeps up above 100 kHz is untested.
- The protocol is shared with FREEPOET's 16×8 devices: a board reports up to **128** keys and
  addresses keys column-major (`index = x * 8 + y`). Our 8×8 boards use indices 0–63.
- Register map:

  | Reg             | Name                                 | Notes                                                   |
  | --------------- | ------------------------------------ | ------------------------------------------------------- |
  | `0x00`–`0x03`   | protocol ver, address, key count L/H |                                                         |
  | `0x10` / `0x11` | set pixel RGB (/ + show)             | **real RGB** (confirmed by FREEPOET), one LED per write |
  | `0x12` / `0x13` | clear / show                         | `set*` writes the buffer only; `show` flushes it        |
  | `0x14` / `0x15` | set level (/ + show)                 | one LED                                                 |
  | `0x16`          | level block, unpacked                | ≤ 29 LEDs                                               |
  | `0x17`          | level block, packed 4-bit            | ≤ 58 LEDs — what both firmwares use today               |
  | `0x18`          | 8×8 block                            | `[0x18, block 0/1, 32 packed bytes]` — a whole board    |
  | `0x19`          | full 16×8 frame                      | 64 packed bytes                                         |
  | `0x20`          | read one key                         | `[0x20, index]` → 1 byte                                |
  | `0x21`          | read key block                       | `[0x21, start, count ≤ 32]` → 1 byte (0/1) per key      |

- **No bitmap read, no event FIFO, no interrupt pin, no velocity.** Key state is on/off only.
- No RGB block write: color costs one ~6-byte transaction per LED (~70 ms for 128 LEDs at 100 kHz,
  ~7 ms at 1 MHz). Fine for sparse color, too slow for full-frame color animation.

### 1.6 Latency budget

At 100 kHz a 32-key block read is ≈ `(3 + 32) × 9 bits / 100 kHz ≈ 3.2 ms`, so one board scan is
≈ 6.5 ms and both ≈ 13 ms. With 3-sample debounce, press-to-event can reach ~40 ms — fine for a
sequencer, noticeable for finger drumming.

| I2C clock       | Key scan (both boards) | Full LED refresh (`0x18`, 2 × 35 B) |
| --------------- | ---------------------- | ----------------------------------- |
| 100 kHz (today) | ~13 ms                 | ~6.3 ms                             |
| 400 kHz         | ~3.3 ms                | ~1.6 ms                             |
| 1 MHz           | ~1.3 ms                | ~0.6 ms                             |

Wire time only; slave clock stretching must be measured with a logic analyzer. Pull-ups: 4.7 kΩ is
fine to 400 kHz on a short bus, ~2.2 kΩ for 1 MHz, to 3.3 V only.

### 1.7 Color

FREEPOET's assembled FP-Grid (a different, NeoTrellis-derived design) maps the 16 levels through a
**level → RGB palette** chosen on the device, so the protocol stays level-only and norns-compatible.
On the Open Grid the LEDs belong to the keypad MCUs, so the equivalent needs a keypad register
(`SET_LEVEL_PALETTE`, 16 × RGB per board, written once at boot) — a concrete ask for FREEPOET.
Until then: treat LED values as **semantic levels** (off, dim, in-scale, root, active…) in scripts,
and offer sparse per-LED color through a Lua call (M5).

---

## 2. Goals and non-goals

**Goals**

- monome serial mode behaves like a monome grid; norns must never be affected.
- iii mode runs existing iii grid scripts unchanged and works with diii.
- Class-compliant USB MIDI (no drivers on macOS/Windows/Linux/iOS) — provided by iii.
- Standalone use: scripts live in the grid's flash; power + a MIDI destination is all it needs.
- Self-contained repo: vendored sources, one-click PlatformIO build.
- Keep the Lua heap free for scripts: move hot or bulky library code to C.

**Non-goals (for now)**

- Velocity/pressure (pads are on/off).
- Full-frame RGB animation (one LED per I2C transaction).
- Impersonating other vendors' devices beyond what serialosc needs to detect a grid.

---

## 3. Milestones

### M0 — iii port ✅ (code done, step 1 passed 2026-10-09)

Everything in §1.3. **Acceptance**, in order:

1. ✅ **XIAO, no keypads**: flash via `pio run -t upload`; diii (Chrome) connects; REPL works
   (`print(device_id())`, `help()`); upload/run/delete a script; `og_keypads()` returns 0; a script
   sending `midi_tx(0x90, 60, 100)` plays a note in a DAW; BOOT held 2 s switches to serial mode and
   back. Simulate keys from the REPL with `event_grid(1,1,1)`.
   Result: all passed; a MIDI monitor received the notes and kria_iii's MIDI clock (`F8`).
2. **XIAO + keypads** (wire D4/D5 + 3V3/GND): keys reach `event_grid`, LEDs follow `grid_led` +
   `grid_refresh`, hot-unplug/replug of a board recovers.
3. **RP2040-Zero in the enclosure**: same, plus serial mode on **norns** (awake, mlr, kria) and
   serialosc on desktop behave exactly like the Arduino firmware.

### M1 — Latency and bus

- Make I2C clock tests easy (already `custom_og_i2c_hz` / `-DOG_I2C_HZ`). Test 400 kHz, then 1 MHz,
  with a logic analyzer (24 MHz Saleae clone + PulseView). Watch NACKs, clock stretching, retries;
  fall back to 100 kHz automatically above an error threshold.
- LED writes: `0x18` (one transaction per board) instead of `0x17` ×2 + show.
- Debounce: 2 samples, or eager press with debounced release (`OG_DEBOUNCE_SAMPLES`).
- Add `[perf]` counters (scan time, LED time, I2C errors) readable from Lua, e.g. `og_stats()`.
- **Accept:** measured press-to-USB latency before/after; no ghost or missed presses in a 5-minute
  hammer test on all 128 keys.

### M2 — Status feedback ✅ (verified on the XIAO; RP2040-Zero and keypads pending)

- WS2812 status LED via PIO (RP2040-Zero GPIO16; XIAO GPIO12 with power on GPIO11). The PIO
  program header is pre-generated in `src/generated/`, so builds don't need `pioasm`.

  | Color       | Meaning                                        |
  | ----------- | ---------------------------------------------- |
  | green       | iii mode                                       |
  | amber       | iii mode, clean boot (`init.lua` skipped)      |
  | blue        | monome serial mode                             |
  | white flash | key press (real or `og_key`)                   |
  | red flash   | Lua error inside `event_grid`                  |
  | magenta     | BOOT held; mode switches at 2 s                |
  | cyan        | first second after power-up: BOOT = clean boot |

  Scripts can take the LED over with `og_led(r,g,b)`.

- Short left-to-right sweep on the grid when iii starts (skipped without keypads; serial mode
  stays dark, like today).
- **Not covered:** errors in metro/MIDI callbacks happen inside iii's `vm.c`; flagging them needs
  a small iii hook (upstream candidate).
- **Accept:** colors match the table on the XIAO and the RP2040-Zero; the sweep shows with keypads.

### M3 — Open Grid script library (replaces the old C++ MIDI-mode milestones)

Lua scripts in a new `scripts/` folder, each usable on its own and selectable from a launcher:

- ✅ **Launcher**: use [many-roads](https://github.com/boreal-ground/many-roads-grid) (tested at
  `691af85`, listed in the README's recommended scripts). It lights one key per stored script, and
  triple-clicking the top-left key returns to it. It has no license, so it is linked rather than
  vendored; ask the authors to add one (MIT, like most iii scripts), then vendor it.
  **Restart broken since og-0.2.6:** many-roads restarts by deliberately running out of memory,
  relying on iii rebooting on a failed allocation. Our firmware now lets Lua recover instead (M4),
  so the triple-click only prints "not enough memory". iii's `device_reset()` (re-runs `init.lua`)
  does what it wants without the trick; propose that change to the many-roads authors.
- **`midi_controller.lua`**: generic MIDI controller driven by a preset file (notes, CC momentary /
  toggle, faders as columns, page keys, MIDI-in LED feedback). Preset = a small Lua/JSON table in
  flash, written by diii or a web editor (M5).
- **`keys.lua`**: Push-style isomorphic keyboard (13×8 note area + 3 control columns: octave,
  root, scale, layout, shift menu), scales from `mu`.
- **`drums.lua`**, **`step.lua`** (16 × 8), arpeggiator, Euclidean, Game of Life.
- Use existing community iii grid scripts as-is where they fit.
- **Accept:** play a soft synth in a DAW with scale/root/octave changes without touching the
  computer; switch scripts without a computer.

### M4 — Memory and speed ✅ (verified on the XIAO, og-0.3.3; measurements in [BENCHMARKS.md](BENCHMARKS.md))

Scripts hit iii's memory limits (lines iii thread; [monome/iii#7](https://github.com/monome/iii/issues/7),
[#33](https://github.com/monome/iii/issues/33)). Work happens in `third_party/iii` (a git subtree)
as one commit per topic, each exportable with `make iii-patches` and tracked with its measured
effect in [third_party/README.md](../third_party/README.md); Lua API unchanged throughout.

- ✅ **Done** (XIAO RP2040, 125 MHz):

| Change                                                 | Effect                                                                |
| ------------------------------------------------------ | --------------------------------------------------------------------- |
| stream Lua files into the compiler                     | no full source copy while loading; kria_iii (29 KB) loads reliably    |
| stream uploads to flash, no 32 KB buffer               | upload peak 197 → 168 KB; no 32 KB script limit                       |
| no reboot on a failed allocation; panic handler        | Lua's emergency GC recovers; real OOM is a script error, not a reboot |
| GC pause 120% instead of 200%                          | garbage can't outgrow RAM before a collection; no measurable cost     |
| 4 KB serial TX that drains when full; 512 B RX         | −15.5 KB static RAM; output of any size (downloads, big prints) works |
| `serial()` bounds; NUL-terminated version check; TRUNC | memory-corruption and lib.lua rewrite bugs fixed                      |
| MIDI helpers in C                                      | correct; no measurable gain (code fetch dominates, see below)         |
| core 0 stack 8 KB (core 1 stack moved to RAM)          | fixes silent lockups when Lua runs deep                               |
| core 1 loop in RAM                                     | midi_to_msg/alloc+gc −45%, gc/redraw −8%                              |
| Lua interpreter loop in RAM                            | pure Lua 1.8× faster, for ~11 KB (paid for by the serial buffers)     |
| 200 MHz (optional)                                     | another 1.6–2.3×                                                      |

**monome/iii#7 and #33**, point by point: string-building loops that hang (GC pacing → 120% pause);
"raise a memory error properly" (allocator returns NULL, Lua raises "not enough memory"); lockups
without a serial warning (core 0's stack overflowing into core 1's, and `serial()` writing past its
buffer: both silent RAM corruption, both fixed); `lua_atpanic` (now prints and reboots); RAM
"goblins" (32 KB upload buffer, 16 + 4 KB serial buffers: all gone); #33's freeze after ~1 hour
with ansible (prints never read, so the 16 KB TX buffer filled in ~1 h and `serial()` overflowed;
now output is dropped, after waiting at most once for a host that isn't reading). #7's repro
(the loop from the issue; was `scripts/issue7.lua`, see git history) verified on og-0.3.3: n=10 and n=13 complete, a
300 KB allocation raises "not enough memory", and the REPL keeps running.

**Findings:** code fetch from flash (16 KB XIP cache, shared by both cores) dominates speed, so
moving hot code into RAM beats rewriting Lua in C. The RP2040-Zero runs flash at half the XIAO's
clock (board headers: clk/4 vs clk/2), so measure on a Zero before drawing conclusions for the kit.

Loading kria peaks at 155 of 223 KB, so compiling big scripts no longer needs work. Optional
speedups are under §5; sending the patches upstream is M11.

### M5 — diii fork (PWA + Open Grid features)

Fork of [monome/web-diii](https://github.com/monome/web-diii) (GPL-3.0, static site):
[carlosedp/web-diii](https://github.com/carlosedp/web-diii), imported into this repo as a **git
subtree at `diii/`** (history kept). `make diii` serves it locally, `make dist` packages it with a
per-build cache name, `make diii-pull-upstream` merges monome's `main`, and `make diii-push-fork`
pushes `diii/` to a branch of the fork (`open-grid`) for upstream PRs. Hosting:
**<https://diii.vfmod.com>**, published from this repository by `.github/workflows/diii-pages.yml`
whenever `diii/` changes on `main` (built with `tools/dist.py --diii-only`, which stamps the
service-worker cache with the commit). Done so far, tested
against og-0.2.x/0.3.x on the XIAO; nothing pushed yet:

- ✅ branch `pwa-fixes` (based on upstream `main`, kept separate so it can go upstream as a PR):
  - relative service-worker and app-shell paths, so it installs from a sub-path; manifest
    `id`/`scope`/`start_url` relative;
  - versioned cache, stale-while-revalidate, old caches deleted on activate, so updates reach
    installed copies;
  - 192/512 px PNG and maskable icons rendered from `logo.svg`.
- ✅ branch `virtual-grid` (on top of `pwa-fixes`): a **grid** button appears when the device has
  `og_mirror`; the panel shows the LEDs live from `^^grid(...)` frames and sends presses with
  `og_key`, multi-touch, blocked during uploads. Stock iii devices are unaffected.

Next:

- ✅ help button (prints `help()`).
- ✅ Publish: in carlosedp/Open-Grid-Controller set Pages → Source: GitHub Actions, custom domain
  `diii.vfmod.com` (Enforce HTTPS); DNS CNAME `diii` → `carlosedp.github.io`.
- Offer `pwa-fixes` upstream as a PR to monome/web-diii.
- Accept our own VID:PID (M6) alongside `cafe:1101`.
- Preset editor for `midi_controller.lua` (M3), script launcher management.

### M6 — Identity

- Request a USB PID from [pid.codes](https://pid.codes) (VID `0x1209`, free for open hardware)
  for iii mode, keep `cafe:1101` as a build option for stock-diii compatibility until our diii fork
  (or upstream) accepts the new ID.
- Serial mode keeps monome-style strings (`monome` / `m…` serial): that is how serialosc finds
  grids. Verify which fields serialosc actually requires and document it.

### M7 — Ableton Live integration

- Quick: `midi_controller.lua` preset for Session view (clip grid on notes, scene column, mixer
  rows on CCs) with LED feedback from MIDI-in.
- Proper: a Python Remote Script (`integrations/ableton/OpenGrid/`, `_Framework` SessionComponent
  15×8 + scene column) sending clip state as note velocities → LED levels.
- **Accept:** the session "red box" follows the grid; clip states show as brightness.

### M8 — Hardware MIDI out (DIY)

- TRS MIDI (Type A) from a spare GPIO: UART TX @ 31250 baud, 3.3 V wiring (33 Ω on TX, 10 Ω on
  3.3 V); optional opto-isolated MIDI in (6N138/H11L1) for clock sync.
- Expose as a second MIDI port to Lua (`midi_tx` with a port argument — needs an iii change, see M4
  item 5).
- **Accept:** Open Grid on a USB power bank plays a hardware synth directly.

### M9 — Diii improvements

- ✅ Virtual grid for testing and development without physical hardware (M5).
- ✅ Add a button to print the help commands (button that calls help())
- ✅ Is there an issue renaming the USB device to OpenGrid_Serial and OpenGrid_MIDI specially for other
  gear to identify it like Monome Norns?
- Be able to remove the "First" flag from a script (not load it on startup)
- ✅ Investigate memory leak from running og_mem() repeatedly. Expected to increase until a gc()?
  Likely expected: each REPL line is compiled into a chunk that becomes garbage until the next
  collection; `og_mem()` itself keeps nothing. Check: `for i=1,1000 do og_mem(true) end
collectgarbage() print(collectgarbage("count"))` should stay flat.
- **Framework port: decided against (2026-10-15).** diii is ~2,000 lines of plain JS with no
  dependencies, and our fork follows monome/web-diii; a TypeScript/framework rewrite would end
  upstream merges and PRs and mean re-finding the Web Serial quirks (port opening on Windows,
  reconnects, partial lines, upload pacing) without tests. Instead, in this order:
  1. ✅ Split `diii.js` into native ES modules (`diii/js/`, mixed into the one app class; all
     methods verified identical). Still no build step.
  2. ~~Type checking the existing code~~: tried `tsc --noEmit` with JSDoc (2026-10-15). The
     methods mixed into one class make `App` a circular type (TS2456), with `@satisfies
     ThisType<App>` and with per-method `@this`; typing it would need a hand-kept interface or a
     composition refactor, not worth it for working code. Instead, new features (zip writer, MIDI
     monitor) are standalone modules with explicit inputs, `// @ts-check`ed one by one (Web Serial
     and File System Access types need `@types/w3c-web-serial` etc.).
  3. ✅ Backup / restore (og-0.3.4, verified on the XIAO 2026-10-15, also while kria runs): `og_dump_file(name)` in our firmware
     (`src/file_dump.c`, iii API only) sends a file as base64 lines plus size and CRC-32, 96 bytes
     at a time from static buffers; diii's **backup** zips every file (`js/zip.js`, `js/backup.js`,
     standalone, `tsc --strict` clean) and **restore** uploads a zip's files through the normal,
     compile-checked upload path (skips `lib.lua`). Without `og_dump_file` diii falls back to
     `cat()`. Later: move `og_dump_file` into iii as `fs_dump_file` (a fourth upstream PR) and
     have diii look for both names; byte-exact restore would need a matching chunked write.
  4. ✅ Script editor (untested on hardware): "edit" in a file's menu opens a panel next to the
     console (which stays as is): prism-code-editor 5.4 (25 KB, vendored, Lua highlighting,
     search, bracket matching, indent guides), save (Ctrl+S) / save as / run / close, unsaved
     dot. On Open Grid firmware a failed save keeps the device's file. Tooling: `diii/package.json`
     for dev tools, Node dev server, `make diii-check` / `diii-vendor`. Later: tabs, iii API
     autocomplete (prism-code-editor's autocomplete extension).
  5. MIDI monitor like the Alloy Controller's, through the browser's Web MIDI (no firmware change).
  6. iii Lua command reference and help in diii, cached for offline use.

  Revisit a framework (Preact/Lit) only if diii grows past ~5,000 lines with several views sharing
  state, or we stop following upstream.
- **Visual refresh**, done separately from the items above so it can be evaluated on its own
  (kept, changed or dropped): a more modern, user-friendly interface.

### M10 - Build firmware for stock versions of Grid

- Have this improved firmware available for stock versions of Grids so users can also leverage the changes

### M11 - Upstream iii changes and improvements ✅ (submitted 2026-10-15; waiting for review)

- Consolidate the patches listed on third_party/README.md so they can be upstreamed to the main iii repository. Make sure our changes are not specific for Open Grid Controller but would also work on all Grid devices.
- ✅ Tested outside our firmware (2026-10-15): tehn's iii-grid-2022 built for a XIAO RP2040
  (pico-sdk 2.2.0, his `-Werror` warning set; D0/D1 tied to 3V3 so the ADC key scan reads no
  keys), stock iii v1.1.2 vs v1.1.2 + our 17 upstream patches. #7's n=10 loop: stock prints
  `-- out of memory!` and reboots (watchdog); patched completes n=10 and n=13, a 300 KB
  allocation raises "not enough memory", the REPL keeps running. The build also caught a
  `-Wconversion` error in our serial TX change (fixed; our build now uses the same flags).
- ✅ Prepared as three PRs (branches `pr1-fixes`, `pr2-loading`, `pr3-serial` in a clone of
  carlosedp/iii; patches and PR texts in `dist/patches/iii-upstream/`): fixes (OOM as a Lua
  error, GC pause, serial overflow, lib.lua version/truncate), loading (streaming loader and
  uploads, compile-error names; builds on tehn's unmerged `chunks` branch and says so), serial
  (draining 4 KB TX buffer, held boot output; on top of the fixes). Fixups squashed, no `iii:`
  prefix, an AI-assistance note in each PR text instead of commit trailers. MIDI helpers in C held
  back (changes `lib.lua`, no gain on its own), so no `III_VERSION` bump. Every commit builds in
  iii-grid-2022 with `-Werror`; the three merged equal the build that passed #7 on the XIAO. All
  four device repos already set `PICO_MALLOC_PANIC=0`, which the allocation-failure patch needs.
- ✅ Posted as [tehn/iii#10](https://codeberg.org/tehn/iii/pulls/10) (fixes),
  [#11](https://codeberg.org/tehn/iii/pulls/11) (loading) and
  [#12](https://codeberg.org/tehn/iii/pulls/12) (serial), and announced on the lines iii thread.
- Next: answer review comments (rework in carlosedp/iii, test in iii-grid-2022 again); as PRs merge,
  mark them in third_party/README.md and `make iii-pull-upstream`. Our unsquashed commits will
  conflict with the merged versions; resolve to upstream's code, keeping our fork-only commits
  (`v1.1.2-ogN`, MIDI helpers in C).
- Post the series with its measurements (BENCHMARKS.md, the #7 loop) on the iii thread / codeberg, referencing monome/iii#7 and #33.

Refs:

<https://llllllll.co/t/iii/74311/177>
<https://codeberg.org/tehn/iii/pulls>

### Asks for FREEPOET (keypad protocol `0x03`)

Bitmap key read (8 bytes per board instead of 64), a "keys changed" flag or interrupt line, a
level→RGB palette register, an RGB block write, and a documented way to update keypad firmware.

---

## 4. Risks and open questions

- **Untested on hardware**: boot timing of the keypad MCUs (we wait up to 500 ms for them), I2C
  behavior while core1 is paused for flash writes (bus is held mid-transfer for up to ~50 ms).
- **norns / serialosc**: serial mode now uses VID:PID `cafe:1110`, product `grid` and a per-board
  serial (`m` + 7 digits) like monome's grid-2022, instead of the Arduino firmware's fixed
  `m6000000`. Verify detection before relying on it.
- **Windows descriptor caching**: different PIDs per mode avoid stale drivers.
- **GPL-3.0**: firmware built from `iii-open-grid/` is GPL-3.0 (iii). The Arduino firmware in `sw/`
  and the repo's other files keep their own license — the repo has no top-level LICENSE yet; add one.
- **Lua memory** (M4): compiling big scripts is still the peak (kria: 155 of 223 KB), with ~68 KB
  to spare.
- **Behavior changes vs. stock iii**: out of memory is now a Lua error instead of a reboot (breaks
  scripts that rely on the reboot, e.g. many-roads' restart, see M3); `lib.lua` is versioned
  `v1.1.2-ogN` so our copy replaces the device's.

---

## 5. Other ideas

- More speed (M4 follow-ups, measure with og_bench first): more hot code in RAM (table access, GC
  mark), RAM permitting; `metro` bookkeeping, `slew` and `mu` tables in C (MIDI showed C alone
  isn't enough); caching compiled bytecode in flash if load-time memory gets tight again.
- HID keyboard/macro script (needs an iii HID interface).
- OSC via serialosc in serial mode: Max/MSP, Pure Data, SuperCollider, TouchDesigner, VCV Rack.
- Lighting/VJ: MIDI → QLC+ (DMX), Resolume clip launching with feedback.
- Chord pads, MPE-style channel rotation, MIDI visualizer.
- DIY arc: iii has an arc port; an encoder board could become a norns-compatible arc.
- Toys: Simon, Game of Life, drawing pad — good demo scripts.

---

## Appendix A — superseded C++ plan

Kept as the fallback if iii proved unworkable. Summary of the design it replaced:

- Mode chosen at boot (USB descriptors are fixed at enumeration): `GRID` (CDC, as today), `MIDI`
  (USB MIDI), later `HYBRID`; selected by key combos at power-up, persisted in LittleFS.
- C++ modes behind a `Mode` interface (`begin/onKey/tick/render`) in the Arduino firmware.
- Configuration over **SysEx** (`F0 7D 4F 47 <ver> <cmd> … F7`): GET_INFO, GET_PAGE, SET_CELLS,
  SAVE, REVERT, FACTORY_RESET, SET_BOOT_MODE, REBOOT, IDENTIFY; edited by a Vite/TypeScript Web MIDI
  configurator and a `mido`-based CLI.
- Data model: 8 pages × 128 cells; cell = type (note, CC momentary/toggle, program, fader step,
  page select, transport), channel, number, on/off values, LED levels, group, flags.
- Same latency, Ableton and hardware-MIDI milestones as above.

With iii, the cell engine becomes `midi_controller.lua` (M3), SysEx is replaced by diii's file
upload, and the configurator becomes a diii feature (M5).
