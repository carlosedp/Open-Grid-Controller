# Open Grid Controller

Open Grid Controller is an open-source 8x16 grid controller for music production, live performance, sound experiments, and interactive installations. It uses an RP2040-Zero as the main controller, appears over USB as a monome grid-compatible device, and bridges 128 key inputs plus per-key LED brightness control to two 8x8 I2C keypad/LED boards.

The project includes ready-to-build PlatformIO firmware, STEP files for the 3D-printed enclosure, and reference photos for assembly. You can use it as a complete DIY controller, or adapt it into your own sequencer, visual controller, media installation input panel, or monome-compatible hardware project.

![Open Grid front view](pics/front.jpg)

## Features

- 8x16 matrix layout with 128 independently readable keys.
- Per-key LED brightness control with 16 levels, from 0 to 15.
- USB serial protocol compatible with the core monome grid input and LED control workflow.
- Based on RP2040-Zero, Arduino Core, and Adafruit TinyUSB.
- Two 8x8 [FREEPOET-Keypad](https://github.com/FREEPOET-OFFICIAL/FREEPOET_Keypad) I2C boards combine into a full 16-column grid.
- Supports key debouncing, hot-plug detection, full LED sync, and incremental LED refresh.
- Includes 3D-printable enclosure files for replication and customization.

## Product Photos

The front panel uses a classic 16-column, 8-row grid layout, suitable for step sequencing, clip launching, parameter mapping, and mode switching.

![Open Grid side view](pics/side.jpg)

The enclosure has a low-profile desktop form factor with a side USB port, making it comfortable to keep beside a synthesizer, controller, or computer.

![Open Grid with Shield Pro](pics/OpenGridAndShieldPro.jpg)

Open Grid can be used alongside other hardware controllers. The photo above shows it working with Shield Pro on the same desktop, forming a more complete live performance control setup.

## Hardware

![Open Grid internal assembly](pics/installation.jpg)

## Bill of Materials

FREEPOET OpenGrid kit: <https://github.com/FREEPOET-OFFICIAL/FREEPOET_Keypad>

- RP2040-Zero main controller board.
- Two 8x8 FREEPOET_Keypad key/LED boards.
- Eight 4x4 silicone pads.

Parts to prepare separately:

- 3D-printed top cover and bottom shell.
- 4 wires.
- 24pcs M2\*6 mm self-tapping screws.
- 8pcs M2\*10 mm self-tapping screws.
- 6pcs M3\*8 mm self-tapping screws.

The firmware uses Pico I2C0 by default:

- SDA: GPIO 4
- SCL: GPIO 5
- I2C speed: 100 kHz

Select the address jumpers as shown by the red circles in the image below:

- Board 1: solder the address bit 0 jumper.
- Board 2: solder the address bit 1 jumper.

![Address jumper selection](pics/address.png)

## Repository Structure

```text
.
├── enclosure/              # 3D enclosure STEP files
│   ├── OpenGrid-Bot.step
│   └── OpenGrid-Top.step
├── diii/                   # diii web app (fork of monome/web-diii, git subtree)
├── docs/                   # Roadmap and design notes
├── iii-open-grid/          # iii (Lua) firmware, built from the root platformio.ini
├── pics/                   # README and assembly reference images
├── third_party/            # Vendored iii, Lua, littlefs and FREEPOET_Keypad sources
├── tools/                  # Build helpers (dist.py)
├── Makefile                # Build, flash and package everything (make help)
├── platformio.ini          # PlatformIO project for iii-open-grid
└── sw/                     # Original Arduino firmware (PlatformIO project)
    ├── platformio.ini
    └── src/
```

## Firmware Build and Upload

This section covers the original Arduino firmware in `sw/`, which only has the monome serial protocol. For the iii firmware, which has both modes, see [iii Firmware](#iii-firmware-experimental). The Arduino firmware is built with PlatformIO. The target board is Raspberry Pi Pico.

1. Install VS Code and the PlatformIO extension.
2. Open the `sw` directory in this repository.
3. Connect the RP2040-Zero.
4. Build and upload the firmware:

```bash
cd sw
pio run -t upload
```

## iii Firmware (experimental)

`iii-open-grid/` is a second firmware based on monome's [iii](https://monome.org/docs/iii/): Lua scripts run on the grid itself, it shows up as a USB MIDI device, and scripts are written and uploaded from the browser with [diii](https://monome.org/diii). It also keeps a monome serial mode for norns and serialosc.

### Two modes: iii and monome serial

The iii firmware contains both modes. The grid stays in the last one you chose, even after unplugging.

| Mode              | Status LED | Shows up as                                            | Use it with                                                                                                                                                                                                |
| ----------------- | ---------- | ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **iii** (default) | green      | USB MIDI + serial REPL (`Open Grid` / `iii open grid`) | [diii](https://diii.vfmod.com) to write and upload Lua scripts; scripts then play any DAW or synth over USB MIDI, no computer software needed                                                              |
| **monome serial** | blue       | a monome grid (`monome` / `grid`)                      | **norns** directly (plug into its USB port), and **Max/MSP, Pure Data, SuperCollider, VCV Rack, TouchDesigner**, etc. on a computer through monome's [serialosc](https://monome.org/docs/serialosc/setup/) |

To switch modes, use any of these:

- **Top-left key**: hold it while plugging in. This toggles iii ↔ serial. When switching to iii, keep holding for about 1 second (the top row fills up) to also skip `init.lua`.
- **BOOT button**: hold it for 2 seconds while the grid is running (the LED turns magenta). This toggles the mode and restarts. It works without keypads.
- **From diii**: type `og_set_mode(1)` for monome serial, or `og_set_mode(0)` for iii. Use this only while in iii mode, because serial mode has no REPL.
- **Back to iii, skipping `init.lua`**: press BOOT while the LED is cyan, during the first second after plugging in. The LED turns amber. Use this to recover from a script that hangs at startup.

Holding BOOT _while_ plugging in does something else: it enters the RP2040's bootloader (an `RPI-RP2` drive) for flashing a firmware.

Serial mode follows the command set of monome's own grid firmware. It hasn't been tested on norns or serialosc yet. Until it has, the Arduino firmware in `sw/` (see [Firmware Build and Upload](#firmware-build-and-upload)) is the known-good choice for norns.

If the grid stops responding for 8 seconds (a script stuck in a loop, or a firmware hang), a watchdog resets it and it starts again with `init.lua`, so a performance setup comes back on its own. If it hangs again within 30 seconds of starting, the next start skips `init.lua` (status LED amber) so you can fix the script from diii. Either way diii shows a `watchdog:` line once it connects (og-0.3.10 or later).

Everything is driven from the root `Makefile` (needs `make`, `python` and PlatformIO's `pio`). Run `make help` for the full list:

```bash
make upload                    # build and flash the XIAO RP2040 (default BOARD)
make upload BOARD=rp2040_zero  # build and flash the Open Grid kit (RP2040-Zero)
make diii                      # serve the diii web app at http://localhost:8000 (needs Node)
make dist                      # UF2s for all boards + the diii web app into dist/
```

You can also open the repository root in VS Code with the PlatformIO extension and use its Build/Upload buttons.

The first build downloads pico-sdk 2.2.0, the Arm toolchain, CMake and Ninja through PlatformIO (about 300 MB).

### diii

`diii/` is our fork of monome's [web-diii](https://github.com/monome/web-diii) (GPL-3.0), kept as a git subtree of [carlosedp/web-diii](https://github.com/carlosedp/web-diii). On top of upstream, it fixes installing diii as an app (PWA) from any path. It also adds a **grid** drawer at the top of the console for the Open Grid firmware: it shows the grid's LEDs live and lets you press keys from the browser, so scripts can be tried without keypads attached. **edit** in a file's menu opens it in a code editor next to the console (save with Ctrl+S, save as, run, with autocomplete and help for iii functions); Tab completes names in the REPL; **+** creates a new script and **rename** renames one. **backup** downloads every file on the device as one zip, and **restore** uploads a backup's files again (byte-exact backups need og-0.3.4 or later). The **midi** drawer above the buttons shows the MIDI the device sends and receives (og-0.3.8 or later). The status shows the firmware version, and **device ▾ → firmware** checks for a newer release and walks through updating. When a newer diii is published at [diii.vfmod.com](https://diii.vfmod.com), an **update ready** button appears in the header to load it. `make diii-pull-upstream` merges monome's latest changes into `diii/`, and `make diii-push-fork` pushes `diii/` history to the fork, which is useful for pull requests to monome.

#### diii with monome's own firmware

diii works with any iii device (monome grids and arcs with their stock iii firmware, too). The extras that need the Open Grid firmware hide themselves or say so:

|                                                                                               | Stock iii firmware                                             | Open Grid firmware                               |
| --------------------------------------------------------------------------------------------- | -------------------------------------------------------------- | ------------------------------------------------ |
| REPL, history, Tab completion, uploads (several at once), run, new, delete, first / not first | ✅                                                             | ✅                                               |
| Editor: autocomplete, hover help, reference, drafts, clickable errors, line wrap              | ✅                                                             | ✅                                               |
| Read, edit, download, backup: files come back exactly (blank lines and all)                   | ✅ slower: read in 3 KB pieces                                 | ✅ fast (`og_dump_file`, og-0.3.4+)              |
| Rename                                                                                        | ✅ copy, then delete                                           | ✅ one atomic step (`og_rename_file`, og-0.3.5+) |
| Save from the editor with a syntax error                                                      | ⚠ the device's copy is deleted (your text stays in the editor) | ✅ the device keeps the old file                 |
| Scripts bigger than 32 KB                                                                     | ❌ stock iii's upload limit                                    | ✅                                               |
| Memory in the explorer footer                                                                 | Lua's share only                                               | ✅ Lua and the C heap (`og_mem`)                 |
| Virtual grid drawer (LEDs, press keys from the browser)                                       | ❌ hidden                                                      | ✅ (`og_mirror`, `og_key`)                       |
| MIDI monitor list                                                                             | ❌ says it needs og-0.3.8                                      | ✅ og-0.3.8+                                     |
| MIDI counts and clock tempo with the drawer closed                                            | ❌                                                             | ✅ og-0.3.9+                                     |
| Firmware update dialog                                                                        | shows the version; no Open Grid updates offered                | ✅ newest release for your board                 |

The ⚠ and ❌ rows on stock firmware are limits of iii itself; the fixes are in our [iii pull requests](https://codeberg.org/tehn/iii/pulls) (#10–#12).

The iii firmware is GPL-3.0 (inherited from iii). So far it has been tested on a Seeed XIAO RP2040 without keypads: diii, USB MIDI, and switching modes. It has not yet been tested with the keypad boards or on norns. See [docs/MULTIMODE-ROADMAP.md](docs/MULTIMODE-ROADMAP.md) for status and plans, and [third_party/README.md](third_party/README.md) for the vendored libraries.

### Recommended iii Scripts

Scripts written for monome's iii grids run unchanged on the Open Grid. Upload them with [diii](https://monome.org/diii).

| Script                                                         | What it does                                                                                                                                                                                                                                                                                                                                      |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [many-roads](https://github.com/boreal-ground/many-roads-grid) | Script launcher. On startup it lights one key per script stored on the grid; press a key to run that script. Triple-click the top-left key to restart and return to the launcher (but see the note below). To install, upload `many-roads.lua` and `many-roads-data.lua` and set `many-roads.lua` as `first` in diii. Tested at commit `691af85`. |

**many-roads restart on this firmware:** many-roads 1.2.1 restarts by deliberately running out of memory, relying on iii rebooting when that happens. This firmware instead lets Lua recover from out-of-memory (see [monome/iii#7](https://github.com/monome/iii/issues/7)), so the triple-click only prints `not enough memory`. A fix that uses iii's `device_reset()` is being proposed to the many-roads authors. Until it's released, type `^^i` in diii (or replug the grid) to get back to the launcher.

Scripts are listed by link only. Their authors hold the copyright, and we only copy a script into this repository when its license allows redistribution.

## Software Compatibility

With the iii firmware, iii mode works with diii and anything that takes USB MIDI, and monome serial mode works with norns and serialosc (see [Two modes](#two-modes-iii-and-monome-serial)). The rest of this section describes the Arduino firmware in `sw/`.

On the USB side, the Arduino firmware uses a monome-style serial device descriptor and reports `OpenGrid` as the device ID. The firmware implements common commands such as grid size query, device ID handling, key event reporting, and single-pixel, region, and full-grid LED control. It is intended to work with music software, scripts, or custom toolchains that support monome grid devices.

The current firmware focuses on providing a stable 8x16 grid interaction layer. If your software depends on more complete monome protocol behavior, test it against your target application and extend `sw/src/MonomeSerialDevice.cpp` as needed.

## Enclosure and Assembly

The enclosure STEP files are in `enclosure/`:

- `OpenGrid-Top.step`: top cover.
- `OpenGrid-Bot.step`: bottom shell.

## Use Cases

- Sequencer and rhythm programming controller.
- Grid input panel for Max/MSP, norns, SuperCollider, Pure Data, and similar creative environments.
- Lighting, visual, and media installation control surface.
- monome protocol learning and open-source hardware experimentation.
- Hardware foundation for custom MIDI, OSC, or USB controllers.
