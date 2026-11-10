# parterre

<img src="docs/brand/parterre-logo.svg" alt="" width="96" align="right">

**parterre** is a grid controller of 128 lit keys that runs Lua scripts on the
device and plays norns as a monome-compatible grid. It is the firmware and
tools in this repository: monome's [iii](https://monome.org/docs/iii/) on an
RP2040, with USB MIDI, a monome serial mode, a watchdog, firmware updates from
the browser and a DIY MIDI out, written and patched from
[greenhouse](https://greenhouse.vfmod.com).

It runs on **FREEPOET's [Open Grid](https://github.com/FreepoetOfficial/Open-Grid-Controller)
kit** today: two 8×8 FREEPOET keypad boards, an RP2040-Zero and a printed case.
This repository started as a fork of theirs, and their hardware build is kept
below. The firmware reads the keypads through a small layer of its own, so
other keypad boards (other sizes, or ones such as denki-oto's mechatrellis)
could become further backends.

- Page: [vfmod.com/parterre](https://vfmod.com/parterre/), by Voltage Foundry Modular
- Manual: [docs/manual/Manual.md](docs/manual/Manual.md)
- Firmware: the [releases](../../releases) (a UF2 file for each board)

It was called Open Grid Controller until November 2026. Code names stay as
they were, so scripts keep working: the `og_*` Lua tools, the
`iii-open-grid/` folder and the `og-x.y.z` versions. From og-0.3.14 the device
shows up on USB as **parterre** (Voltage Foundry Modular) in iii mode; serial
mode keeps monome's names, which norns and serialosc look for.

## What parterre adds to the kit

- **Lua scripts on the grid** (monome's iii): the grid plays on its own, from a
  power bank, with no computer; scripts for monome's iii grids run unchanged.
- **Two modes in one firmware**: iii, and monome serial for norns and
  serialosc, switched from the grid itself.
- **greenhouse in the browser**: a console, an editor, a live view of the grid
  that mirrors its LEDs and plays its keys, **graft** (a patch editor that
  wires grid widgets into a script without code), backups and firmware
  updates.
- **A watchdog** that restarts a hung script, and skips the start-up script
  after a crash loop.
- **TRS MIDI out** (DIY): play hardware synths with a jack and two resistors.
- **Boards**: the kit's RP2040-Zero, and a Seeed XIAO RP2040 for a breadboard.

**Status: in development.** Tested on a Seeed XIAO RP2040 without keypads:
the scripts, greenhouse, USB MIDI and switching modes. Testing with the keypad
boards and on norns is next; see
[docs/MULTIMODE-ROADMAP.md](docs/MULTIMODE-ROADMAP.md).

## Two modes: iii and monome serial

The iii firmware contains both modes. The grid stays in the last one you chose, even after unplugging.

| Mode              | Status LED | Shows up as                                            | Use it with                                                                                                                                                                                                |
| ----------------- | ---------- | ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **iii** (default) | green      | USB MIDI + serial REPL (`Open Grid` / `iii open grid`) | [greenhouse](https://greenhouse.vfmod.com) to write and upload Lua scripts; scripts then play any DAW or synth over USB MIDI, no computer software needed                                                              |
| **monome serial** | blue       | a monome grid (`monome` / `grid`)                      | **norns** directly (plug into its USB port), and **Max/MSP, Pure Data, SuperCollider, VCV Rack, TouchDesigner**, etc. on a computer through monome's [serialosc](https://monome.org/docs/serialosc/setup/) |

To switch modes, use any of these:

- **Top-left key**: hold it while plugging in. This toggles iii ↔ serial. When switching to iii, keep holding for about 1 second (the top row fills up) to also skip `init.lua`.
- **BOOT button**: hold it for 2 seconds while the grid is running (the LED turns magenta). This toggles the mode and restarts. It works without keypads.
- **From greenhouse**: type `og_set_mode(1)` for monome serial, or `og_set_mode(0)` for iii. Use this only while in iii mode, because serial mode has no REPL.
- **Back to iii, skipping `init.lua`**: press BOOT while the LED is cyan, during the first second after plugging in. The LED turns amber. Use this to recover from a script that hangs at startup.

Holding BOOT _while_ plugging in does something else: it enters the RP2040's bootloader (an `RPI-RP2` drive) for flashing a firmware.

Serial mode follows the command set of monome's own grid firmware. It hasn't been tested on norns or serialosc yet. Until it has, the Arduino firmware in `sw/` (see [the original Arduino firmware](#the-original-arduino-firmware)) is the known-good choice for norns.

If the grid stops responding for 8 seconds (a script stuck in a loop, or a firmware hang), a watchdog resets it and it starts again with `init.lua`, so a performance setup comes back on its own. If it hangs again within 30 seconds of starting, the next start skips `init.lua` (status LED amber) so you can fix the script from greenhouse. Either way greenhouse shows a `watchdog:` line once it connects (og-0.3.10 or later).

## Building and flashing

Everything is driven from the root `Makefile` (needs `make`, `python` and PlatformIO's `pio`). Run `make help` for the full list:

```bash
make upload                    # build and flash the XIAO RP2040 (default BOARD)
make upload BOARD=rp2040_zero  # build and flash the Open Grid kit (RP2040-Zero)
make diii                      # serve a greenhouse checkout (DIII_DIR, default ../web-diii) at http://localhost:8000
make dist                      # UF2s for all boards into dist/
```

You can also open the repository root in VS Code with the PlatformIO extension and use its Build/Upload buttons.

The first build downloads pico-sdk 2.2.0, the Arm toolchain, CMake and Ninja through PlatformIO (about 300 MB).

## TRS MIDI out (DIY, og-0.3.13 or later)

In iii mode, every MIDI message a script sends over USB also goes out GPIO0 (XIAO pin D6) at MIDI speed, so the grid can play hardware with only a USB power bank. Add a 3.5 mm stereo jack and two resistors, wired as TRS type B (Polyend's gear, such as the Tracker Mini, uses type B, so a plain stereo cable connects them):

- tip ← 33 Ω ← 3V3
- ring ← 10 Ω ← GPIO0
- sleeve → GND

For type A gear, swap tip and ring, or use an A/B adapter. In the printed enclosure, a low jack (about 5 mm tall) fits on the USB end next to the RP2040-Zero, with its hole centered on the seam between the top and bottom parts. It's out only; MIDI in stays on USB. This hasn't been tested with hardware yet.

## greenhouse

[greenhouse](https://greenhouse.vfmod.com) (diii until November 2026) is our
fork of monome's [web-diii](https://github.com/monome/web-diii) (GPL-3.0), in
its own repository, [carlosedp/greenhouse](https://github.com/carlosedp/greenhouse).
It works with any iii device, and with parterre it adds a **grid** drawer that
shows the grid's LEDs live and plays its keys from the browser (so scripts can
be tried without keypads), **graft**'s grid widgets, a MIDI monitor and a
firmware dialog that offers this repository's releases. Until November 2026 it
lived here as a git subtree at `diii/`.

## Recommended iii scripts

Scripts written for monome's iii grids run unchanged on parterre. Upload them with [greenhouse](https://greenhouse.vfmod.com).

| Script                                                         | What it does                                                                                                                                                                                                                                                                                                                                      |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [many-roads](https://github.com/boreal-ground/many-roads-grid) | Script launcher. On startup it lights one key per script stored on the grid; press a key to run that script. Triple-click the top-left key to restart and return to the launcher (but see the note below). To install, upload `many-roads.lua` and `many-roads-data.lua` and set `many-roads.lua` as `first` in greenhouse. Tested at commit `691af85`. |

**many-roads restart on this firmware:** many-roads 1.2.1 restarts by deliberately running out of memory, relying on iii rebooting when that happens. This firmware instead lets Lua recover from out-of-memory (see [monome/iii#7](https://github.com/monome/iii/issues/7)), so the triple-click only prints `not enough memory`. A fix that uses iii's `device_reset()` is being proposed to the many-roads authors. Until it's released, type `^^i` in greenhouse (or replug the grid) to get back to the launcher.

Scripts are listed by link only. Their authors hold the copyright, and we only copy a script into this repository when its license allows redistribution.

## Hardware: FREEPOET's Open Grid kit

Everything in this section is FREEPOET's design and text, from their
[Open-Grid-Controller](https://github.com/FreepoetOfficial/Open-Grid-Controller)
repository: the keypad boards, the enclosure, the photos and the original
Arduino firmware in `sw/`. parterre runs on this hardware unchanged.

> Open Grid Controller is an open-source 8x16 grid controller for music
> production, live performance, sound experiments, and interactive
> installations. It uses an RP2040-Zero as the main controller, appears over
> USB as a monome grid-compatible device, and bridges 128 key inputs plus
> per-key LED brightness control to two 8x8 I2C keypad/LED boards.

![Open Grid front view](pics/front.jpg)

### Features

- 8x16 matrix layout with 128 independently readable keys.
- Per-key LED brightness control with 16 levels, from 0 to 15.
- USB serial protocol compatible with the core monome grid input and LED control workflow.
- Based on RP2040-Zero, Arduino Core, and Adafruit TinyUSB.
- Two 8x8 [FREEPOET-Keypad](https://github.com/FREEPOET-OFFICIAL/FREEPOET_Keypad) I2C boards combine into a full 16-column grid.
- Supports key debouncing, hot-plug detection, full LED sync, and incremental LED refresh.
- Includes 3D-printable enclosure files for replication and customization.

### Product Photos

The front panel uses a classic 16-column, 8-row grid layout, suitable for step sequencing, clip launching, parameter mapping, and mode switching.

![Open Grid side view](pics/side.jpg)

The enclosure has a low-profile desktop form factor with a side USB port, making it comfortable to keep beside a synthesizer, controller, or computer.

![Open Grid with Shield Pro](pics/OpenGridAndShieldPro.jpg)

Open Grid can be used alongside other hardware controllers. The photo above shows it working with Shield Pro on the same desktop, forming a more complete live performance control setup.

### Hardware

![Open Grid internal assembly](pics/installation.jpg)

### Bill of Materials

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

### Enclosure and Assembly

The enclosure STEP files are in `enclosure/`:

- `OpenGrid-Top.step`: top cover.
- `OpenGrid-Bot.step`: bottom shell.

### Use Cases

- Sequencer and rhythm programming controller.
- Grid input panel for Max/MSP, norns, SuperCollider, Pure Data, and similar creative environments.
- Lighting, visual, and media installation control surface.
- monome protocol learning and open-source hardware experimentation.
- Hardware foundation for custom MIDI, OSC, or USB controllers.

### The original Arduino firmware

FREEPOET's original firmware is in `sw/`. It has only the monome serial protocol; parterre has both modes (see [Two modes](#two-modes-iii-and-monome-serial)). The Arduino firmware is built with PlatformIO. The target board is Raspberry Pi Pico.

1. Install VS Code and the PlatformIO extension.
2. Open the `sw` directory in this repository.
3. Connect the RP2040-Zero.
4. Build and upload the firmware:

```bash
cd sw
pio run -t upload
```

On the USB side, the Arduino firmware uses a monome-style serial device descriptor and reports `OpenGrid` as the device ID. The firmware implements common commands such as grid size query, device ID handling, key event reporting, and single-pixel, region, and full-grid LED control. It is intended to work with music software, scripts, or custom toolchains that support monome grid devices.

The current firmware focuses on providing a stable 8x16 grid interaction layer. If your software depends on more complete monome protocol behavior, test it against your target application and extend `sw/src/MonomeSerialDevice.cpp` as needed.

## Repository structure

```text
.
├── iii-open-grid/          # parterre: the iii (Lua) firmware, built from the root platformio.ini
├── docs/                   # the manual, roadmap, design notes and icons (docs/brand/)
├── third_party/            # vendored iii, Lua, littlefs and FREEPOET_Keypad sources
├── tools/                  # build helpers (dist.py)
├── Makefile                # build, flash and package everything (make help)
├── platformio.ini          # PlatformIO project for iii-open-grid
├── enclosure/              # FREEPOET: 3D enclosure STEP files
├── pics/                   # FREEPOET: photos and assembly reference images
└── sw/                     # FREEPOET: the original Arduino firmware (PlatformIO project)
```

## License and credits

GPL-3.0 (see [LICENSE](LICENSE)). parterre's firmware is built on monome's
[iii](https://github.com/monome/iii) (GPL-3.0) and is by Carlos de Paula,
published by Voltage Foundry Modular. The Open Grid kit, its keypad boards,
enclosure, photos and original firmware are by
[FREEPOET](https://github.com/FREEPOET-OFFICIAL).
