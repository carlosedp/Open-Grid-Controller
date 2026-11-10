# parterre

A grid controller of 128 lit keys that runs Lua scripts on the device, and
also plays norns and serialosc as a monome-compatible grid. parterre is the
firmware and its tools: it runs on an RP2040 and today drives
[FREEPOET's Open Grid kit](https://github.com/FREEPOET-OFFICIAL/FREEPOET_Keypad).
Other keypad boards can become its backends later.

![parterre](images/Front.png)

> **Status: in development.** The firmware runs and is tested on a Seeed XIAO
> RP2040 without keypads (the scripts, USB MIDI, switching modes); testing
> with the keypad boards and on norns is next. It was called Open Grid
> Controller until November 2026; from firmware og-0.3.14 it shows up on USB
> as parterre.

## The hardware

parterre runs on FREEPOET's Open Grid kit: two 8×8 FREEPOET keypad boards
side by side make a 16×8 grid, with an LED under each key (16 levels of
brightness), read by an RP2040-Zero, in a 3D-printed low-profile case. The
kit, its boards and its case are FREEPOET's design; see their
[repository](https://github.com/FreepoetOfficial/Open-Grid-Controller) for
the parts and the assembly.

The firmware reads the keypads through a small layer of its own, so other
keypad hardware (other sizes, or boards such as denki-oto's mechatrellis)
could become further backends.

## Two modes

parterre has two modes and stays in the last one you chose, even unplugged.

| mode              | status LED | shows up as                     | use it with                                                                                                                               |
| ----------------- | ---------- | ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| **iii** (default) | green      | USB MIDI and a serial console   | [greenhouse](https://greenhouse.vfmod.com) to write and upload Lua scripts; the scripts then play any DAW or synth over USB MIDI on their own |
| **monome serial** | blue       | a monome grid                   | **norns** directly, and Max/MSP, Pure Data, SuperCollider, VCV Rack or TouchDesigner on a computer through monome's serialosc                  |

To switch:

- **Top-left key**: hold it while plugging in. When switching to iii, keep
  holding for about a second (the top row fills up) to also skip the
  start-up script.
- **BOOT button**: hold it for 2 seconds while running (the LED turns
  magenta); it switches and restarts.
- **From greenhouse**: type `og_set_mode(1)` for monome serial, `og_set_mode(0)`
  for iii.

## Scripts in iii mode

In iii mode the grid runs monome's [iii](https://monome.org/docs/iii/): Lua
scripts stored on the device decide what each key does and what lights up.
Scripts written for monome's own iii grids run unchanged.

Open [greenhouse.vfmod.com](https://greenhouse.vfmod.com) in Chrome, Edge or
Opera and connect over USB. greenhouse has a console that talks to the grid
live, an editor, a view of the grid that mirrors its LEDs and plays its keys
from the screen, a MIDI monitor, and **graft**, a patch editor that wires grid
widgets (buttons, faders, a step sequencer, a keyboard, ripples) into a
script without writing code.

A good first script is [many-roads](https://github.com/boreal-ground/many-roads-grid),
a launcher that lights one key per script on the grid.

## When something goes wrong

- **A script that hangs:** if the grid stops responding for 8 seconds, a
  watchdog restarts it. If it hangs again within 30 seconds, the next start
  skips the start-up script (status LED amber) so you can fix it from
  greenhouse, which shows a `watchdog:` line when it connects.
- **Skip the start-up script yourself:** press BOOT while the LED is cyan,
  during the first second after plugging in (the LED turns amber).

## MIDI to hardware (DIY)

Every MIDI message a script sends over USB also goes out a pin at MIDI speed,
so with a 3.5 mm TRS jack and two resistors the grid can play a synth from a
USB power bank, no computer. The wiring is in the
[repository's README](https://github.com/VoltageFoundryMod/parterre#trs-midi-out-diy-og-0313-or-later).

## Updating the firmware

When the grid connects, greenhouse checks for a newer firmware release and
offers it. To install one: hold BOOT while plugging in, so the
grid shows up as an `RPI-RP2` drive, and drag the `.uf2` file for your board
from the [releases](https://github.com/VoltageFoundryMod/parterre/releases)
onto it. Your scripts are kept.

## Credits

parterre's iii firmware is GPL-3.0, built on monome's iii. The Open Grid kit,
its keypad boards and its enclosure are by FREEPOET.
