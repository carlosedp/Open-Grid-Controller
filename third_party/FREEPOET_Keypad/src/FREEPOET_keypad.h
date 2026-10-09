/**
 * @file FREEPOET_keypad.h
 * @brief Arduino library for controlling 8x8-Pad I2C slave devices.
 *
 * Each 8x8-Pad device has a 16x8 matrix of 128 keys and 128 grayscale LEDs.
 * Communication uses the protocol 0x02 register-based I2C interface.
 *
 * Default MCU I2C pins:
 *   Uno / Nano / Mega / Nano Every: SDA=A4, SCL=A5
 *   Uno R4: SDA=A4(D18), SCL=A5(D19)
 *   ESP32: SDA=GPIO21, SCL=GPIO22
 *   ESP32-S3 / ESP32-C3: SDA=GPIO8, SCL=GPIO9
 *   ESP32-C6: SDA=GPIO6, SCL=GPIO7
 *   ESP8266: SDA=GPIO4(D2), SCL=GPIO5(D1)
 *   RP2040 / RP2350: SDA=GPIO4, SCL=GPIO5
 *
 * Supported platforms:
 *   Arduino Uno/R3, Nano, Mega 2560, Uno R4, Nano Every,
 *   ESP32, ESP32-S3, ESP32-C3/C6, ESP8266, RP2040, RP2350
 */

#ifndef FREEPOET_KEYPAD_H
#define FREEPOET_KEYPAD_H

#include <Arduino.h>
#include <Wire.h>

// ---------- Constants ----------

/// Default I2C address (all address pins floating / HIGH)
#define PAD_GRID_DEFAULT_ADDR  0x2F

/// Address range
#define PAD_GRID_ADDR_MIN      0x20
#define PAD_GRID_ADDR_MAX      0x2F

/// Matrix dimensions
#define PAD_GRID_ROWS                     8
#define PAD_GRID_COLS                     16
#define PAD_GRID_KEY_COUNT                128

/// Protocol version implemented by the current firmware manual
#define PAD_GRID_PROTOCOL_VERSION         0x02

/// Maximum counts for protocol block transfers
#define PAD_GRID_KEY_BLOCK_MAX            32
#define PAD_GRID_LEVEL_MAX                15
#define PAD_GRID_LEVEL_BLOCK_MAX          29
#define PAD_GRID_LEVEL_BLOCK_PACKED_MAX   58
#define PAD_GRID_PACKED_BLOCK_BYTES       32
#define PAD_GRID_PACKED_FRAME_BYTES       64

// ---------- Register addresses ----------

#define REG_PROTOCOL_VERSION   0x00
#define REG_DEVICE_ADDRESS     0x01
#define REG_KEY_COUNT_L        0x02
#define REG_KEY_COUNT_H        0x03

#define REG_LED_SET_PIXEL      0x10
#define REG_LED_SET_PIXEL_SHOW 0x11
#define REG_LED_CLEAR          0x12
#define REG_LED_SHOW           0x13

#define REG_LED_SET_LEVEL              0x14
#define REG_LED_SET_LEVEL_SHOW         0x15
#define REG_LED_SET_LEVEL_BLOCK        0x16
#define REG_LED_SET_LEVEL_BLOCK_PACKED 0x17
#define REG_LED_SET_8X8_BLOCK          0x18
#define REG_LED_SET_FULL_FRAME         0x19

#define REG_KEY_READ_ONE       0x20
#define REG_KEY_READ_BLOCK     0x21

// ---------- Class ----------

class FREEPOET_keypad {
public:
  /**
   * @brief Construct a FREEPOET_keypad instance.
   * @param addr I2C slave address (0x20..0x2F). Default 0x2F.
   * @param wire Pointer to TwoWire instance. Default &Wire.
   */
  FREEPOET_keypad(uint8_t addr = PAD_GRID_DEFAULT_ADDR, TwoWire *wire = &Wire);

  /**
   * @brief Initialize I2C communication.
   * @return true if the device responds on the bus.
   */
  bool begin();

