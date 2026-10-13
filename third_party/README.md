# Vendored third-party code

Copied in-tree (not git submodules) so the repo builds on its own.

| Directory          | Upstream                                             | Base      | License | How it is kept                                           |
| ------------------ | ---------------------------------------------------- | --------- | ------- | -------------------------------------------------------- |
| `iii/`             | https://codeberg.org/tehn/iii                        | `ed920ef` | GPL-3.0 | **git subtree**, v1.1.2; our changes are commits on top  |
| `lua/`             | https://codeberg.org/tehn/lua                        | `86c9ee5` | MIT     | unmodified export; monome's Lua 5.4 fork (32-bit config) |
| `littlefs/`        | https://codeberg.org/tehn/littlefs                   | `e3eefe8` | BSD-3   | unmodified export; only `lfs*.{c,h}`, license and readme |
| `FREEPOET_Keypad/` | https://github.com/FREEPOET-OFFICIAL/FREEPOET_Keypad | `00893c1` | MIT     | unmodified export; register reference for our C driver   |

Not vendored: **pico-sdk 2.2.0** (large, with its own submodules). PlatformIO
installs it (`framework-picosdk`), or point CMake at a checkout with
`PICO_SDK_PATH`.

**Licensing:** iii is GPL-3.0, so firmware images built from `iii-open-grid/`
are GPL-3.0 as a whole.

## iii: local changes

`iii/` is a git subtree, so changes are ordinary commits in this repo:

- **One topic per commit**, touching only `third_party/iii`, subject prefixed
  `iii:`, so each one can go upstream as its own PR.
- **iii's Lua API stays the same**: existing scripts must run unchanged.
- **Nothing Open Grid-specific** inside iii; that lives in `iii-open-grid/`.
  Changes here should make sense on every iii device (grids, arc, Pico).
- **Measure** memory/speed changes before and after with
  `scripts/og_bench.lua`, and record the numbers in the table below.

| Make target              | What it does                                                        |
| ------------------------ | ------------------------------------------------------------------- |
| `make iii-changes`       | list our iii commits                                                |
| `make iii-patches`       | export them to `dist/patches/iii/`, with paths relative to tehn/iii |
| `make iii-pull-upstream` | merge the latest tehn/iii `main` into `third_party/iii`             |

To check a patch against upstream: `git -C <tehn/iii clone> apply --check <patch>`.

| Commit               | Change                                                           | Why / measured effect                                                                                                                                                                                   | Upstream  |
| -------------------- | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------- |
| `e7107bf`            | `serial()`: don't write past the TX buffer                       | fixes RAM corruption when >16 KB is printed in one Lua call                                                                                                                                             | not sent  |
| `3ee8dd2`            | stream Lua files into the compiler                               | no full source copy while loading: kria_iii (29 KB) now loads after og_bench's tests, was out of memory; fixups `9340efe` (stop at the NUL uploads end with), `f47cc2c` (no big buffers on the C stack) | not sent  |
| `b45e254`            | stream uploads to flash, no 32 KB buffer                         | heap high water after uploading 4 files: 168 KB (was 197 KB after one upload, of 221 KB); no 32 KB script limit                                                                                         | not sent  |
| `f8be883`            | no reboot on a failed allocation                                 | og_bench with kria_iii loaded no longer reboots the device (og-0.2.5 did); a real OOM is a script error, panics still reboot                                                                            | not sent  |
| `e537ece`            | GC pause 120% instead of 200%                                    | collect before garbage can outgrow RAM; no measurable speed cost (midi_to_msg 81 µs, redraw 1.04 ms)                                                                                                    | not sent  |
| `289097f`            | name the file in upload compile errors                           | no NULL+3 crash on messages without a quote; verified: `bogus.lua:3: unexpected symbol`                                                                                                                 | not sent  |
| `bd89884`            | MIDI helpers in C (fixup `5dd8ec6`)                              | same results as the Lua versions; no measurable gain on its own (code fetch dominates)                                                                                                                  | not sent  |
| `fb29f50`            | terminate the lib.lua version buffer                             | strstr() read past a 24-byte stack buffer                                                                                                                                                               | not sent  |
| `e9d184b`            | truncate lib.lua when rewriting it                               | a shorter new lib.lua kept the old tail and failed to compile (nothing in lib.lua ran)                                                                                                                  | not sent  |
| `9d0a494`            | 4 KB serial TX that drains when full; 512 B RX (fixup `8ac65d0`) | −15.5 KB static RAM; prints and downloads of any size get through; a host that never reads costs one 250 ms wait                                                                                        | not sent  |
| `e4e1b89`            | keep output printed before a host connects (fixup `9dabcc7`)     | startup prints (e.g. many-roads' script list, `-- init:` lines) show up when diii connects                                                                                                              | not sent  |
| `262253f`, `08e814d` | lib.lua version `v1.1.2-ogN`                                     | fork-only, so devices replace their lib.lua; don't send                                                                                                                                                 | fork only |

`make iii-patches` exports every iii commit since the subtree was added. When a
change lands upstream, mark it *merged* here; to leave it out of later
exports, pass a newer base, e.g. `make iii-patches III_BASE=<last merged commit>`.

## Other directories

`lua/`, `littlefs/` and `FREEPOET_Keypad/` are plain exports. To update one,
re-export it at a new commit and update the table (run from the repo root, in a
clone of the upstream repo at `$UP`):

```sh
rm -rf third_party/lua && mkdir third_party/lua
git -C "$UP" archive <commit> | tar -x -C third_party/lua
```

If one of them ever needs local changes, convert it to a subtree the way
`iii/` was (commit `337d710`).
