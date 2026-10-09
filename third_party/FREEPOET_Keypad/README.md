# FREEPOET_keypad Arduino Library

Arduino library for controlling **8x8-Pad** I2C slave devices that implement protocol **0x02**. Each device exposes a 16x8 matrix of **128 LEDs** and **128 keys** over a register-based I2C interface.

Protocol `0x02` is level-based. The host sends LED brightness `level` values from `0` to `15`, and the device maps those levels to local grayscale RGB output. The preferred APIs in this library therefore use `level`, packed block transfers, and buffered `show` semantics.

Up to **16 devices** can share one I2C bus using addresses `0x20` to `0x2F`.

## Supported Boards

| Platform | Boards |
|----------|--------|
| AVR | Arduino Uno (R3), Arduino Nano, Arduino Mega 2560 |
| Renesas RA4M1 | Arduino Uno R4 (Minima / WiFi) |
| megaAVR | Arduino Nano Every |
| ESP32 | ESP32, ESP32-S3, ESP32-C3, ESP32-C6 |
| ESP8266 | NodeMCU, Wemos D1 Mini, etc. |
| RP2040 | Raspberry Pi Pico, Arduino Nano RP2040 Connect |
| RP2350 | Raspberry Pi Pico 2 |

## Installation

### Arduino IDE (Manual)

1. Download or clone this repository.
2. Copy the `FREEPOET_keypad` folder into your Arduino `libraries/` directory.
   - macOS: `~/Documents/Arduino/libraries/`
   - Windows: `Documents\Arduino\libraries\`
   - Linux: `~/Arduino/libraries/`
3. Restart the Arduino IDE.
4. Go to **Sketch -> Include Library** and confirm **FREEPOET_keypad** appears.

### Arduino IDE (ZIP)

1. Zip the `FREEPOET_keypad` folder.
2. In Arduino IDE choose **Sketch -> Include Library -> Add .ZIP Library...**
3. Select the zip file.

## Dependencies

This library has **no external Library Manager dependency**.

- `Wire` is part of the Arduino core on supported boards.
- Users do not need to install any extra library for `FREEPOET_keypad` itself.
- If you later add a third-party library dependency, declare it in `library.properties` with `depends=` so Arduino IDE / Library Manager can offer installation help.

This statement applies only to the published host-side library in this folder.
The root firmware project in this repository is a different target and does use `Adafruit NeoPixel` for direct LED driving.

## Wiring

Connect the 8x8-Pad device to your board's I2C bus:

| 8x8-Pad Pin | Board Pin |
|-------------|-----------|
| SDA | SDA (board default or custom) |
| SCL | SCL (board default or custom) |
| GND | GND |
| VCC | 3.3V or 5V (depending on board) |

### Default I2C Pins by Board

| Board | SDA | SCL |
|-------|-----|-----|
| Arduino Uno / Nano / Mega | A4 | A5 |
| Arduino Uno R4 | A4 (D18) | A5 (D19) |
| Arduino Nano Every | A4 | A5 |
| ESP32 | GPIO 21 | GPIO 22 |
| ESP32-S3 | GPIO 8 | GPIO 9 |
| ESP32-C3 | GPIO 8 | GPIO 9 |
| ESP32-C6 | GPIO 6 | GPIO 7 |
| ESP8266 | GPIO 4 (D2) | GPIO 5 (D1) |
| RP2040 / RP2350 | GPIO 4 | GPIO 5 |

On ESP32 and RP2040/RP2350 boards you can use `begin(sdaPin, sclPin)` to choose custom I2C pins.

## I2C Pull-Ups Required

This bus requires pull-up resistors on `SDA` and `SCL`.

- Use external pull-ups to `3.3V`
- Typical value: `4.7kΩ` on `SDA` and `4.7kΩ` on `SCL`
- Do not leave the bus floating
- Do not pull `SDA` or `SCL` to `5V` when using RP2040 / RP2350

If the idle bus voltage is around `1V~2V`, the bus is floating or being weakly biased and scan results may show false device addresses.

## I2C Address Configuration

The device address is set by four hardware pins with internal pull-up:

```text
Address = 0x20 + ((GPIO11 << 3) | (GPIO10 << 2) | (GPIO9 << 1) | GPIO8)
```

- All pins floating -> `0x2F` (default)
- All pins grounded -> `0x20`

## Protocol Notes

- Protocol version is currently `0x02`
- LED/key index mapping is `keyIndex = x * 8 + y`
- Preferred LED brightness range is `0..15`
- `set` commands only update the device target buffer
- `showLeds()` requests a buffered flush
- RGB compatibility APIs remain available, but the device stores only a grayscale level derived from the brightest RGB channel

## Quick Start

```cpp
#include <FREEPOET_keypad.h>

FREEPOET_keypad keypad;

void setup() {
  Serial.begin(115200);

  if (!keypad.begin()) {
    Serial.println("Device not found!");
    while (1) {
      delay(1000);
    }
  }

  keypad.setLevelShow(0, 15);
}