  /**
   * @brief Initialize with explicit SDA/SCL pins (ESP32, RP2040, etc.).
   * @param sdaPin SDA pin number.
   * @param sclPin SCL pin number.
   * @return true if the device responds on the bus.
   */
  bool begin(int sdaPin, int sclPin);

  // ---------- Device Info ----------

  /**
   * @brief Read protocol version from device.
    * @return Protocol version (e.g., 0x02), or 0 on failure.
   */
  uint8_t readProtocolVersion();

  /**
   * @brief Read the device's own I2C address register.
   * @return Address (0x20..0x2F), or 0 on failure.
   */
  uint8_t readDeviceAddress();

  /**
   * @brief Read the total key count reported by the device.
   * @return Key count (e.g., 128), or 0 on failure.
   */
  uint16_t readKeyCount();

  // ---------- LED Control ----------

  /**
   * @brief Set one LED brightness level without requesting a refresh.
   * @param keyIndex LED index (0..127).
   * @param level Brightness level (0..15).
   * @return true on success.
   */
  bool setLevel(uint8_t keyIndex, uint8_t level);

  /**
   * @brief Set one LED brightness level and request a buffered refresh.
   * @param keyIndex LED index (0..127).
   * @param level Brightness level (0..15).
   * @return true on success.
   */
  bool setLevelShow(uint8_t keyIndex, uint8_t level);

  /**
   * @brief Write a continuous range of unpacked levels.
   * @param startKey Starting LED index (0..127).
   * @param count Number of LEDs to write (1..29).
   * @param levels Input buffer containing count levels.
   * @return true on success.
   */
  bool setLevelBlock(uint8_t startKey, uint8_t count, const uint8_t *levels);

  /**
   * @brief Write a continuous range of packed 4-bit levels.
   * @param startKey Starting LED index (0..127).
   * @param count Number of LEDs to write (1..58).
   * @param packedLevels Input buffer of ceil(count / 2) packed bytes.
   * @return true on success.
   */
  bool setLevelBlockPacked(uint8_t startKey, uint8_t count, const uint8_t *packedLevels);

  /**
   * @brief Write one 8x8 block using 32 packed bytes.
   * @param blockIndex 0 for keys 0..63, 1 for keys 64..127.
   * @param packedLevels Input buffer of 32 packed bytes.
   * @return true on success.
   */
  bool set8x8Block(uint8_t blockIndex, const uint8_t *packedLevels);

  /**
   * @brief Write the full 16x8 frame using 64 packed bytes.
   * @param packedLevels Input buffer of 64 packed bytes.
   * @return true on success.
   */
  bool setFullFrame(const uint8_t *packedLevels);

  /**
   * @brief Set one LED color without refreshing.
   * @param keyIndex LED index (0..127).
   * @param r Red (0..255).
   * @param g Green (0..255).
   * @param b Blue (0..255).
   * @return true on success. RGB is quantized to a grayscale level.
   */
  bool setPixel(uint8_t keyIndex, uint8_t r, uint8_t g, uint8_t b);

  /**
   * @brief Set one LED color and immediately refresh.
   * @param keyIndex LED index (0..127).
   * @param r Red (0..255).
   * @param g Green (0..255).
   * @param b Blue (0..255).
    * @return true on success. RGB is quantized to a grayscale level.
   */
  bool setPixelShow(uint8_t keyIndex, uint8_t r, uint8_t g, uint8_t b);

  /**
   * @brief Clear all LEDs (no immediate refresh).
   * @return true on success.
   */
  bool clearLeds();

  /**
   * @brief Refresh all LEDs to show previously set colors.
   * @return true on success.
   */
  bool showLeds();

  /**
   * @brief Set all LEDs to one brightness level and request a refresh.
   * @param level Brightness level (0..15).
   * @return true on success.
   */
  bool fillLevel(uint8_t level);

  /**
   * @brief Set all LEDs to one color and refresh.
   * @param r Red (0..255).
   * @param g Green (0..255).
   * @param b Blue (0..255).
   * @return true on success. RGB is quantized to a grayscale level.
   */
  bool fillAll(uint8_t r, uint8_t g, uint8_t b);

