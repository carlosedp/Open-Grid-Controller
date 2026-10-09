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
  tools/pio_cmake.py    PlatformIO → CMake bridge
platformio.ini          PlatformIO project for iii-open-grid (installs the whole toolchain)
third_party/            vendored iii, lua, littlefs, FREEPOET_Keypad (see third_party/README.md)
sw/                     original Arduino firmware (monome serial only), unchanged
docs/                   this file
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

### 1.3 Firmware status (`iii-open-grid`, og-0.1.0)

Builds for both boards (≈ 318 KB flash, 29 KB static RAM). **Not yet run on hardware.**

- **Two modes**, stored in iii's flash status byte (same scheme as monome's grids):

  | Mode       | USB                   | VID:PID     | Strings (mfr / product)        | Used by                     |
  | ---------- | --------------------- | ----------- | ------------------------------ | --------------------------- |
  | iii (0)    | CDC (REPL) + USB MIDI | `cafe:1101` | `Open Grid` / `iii open grid`  | diii, DAWs, standalone      |
  | serial (1) | CDC                   | `cafe:1110` | `monome` / `grid`, serial `m…` | norns, serialosc, Max/PD/SC |

  `cafe:1101` is required today: monome's hosted diii only lists ports with that ID. `0xCAFE` is
  TinyUSB's shared test VID; see M6 for getting our own.
- **Switching modes**:
  - hold the top-left key while plugging in → toggles iii ↔ serial (monome's behavior); when
    switching to iii, keep holding ~1 s (row 1 fills up) to skip `init.lua` (clean boot);
  - hold BOOT for 2 s while running → toggle and reboot (works without keypads);
  - from Lua: `og_set_mode(0|1)`.
- **Lua grid API** identical to monome's iii grids (`event_grid`, `grid_led`, `grid_led_all`,
  `grid_led_get`, `grid_intensity`, `grid_refresh`, `grid_size_x/y`), so existing iii grid
  scripts run unchanged. `device_id()` returns `"grid"` for the same reason. Extras: `og_keypads()`.
- **Core split**: core1 owns I2C (key scan every 2 ms, LED flush at most every 4 ms when dirty);
  core0 runs USB + Lua. Events cross through a multicore-safe queue. Core1 registers for flash
  lockout, so file writes from diii are safe.
- **1200-baud touch** reboots into the UF2 bootloader (PlatformIO upload works without BOOT).
- serial mode mirrors monome's iii-grid-2022 command set (and fixes three small bugs found there).

### 1.4 Building

**PlatformIO (VS Code extension or CLI)** — the root `platformio.ini` installs pico-sdk 2.2.0, Arm
GCC 14.2, CMake and Ninja on first build:

```sh
pio run -e xiao_rp2040 -t upload      # or -e rp2040_zero
```

**Offline PlatformIO** — `xiao_rp2040_offline` / `rp2040_zero_offline` reuse the Arduino-Pico
toolchain already installed by `sw/`, a pico-sdk 2.2.0 checkout (with `lib/tinyusb`) in the
git-ignored `.deps/pico-sdk-2.2.0`, and CMake + make from `PATH`. Nothing is downloaded.

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

### M2 — Status feedback

- WS2812 status LED via PIO (RP2040-Zero GPIO16; XIAO GPIO12 with power on GPIO11): color = mode,
  blink on Lua error / out-of-memory.
- Short boot splash on the grid in iii mode only (serial mode stays dark, like today).

### M3 — Open Grid script library (replaces the old C++ MIDI-mode milestones)

Lua scripts in a new `scripts/` folder, each usable on its own and selectable from a launcher:

- **`launcher.lua`** as `init.lua`: hold a key at boot (or a corner chord) to show stored scripts on
  the grid and pick one; remembers the last choice.
- **`midi_controller.lua`**: generic MIDI controller driven by a preset file (notes, CC momentary /
  toggle, faders as columns, page keys, MIDI-in LED feedback). Preset = a small Lua/JSON table in
  flash, written by diii or a web editor (M5).
- **`keys.lua`**: Push-style isomorphic keyboard (13×8 note area + 3 control columns: octave,
  root, scale, layout, shift menu), scales from `mu`.
- **`drums.lua`**, **`step.lua`** (16 × 8), arpeggiator, Euclidean, Game of Life.
- Use existing community iii grid scripts as-is where they fit.
- **Accept:** play a soft synth in a DAW with scale/root/octave changes without touching the
  computer; switch scripts without a computer.

### M4 — Move hot Lua code to C (memory and speed)

Scripts already hit iii's memory limits (see the lines iii thread; tehn suggested moving libraries
such as MIDI to C). RAM today: ~29 KB static (incl. iii's 16 KB serial TX + 4 KB RX buffers), a
32 KB buffer allocated while uploading a file, the rest is Lua heap. Plan:

1. **Measure**: `mem()` (Lua heap in use, KB) after boot with `lib.lua` only, and with each M3
   script; log peak usage. Baseline on og-0.1.0 (XIAO, 2026-10-09):

   | State                        | `mem()`  |
   | ---------------------------- | -------- |
   | boot, `lib.lua` only         | 64.2 KB  |
   | `kria_iii.lua` v1.6.3 loaded | 147.3 KB |

   The RP2040 has 264 KB of RAM; firmware buffers and stacks take ~40–50 KB, and a file upload
   from diii needs a further 32 KB, so kria leaves only a few tens of KB of headroom.
2. Port, in order of payoff:
   - MIDI encode/decode (`msg_to_midi_tx`, `midi_to_msg`, note/cc helpers) → C functions;
   - `metro` bookkeeping (tables of metros) → C userdata;
   - `slew` (per-tick Lua work) → C;
   - `mu` scale/note tables → read-only C tables in flash (not copied into the heap).
3. Smaller wins: shrink serial TX buffer, precompile `lib.lua` to bytecode, tune Lua GC
   (`collectgarbage("generational")` / step sizes).
4. Keep the Lua-visible API identical so scripts don't change.
5. **Upstream first**: these changes belong in iii itself. Propose them on the iii thread /
   codeberg before diverging; keep `third_party/iii` unmodified and carry any interim patch as an
   explicit, documented patch file until it lands upstream.

- **Accept:** same scripts, measurably more free heap; no API changes.

### M5 — diii fork (PWA + Open Grid features)

Fork [monome/web-diii](https://github.com/monome/web-diii) (GPL-3.0, static site) to our GitHub
account and host it on GitHub Pages. Known issues to fix first:

- **PWA is broken when not hosted at the site root**: `diii.js` registers `/sw.js`, the service
  worker caches absolute `/…` paths, and the manifest uses `start_url: "/"`. Use relative paths and
  `scope: "./"`.
- Cache is cache-first with a fixed name (`diii-v1`) → users never get updates. Version the cache
  per release, use stale-while-revalidate, clean up old caches in `activate`.
- Only an SVG icon → add 192/512 px PNG and maskable icons for install prompts on all platforms.

Open Grid additions:

- Accept our own VID:PID (M6) alongside `cafe:1101`.
- **Virtual grid panel**: show the 16×8 LED state (polling `grid_led_get` or a compact dump call)
  and inject presses with `event_grid` — lets people develop scripts without keypads.
- Preset editor for `midi_controller.lua` (M3), script launcher management.
- Offer the PWA fixes upstream as a PR to monome/web-diii.

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
- **Lua memory** (M4) is the main scaling limit for big scripts.

---

## 5. Other ideas

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
