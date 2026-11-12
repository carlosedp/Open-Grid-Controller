# Names and compatibility

What each name means, and what works with what between parterre, monome's
own firmware, greenhouse and monome's diii. Checked in the sources in
November 2026 (parterre 0.4.0, iii v1.1.2, greenhouse `main`).

## The names

| Name                     | What it is                                                                                           | Whose                                                                           |
| ------------------------ | ---------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| **Open Grid**            | hardware: FREEPOET's kit (two 8×8 I2C keypad boards, an RP2040-Zero, a printed case)                 | [FREEPOET](https://github.com/FreepoetOfficial/Open-Grid-Controller)            |
| **grid 2022, one, zero** | hardware: monome's grids that run iii                                                                | [monome](https://monome.org/docs/grid/editions)                                 |
| **iii**                  | the runtime ("operating system"): Lua on the device, REPL, files, USB MIDI, the `^^` serial protocol | monome ([tehn/iii](https://codeberg.org/tehn/iii))                              |
| **parterre**             | a firmware: iii (with our patches) + a board's hardware layer + extras; one build per board          | this repository                                                                 |
| `parterre-X.Y.Z`         | parterre's version (`og-X.Y.Z` until 0.3.16); the UF2 is `parterre-X.Y.Z-<board>.uf2`                |                                                                                 |
| `pt_*`                   | parterre's own Lua calls (mode, status LED, keypads)                                                 |                                                                                 |
| `gh_*`                   | the Lua calls greenhouse uses (grid view, backup, rename, memory, MIDI monitor, board)               | any firmware can offer them (achene does, as `og_*` for now)                    |
| `og_*`                   | the names of both before 0.4; still work on parterre                                                 |                                                                                 |
| **greenhouse**           | the browser app (a fork of monome's diii)                                                            | [VoltageFoundryMod/greenhouse](https://github.com/VoltageFoundryMod/greenhouse) |
| **diii**                 | monome's browser app                                                                                 | [monome](https://monome.org/docs/iii/diii)                                      |
| **monome serial**        | the grid protocol norns and serialosc speak; a mode of both parterre and monome's iii firmware       | monome                                                                          |

The C sources keep `og_` identifiers inside; they don't show anywhere a user
or a script sees.

## What works with what

**Stock diii with parterre: works, without the extras.** parterre's iii mode
uses the same USB ID as monome's grids (`cafe:1101`, the only one diii lists)
and the same `^^` protocol. Its extra output lines (`^^grid`, `^^midi`) only
start when greenhouse asks for them. diii's uploads (delete, then write) work,
also for files over 32 KB.

**greenhouse with monome's stock grids (and arcs): works, minus parterre's
extras.** greenhouse lists the same `cafe:1101` and checks for each extra
before using it:

| On stock iii                             | On parterre                                    |
| ---------------------------------------- | ---------------------------------------------- |
| REPL, editor, reference, graft, uploads  | the same                                       |
| backup and reading files: slower, exact  | fast (`gh_dump_file`)                          |
| rename: copy, then delete                | one atomic step (`gh_rename_file`)             |
| a failed save deletes the device's copy  | the device keeps the old file                  |
| no files over 32 KB                      | any size                                       |
| no grid view, no MIDI monitor            | both (`gh_mirror`/`gh_key`, `gh_midi_monitor`) |
| firmware dialog: links to monome's pages | the newest parterre release for the board      |

The first four rows on stock are limits of iii itself; the fixes are offered
upstream ([tehn/iii](https://codeberg.org/tehn/iii/pulls) #10–#12).
greenhouse never offers a parterre UF2 to a device on other firmware: one built
for another board would install and then not work.

**Scripts.** Scripts for monome's iii grids run on parterre unchanged: same
grid API, and `device_id()` is `"grid"`. Two differences: out of memory is a
Lua error on parterre instead of a reboot (scripts that relied on the reboot,
like many-roads before v1.3.0, need `device_reset()`), and bigger scripts fit.
Scripts that use `pt_*` or `gh_*` run on stock firmware only if they check
first (`if gh_mem then ... end`).

**norns and serialosc** see a monome grid in serial mode on either firmware;
nothing in serial mode is parterre-specific.

## Moving between monome's firmware and parterre

Either way, **the scripts on the grid are erased**: parterre keeps its files
at a fixed place in flash (so its own updates keep them), monome's firmware
right after the program, and iii formats the file system when it doesn't find
one. Back up first (greenhouse: device ▾ → backup) and restore afterwards
(device ▾ → restore).

- **To parterre** (once there are builds for monome's grids): download the
  UF2 for your grid from [parterre's releases](https://github.com/VoltageFoundryMod/parterre/releases),
  put the grid in bootloader mode (greenhouse: device ▾ → bootloader; or as in
  monome's [instructions](https://monome.org/docs/iii/)), and drag the UF2
  onto the drive that shows up. greenhouse offers parterre's updates from then on.
- **Back to monome's firmware**: `iii-grid-2022.uf2`, `iii-grid-one.uf2` or
  `iii-grid-zero.uf2` from [monome/iii releases](https://github.com/monome/iii/releases/latest),
  the same way. It has both iii and the monome serial mode.

parterre is unofficial: not made, tested or supported by monome.
