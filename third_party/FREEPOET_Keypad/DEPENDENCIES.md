# Dependencies

## Runtime and Build Dependencies

- Required Arduino core header: `Arduino.h`
- Required Arduino core library: `Wire`
- External Library Manager dependencies: none

`Wire` is shipped with Arduino board cores, so end users do not need to install an extra library to use `FREEPOET_keypad`.

## Scope

This dependency statement is only for the publishable Arduino host library in `arduino_lib/FREEPOET_keypad`.

The repository's top-level device firmware and reference firmware are separate projects. They directly drive NeoPixels in firmware and therefore can depend on `Adafruit NeoPixel`.

## Install Behavior

- Manual folder copy: works with no extra steps beyond installing a supported board core.
- ZIP import in Arduino IDE: works with no extra dependency prompts.
- Library Manager installation: no third-party dependency resolution is needed for this library.

## If Dependencies Are Added Later

If a future release adds a third-party dependency:

1. Declare it in `library.properties` using `depends=`.
2. Mention the exact library name and tested version in `README.md`.
3. Add a compile-time include error only if the dependency is optional but strongly recommended.

Arduino IDE can help with install-time dependency handling through Library Manager metadata, but it does not provide a custom runtime popup from inside the library.