  // ---------- Key Reading ----------

  /**
   * @brief Read a single key state.
   * @param keyIndex Key index (0..127).
   * @param pressed Output: true if key is pressed.
   * @return true on successful I2C transaction.
   */
  bool readKey(uint8_t keyIndex, bool &pressed);

  /**
   * @brief Read key state by (x, y) coordinate.
   * @param x Column (0..15).
   * @param y Row (0..7).
   * @param pressed Output: true if key is pressed.
   * @return true on successful I2C transaction.
   */
  bool readKeyXY(uint8_t x, uint8_t y, bool &pressed);

  /**
   * @brief Read a contiguous block of key states.
   * @param startKey Starting key index (0..127).
   * @param count Number of keys to read (1..32).
   * @param results Output buffer, must be at least count bytes.
   *                Each byte: 1 = pressed, 0 = released.
   * @return Number of bytes actually read, or 0 on failure.
   */
  uint8_t readKeyBlock(uint8_t startKey, uint8_t count, uint8_t *results);

  /**
   * @brief Read all 128 keys. Performs multiple block reads internally.
   * @param results Output buffer of 128 bytes.
   *                Each byte: 1 = pressed, 0 = released.
   * @return true on success.
   */
  bool readAllKeys(uint8_t results[PAD_GRID_KEY_COUNT]);

  // ---------- Utility ----------

  /**
   * @brief Convert (x, y) to linear key index.
   * @param x Column (0..15).
   * @param y Row (0..7).
   * @return Key index (0..127), or 255 if out of range.
   */
  static uint8_t xyToIndex(uint8_t x, uint8_t y);

  /**
   * @brief Convert linear key index to (x, y).
   * @param keyIndex Key index (0..127).
   * @param x Output column.
   * @param y Output row.
   */
  static void indexToXY(uint8_t keyIndex, uint8_t &x, uint8_t &y);

  /**
   * @brief Convert RGB input to the protocol's 4-bit grayscale level.
   * @param r Red (0..255).
   * @param g Green (0..255).
   * @param b Blue (0..255).
   * @return Quantized level (0..15).
   */
  static uint8_t rgbToLevel(uint8_t r, uint8_t g, uint8_t b);

  /**
   * @brief Pack two 4-bit levels into one byte.
   * @param firstLevel Stored in the high nibble.
   * @param secondLevel Stored in the low nibble.
   * @return Packed byte.
   */
  static uint8_t packLevelPair(uint8_t firstLevel, uint8_t secondLevel);

  /**
   * @brief Pack unpacked levels into protocol byte format.
   * @param levels Input buffer of count levels.
   * @param count Number of levels to pack.
   * @param packed Output buffer of ceil(count / 2) bytes.
   */
  static void packLevels(const uint8_t *levels, uint8_t count, uint8_t *packed);

  /**
   * @brief Check if a device is present on the bus.
   * @return true if device ACKs.
   */
  bool isConnected();

  /**
   * @brief Get the I2C address of this instance.
   * @return Address (0x20..0x2F).
   */
  uint8_t getAddress() const { return _addr; }

  /**
    * @brief Scan the I2C bus for all FREEPOET_keypad devices (0x20..0x2F).
   * @param foundAddrs Output array (at least 16 elements).
   * @return Number of devices found.
   */
  static uint8_t scanBus(uint8_t *foundAddrs, TwoWire *wire = &Wire);

private:
  uint8_t  _addr;
  TwoWire *_wire;
  bool     _begun;

  static uint8_t clampLevel(uint8_t level);
  static size_t wireWriteCapacity();

  bool writeCmd(const uint8_t *data, uint8_t len, bool sendStop = true);
  bool readReg(uint8_t reg, uint8_t *buf, uint8_t len);
  bool readRegWithParam(const uint8_t *cmd, uint8_t cmdLen, uint8_t *buf, uint8_t readLen);
};

using PadGrid8x8 = FREEPOET_keypad;

#endif // FREEPOET_KEYPAD_H
