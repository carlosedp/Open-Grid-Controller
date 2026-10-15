# AGENTS.md

Guide for coding agents (and humans) working on this repository. Read this first, then
[docs/MULTIMODE-ROADMAP.md](docs/MULTIMODE-ROADMAP.md) for status and what's next.

## What this is

Open Grid Controller: a 16×8 grid (two FREEPOET 8×8 I2C keypad boards + an RP2040) that runs
**monome iii** (Lua on the device, USB MIDI + serial REPL), plus a monome-serial mode for
norns/serialosc. Scripts are managed from **diii**, a browser app (our fork lives in `diii/`,
published at https://diii.vfmod.com).

| Path                                             | What                                                                     | Rules                                     |
| ------------------------------------------------ | ------------------------------------------------------------------------ | ----------------------------------------- |
| `iii-open-grid/`                                 | our firmware (Pico SDK + CMake): device glue, keypad driver, serial mode | ours; GPL-3.0 (links iii)                 |
| `third_party/iii/`                               | monome iii, **git subtree** of codeberg.org/tehn/iii + our commits       | see "Changing iii" below                  |
| `third_party/lua`, `littlefs`, `FREEPOET_Keypad` | vendored exports                                                         | don't edit; re-export to update           |
| `diii/`                                          | web-diii fork, **git subtree** of monome/web-diii                        | plain JS (ES modules in `js/`), no build  |
| `scripts/og_bench.lua`                           | on-device benchmark                                                      | results go to `docs/BENCHMARKS.md`        |
| `sw/`                                            | original Arduino firmware (monome serial only)                           | keep as-is; known-good fallback for norns |
| `docs/`                                          | roadmap, benchmark history                                               | keep current when behavior changes        |

## Build

```sh
make help                      # all targets
make                           # firmware for the XIAO RP2040 (default BOARD)
make upload BOARD=rp2040_zero  # build + flash over USB (1200-baud touch, no BOOT button needed)
make diii                      # serve diii at http://localhost:8000 (Web Serial works on localhost)
make dist                      # UF2s for all boards + the diii site into dist/
```

- PlatformIO envs: `xiao_rp2040` and `rp2040_zero`. The first build downloads pico-sdk 2.2.0,
  Arm GCC 14.2, CMake and Ninja. Outputs in `.pio/build/<env>/firmware.uf2`.
- Measurement variants come from environment variables, not extra envs:
  `OG_FAST_CLOCK=1 pio run -e xiao_rp2040` (200 MHz), `OG_LUA_VM_IN_RAM=0` (interpreter in flash).
  They reuse the env's build directory, so build the default again afterwards.
- Plain CMake also works: `cmake -S iii-open-grid -B build -DOG_BOARD=xiao_rp2040 -DPICO_SDK_PATH=...`.
- Build options (CMake / PlatformIO `custom_og_*` / environment `OG_*`): `OG_BOARD`, `OG_I2C_HZ`, `OG_FAST_CLOCK`
  (200 MHz), `OG_LUA_VM_IN_RAM` (default ON).

A clean build with **no new warnings in our files** is the minimum bar for every change.

## Testing: the hardware loop

Agents can build but can't run anything on the device. The maintainer flashes and tests.

- Hardware today: a **Seeed XIAO RP2040 without keypads** (keypad boards and the kit's
  RP2040-Zero not tested yet). Keys can be simulated with `og_key(x,y,z)` or diii's virtual
  grid.
- For every firmware change: bump the version, build, commit, and hand over **concrete test
  steps** (exact REPL lines, what output to expect, what would indicate failure). Say clearly
  what is untested.
- Performance or memory changes: compare `og_bench()` and `og_bench("kria_iii.lua")` against the
  previous build, at the same clock, and record the lines in `docs/BENCHMARKS.md`. Change one
  thing per measurement.
- If a board stops answering: power up and press BOOT while the status LED is **cyan** (first
  second) for a clean boot that skips `init.lua`. Holding BOOT while plugging in enters the
  ROM bootloader (drag a UF2 onto the `RPI-RP2` drive).

## Conventions

- **Version**: bump `DEVICE_VERSION_BASE` in `iii-open-grid/src/main.c` and
  `pico_set_program_version` in `iii-open-grid/CMakeLists.txt` together for every firmware
  build handed out for testing (`og-X.Y.Z`; optional builds add `-200mhz` / `-vmflash`).
- **Commits**: small, one topic, imperative subject with a scope prefix (`iii:`,
  `iii-open-grid:`, `diii:`, `docs:`, `build:`, `tools:`, `scripts:`, `third_party:`). The body
  says what was wrong and why the change fixes it; include measurements when relevant. Never
  mix `third_party/iii` changes with other paths in one commit.
- **Formatting**: `.clang-format` (Google-based, 2 spaces, 100 columns) for our C code;
  `third_party/` and `diii/` keep upstream's style (formatting disabled there).
- **Comments** explain why, not what; match the surrounding code's density.
- **Docs**: when behavior changes, update the roadmap status (§1.3) and the relevant milestone;
  user-facing changes also go in `README.md`.
- **Don't push or publish** (git push, PRs, issues, forum posts) without the maintainer's OK.

## Changing iii (`third_party/iii`)

iii is a git subtree so our changes can go upstream as patches.

1. One topic per commit, touching **only** `third_party/iii`, subject `iii: ...`. Fixups for an
   earlier change say so in the body ("squash when sending upstream").
2. Keep iii's **Lua API unchanged**; existing scripts (e.g. kria_iii, many-roads) must keep
   working.
3. Nothing Open Grid-specific inside iii; it must make sense on every iii device (grids, arc,
   Pico). Board-specific code goes in `iii-open-grid/`.
4. iii's sources build with the warnings monome's device firmwares use (`-Wall -Wextra
   -Wconversion ... -Werror`, set in `iii-open-grid/CMakeLists.txt`); a change that warns there
   would break their builds.
5. Record each change and its measured effect in the table in
   [third_party/README.md](third_party/README.md).
6. `make iii-changes` lists our iii commits; `make iii-patches` exports them to
   `dist/patches/iii/` with paths relative to tehn/iii (check with `git apply --check`).
   `make iii-pull-upstream` merges new upstream releases.
7. Changing `lua/lib.lua`: regenerate `resource/lib_lua.c` with
   `python iii-open-grid/tools/embed_lua.py third_party/iii/lua/lib.lua third_party/iii/resource/lib_lua.c`
   and bump the fork version in `iii.h` **and** line 1 of `lib.lua` (`v1.1.2-ogN`, must stay
   within 24 characters). Devices only rewrite their `lib.lua` when the version string changes.
   Version-bump commits are fork-only: never send them upstream.

## Things learned the hard way

- **Code fetch from flash dominates speed.** The RP2040 runs code from flash through a 16 KB
  cache shared by both cores; the Lua interpreter loop alone is 11 KB. Moving hot code into RAM
  gave far more than rewriting Lua in C (see `docs/BENCHMARKS.md`). Benchmark numbers also shift
  with code layout, so compare several metrics.
- **The RP2040-Zero (the kit's board) runs flash at half the XIAO's speed** (board headers:
  clk/4 vs clk/2). XIAO numbers are best case.
- **Stacks:** core 0 (USB + Lua) has 8 KB (both scratch banks); core 1 runs on a 2 KB stack in
  RAM. Keep large buffers off the C stack in anything Lua calls.
- **Core 1 must not run from flash in its idle loop**, and it must keep
  `multicore_lockout_victim_init()`: iii's flash writes pause it.
- **Memory:** ~223 KB of heap. Compiling a big script is the tightest moment (kria_iii peaks at
  ~155 KB, og-0.3.3). `og_mem()` shows the C heap; Lua's `mem()` only counts Lua's share.
- **Files uploaded through the REPL end with a `\0`**; anything reading scripts must stop there.
- **Write files with `LFS_O_TRUNC`**, or a shorter new file keeps the old file's tail.
- **Serial output**: 4 KB buffer, held until a host has had the port open for 200 ms (Chrome on
  Windows drops what arrives while opening), drained synchronously when full, dropped (after one
  250 ms wait) when the host doesn't read.
- **Out of memory is a Lua error, not a reboot**, in our firmware. Scripts that relied on the
  reboot (many-roads' restart) need `device_reset()`.
- **Windows tooling:** the VS Code PlatformIO extension can hold PlatformIO's package lock for a
  long time (`LockFileTimeoutError`); CMake build dirs are per generator (Git Bash and
  PowerShell pick different ones); shell aliases may redirect `find`/`grep`/`mv` (use `command`
  or full paths).

## Licensing

Firmware built from `iii-open-grid/` is GPL-3.0 (iii); `diii/` is GPL-3.0 (web-diii). Vendored
libraries keep their own licenses (see `third_party/README.md`). Scripts without a license (e.g.
many-roads) are linked from the README, not copied into the repo. The repo has no top-level
LICENSE yet.