void loop() {
  bool pressed = false;
  if (keypad.readKey(0, pressed) && pressed) {
    Serial.println("Key 0 pressed!");
  }
  delay(50);
}
```

## API Reference

### Constructor

```cpp
FREEPOET_keypad(uint8_t addr = 0x2F, TwoWire *wire = &Wire)
```

### Initialization

```cpp
bool begin()
bool begin(int sdaPin, int sclPin)
```

### Device Info

```cpp
uint8_t readProtocolVersion()
uint8_t readDeviceAddress()
uint16_t readKeyCount()
```

### Preferred LED APIs

```cpp
bool setLevel(uint8_t keyIndex, uint8_t level)
bool setLevelShow(uint8_t keyIndex, uint8_t level)
bool setLevelBlock(uint8_t startKey, uint8_t count, const uint8_t *levels)
bool setLevelBlockPacked(uint8_t startKey, uint8_t count, const uint8_t *packedLevels)
bool set8x8Block(uint8_t blockIndex, const uint8_t *packedLevels)
bool setFullFrame(const uint8_t *packedLevels)
bool fillLevel(uint8_t level)
bool clearLeds()
bool showLeds()
```

`setLevelBlockPacked()` is the recommended default bulk write. `set8x8Block()` and `setFullFrame()` automatically fall back to smaller packed writes on boards with small `Wire` transmission buffers.

### RGB Compatibility APIs

```cpp
bool setPixel(uint8_t keyIndex, uint8_t r, uint8_t g, uint8_t b)
bool setPixelShow(uint8_t keyIndex, uint8_t r, uint8_t g, uint8_t b)
bool fillAll(uint8_t r, uint8_t g, uint8_t b)
```

These methods quantize RGB input to the protocol's `0..15` grayscale level.

### Key Reading

```cpp
bool readKey(uint8_t keyIndex, bool &pressed)
bool readKeyXY(uint8_t x, uint8_t y, bool &pressed)
uint8_t readKeyBlock(uint8_t startKey, uint8_t count, uint8_t *results)
bool readAllKeys(uint8_t results[128])
```

### Utility

```cpp
static uint8_t xyToIndex(uint8_t x, uint8_t y)
static void indexToXY(uint8_t keyIndex, uint8_t &x, uint8_t &y)
static uint8_t rgbToLevel(uint8_t r, uint8_t g, uint8_t b)
static uint8_t packLevelPair(uint8_t firstLevel, uint8_t secondLevel)
static void packLevels(const uint8_t *levels, uint8_t count, uint8_t *packed)
bool isConnected()
uint8_t getAddress()
static uint8_t scanBus(uint8_t *foundAddrs, TwoWire *wire = &Wire)
```

## Compatibility

The primary header is now `FREEPOET_keypad.h` and the primary class is `FREEPOET_keypad`.

For easier migration, the library also provides a compatibility header named `PadGrid8x8.h` that includes the new header, and the new header exposes `using PadGrid8x8 = FREEPOET_keypad;`.

## Key Index Mapping

Keys and LEDs are indexed as `keyIndex = x * 8 + y`:

```text
             Row 0  Row 1  Row 2  Row 3  Row 4  Row 5  Row 6  Row 7
Column 0  :    0      1      2      3      4      5      6      7
Column 1  :    8      9     10     11     12     13     14     15
Column 2  :   16     17     18     19     20     21     22     23
  ...
Column 15 :  120    121    122    123    124    125    126    127
```

## Examples

| Example | Description |
|---------|-------------|
| **BasicLED** | Set individual LED brightness levels |
| **ReadKeys** | Poll key states one by one |
| **KeyToLED** | Light the LED under each pressed key |
| **ToggleKeys** | Toggle LED on or off with each key press |
| **RainbowAnimation** | Packed full-frame grayscale animation |
| **BrightnessWave** | Pulsing brightness wave using packed writes |
| **ColumnChase** | Column-by-column chase animation |
| **KeyBlockRead** | Efficient block read of 32 keys at a time |
| **MultiDevice** | Control two devices on the same bus |
| **ScanDevices** | Discover all devices on the I2C bus |
| **ESP32_CustomPins** | Use custom SDA/SCL pins on ESP32/RP2040 |

## Multi-Device Usage

```cpp
FREEPOET_keypad keypadA(0x2F);
FREEPOET_keypad keypadB(0x2E);

void setup() {
  keypadA.begin();
  keypadB.begin();

  keypadA.fillLevel(4);
  keypadB.fillLevel(12);
}
```

## Error Handling

All I2C transaction methods return `bool` or a byte count. Always check the return value:

```cpp
if (!keypad.setLevelShow(0, 15)) {
  Serial.println("I2C write failed!");
}

bool pressed;
if (!keypad.readKey(0, pressed)) {
  Serial.println("I2C read failed!");
}
```

Common failure causes:
- Wrong I2C address
- Wiring issue (SDA/SCL swapped, missing pull-ups)
- Device not powered
- Bus speed too high for long wires (try 100 kHz)

## License

This library is released under the MIT License.

See `LICENSE` in this folder for the full text.

## Release Notes

- Current release: `2.1.0`
- Protocol target: `0x02`
- External dependency: none
- Core dependency: Arduino `Wire`

Repository note: the top-level firmware project still depends on `Adafruit NeoPixel`, but that dependency is not required by this `FREEPOET_keypad` Arduino library package.

If you publish this library to a public repository or Arduino Library Manager later, update `library.properties:url` and the maintainer information before tagging the release.

## Package Contents

- `src/` public library source
- `examples/` usage examples for Arduino IDE
- `library.properties` Arduino library metadata
- `keywords.txt` Arduino IDE syntax highlighting
- `CHANGELOG.md` release history
- `DEPENDENCIES.md` dependency policy and install behavior
- `RELEASE_CHECKLIST.md` pre-release checklist
- `LICENSE` license text
