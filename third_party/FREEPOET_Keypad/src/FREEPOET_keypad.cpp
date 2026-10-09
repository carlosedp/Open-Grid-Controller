/**
 * @file FREEPOET_keypad.cpp
 * @brief Implementation of the FREEPOET_keypad Arduino library.
 *
 * Default MCU I2C pins:
 *   Uno / Nano / Mega / Nano Every: SDA=A4, SCL=A5
 *   Uno R4: SDA=A4(D18), SCL=A5(D19)
 *   ESP32: SDA=GPIO21, SCL=GPIO22
 *   ESP32-S3 / ESP32-C3: SDA=GPIO8, SCL=GPIO9
 *   ESP32-C6: SDA=GPIO6, SCL=GPIO7
 *   ESP8266: SDA=GPIO4(D2), SCL=GPIO5(D1)
 *   RP2040 / RP2350: SDA=GPIO4, SCL=GPIO5
 */

#include "FREEPOET_keypad.h"

#include <string.h>

// ============================================================
//  Constructor
// ============================================================

FREEPOET_keypad::FREEPOET_keypad(uint8_t addr, TwoWire *wire)
    : _addr(addr), _wire(wire), _begun(false) {}

// ============================================================
//  begin
// ============================================================

bool FREEPOET_keypad::begin() {
  if (!_begun) {
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    _wire->setSDA(4);
    _wire->setSCL(5);
#endif
    _wire->begin();
    _begun = true;
  }
  return isConnected();
}

bool FREEPOET_keypad::begin(int sdaPin, int sclPin) {
  if (!_begun) {
#if defined(ESP32) || defined(ESP8266)
    _wire->begin(sdaPin, sclPin);
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
  _wire->setSDA(sdaPin);
  _wire->setSCL(sclPin);
  _wire->begin();
#else
    // Platforms that do not support custom SDA/SCL at runtime
    (void)sdaPin;
    (void)sclPin;
    _wire->begin();
#endif
    _begun = true;
  }
  return isConnected();
}

// ============================================================
//  Device Info
// ============================================================

uint8_t FREEPOET_keypad::readProtocolVersion() {
  uint8_t val = 0;
  readReg(REG_PROTOCOL_VERSION, &val, 1);
  return val;
}

uint8_t FREEPOET_keypad::readDeviceAddress() {
  uint8_t val = 0;
  readReg(REG_DEVICE_ADDRESS, &val, 1);
  return val;
}

uint16_t FREEPOET_keypad::readKeyCount() {
  uint8_t lo = 0, hi = 0;
  if (!readReg(REG_KEY_COUNT_L, &lo, 1)) return 0;
  if (!readReg(REG_KEY_COUNT_H, &hi, 1)) return 0;
  return ((uint16_t)hi << 8) | lo;
}

// ============================================================
//  LED Control
// ============================================================

bool FREEPOET_keypad::setLevel(uint8_t keyIndex, uint8_t level) {
  if (keyIndex >= PAD_GRID_KEY_COUNT) return false;
  uint8_t cmd[3] = { REG_LED_SET_LEVEL, keyIndex, clampLevel(level) };
  return writeCmd(cmd, 3);
}

bool FREEPOET_keypad::setLevelShow(uint8_t keyIndex, uint8_t level) {
  if (keyIndex >= PAD_GRID_KEY_COUNT) return false;
  uint8_t cmd[3] = { REG_LED_SET_LEVEL_SHOW, keyIndex, clampLevel(level) };
  return writeCmd(cmd, 3);
}

bool FREEPOET_keypad::setLevelBlock(uint8_t startKey, uint8_t count, const uint8_t *levels) {
  if (!levels || count == 0 || count > PAD_GRID_LEVEL_BLOCK_MAX) return false;
  if ((uint16_t)startKey + count > PAD_GRID_KEY_COUNT) return false;

  uint8_t cmd[3 + PAD_GRID_LEVEL_BLOCK_MAX] = {0};
  cmd[0] = REG_LED_SET_LEVEL_BLOCK;
  cmd[1] = startKey;
  cmd[2] = count;

  for (uint8_t i = 0; i < count; i++) {
    cmd[3 + i] = clampLevel(levels[i]);
  }

  return writeCmd(cmd, (uint8_t)(3 + count));
}

bool FREEPOET_keypad::setLevelBlockPacked(uint8_t startKey, uint8_t count, const uint8_t *packedLevels) {
  if (!packedLevels || count == 0 || count > PAD_GRID_LEVEL_BLOCK_PACKED_MAX) return false;
  if ((uint16_t)startKey + count > PAD_GRID_KEY_COUNT) return false;

  const uint8_t packedBytes = (uint8_t)((count + 1u) / 2u);
  uint8_t cmd[3 + (PAD_GRID_LEVEL_BLOCK_PACKED_MAX / 2)] = {0};
  cmd[0] = REG_LED_SET_LEVEL_BLOCK_PACKED;
  cmd[1] = startKey;
  cmd[2] = count;
  memcpy(&cmd[3], packedLevels, packedBytes);
  return writeCmd(cmd, (uint8_t)(3 + packedBytes));
}

bool FREEPOET_keypad::set8x8Block(uint8_t blockIndex, const uint8_t *packedLevels) {
  if (!packedLevels || blockIndex > 1) return false;

  if (wireWriteCapacity() >= (2u + PAD_GRID_PACKED_BLOCK_BYTES)) {
    uint8_t cmd[2 + PAD_GRID_PACKED_BLOCK_BYTES] = {0};
    cmd[0] = REG_LED_SET_8X8_BLOCK;
    cmd[1] = blockIndex;
    memcpy(&cmd[2], packedLevels, PAD_GRID_PACKED_BLOCK_BYTES);
    return writeCmd(cmd, sizeof(cmd));
  }

  const uint8_t startKey = (uint8_t)(blockIndex * 64u);
  return setLevelBlockPacked(startKey, 58, packedLevels) &&
         setLevelBlockPacked((uint8_t)(startKey + 58u), 6, packedLevels + 29);
}

bool FREEPOET_keypad::setFullFrame(const uint8_t *packedLevels) {
  if (!packedLevels) return false;

  if (wireWriteCapacity() >= (1u + PAD_GRID_PACKED_FRAME_BYTES)) {
    uint8_t cmd[1 + PAD_GRID_PACKED_FRAME_BYTES] = {0};
    cmd[0] = REG_LED_SET_FULL_FRAME;
    memcpy(&cmd[1], packedLevels, PAD_GRID_PACKED_FRAME_BYTES);
    return writeCmd(cmd, sizeof(cmd));
  }

  return set8x8Block(0, packedLevels) &&
         set8x8Block(1, packedLevels + PAD_GRID_PACKED_BLOCK_BYTES);
}

bool FREEPOET_keypad::setPixel(uint8_t keyIndex, uint8_t r, uint8_t g, uint8_t b) {
  return setLevel(keyIndex, rgbToLevel(r, g, b));
}

bool FREEPOET_keypad::setPixelShow(uint8_t keyIndex, uint8_t r, uint8_t g, uint8_t b) {
  return setLevelShow(keyIndex, rgbToLevel(r, g, b));
}

bool FREEPOET_keypad::clearLeds() {
  uint8_t cmd[1] = { REG_LED_CLEAR };
  return writeCmd(cmd, 1);
}

bool FREEPOET_keypad::showLeds() {
  uint8_t cmd[1] = { REG_LED_SHOW };
  return writeCmd(cmd, 1);
}

bool FREEPOET_keypad::fillLevel(uint8_t level) {
  uint8_t frame[PAD_GRID_PACKED_FRAME_BYTES];
  memset(frame, packLevelPair(level, level), sizeof(frame));
  return setFullFrame(frame) && showLeds();
}

bool FREEPOET_keypad::fillAll(uint8_t r, uint8_t g, uint8_t b) {
  return fillLevel(rgbToLevel(r, g, b));
}

// ============================================================
//  Key Reading
// ============================================================

bool FREEPOET_keypad::readKey(uint8_t keyIndex, bool &pressed) {
  if (keyIndex >= PAD_GRID_KEY_COUNT) return false;

  uint8_t cmd[2] = { REG_KEY_READ_ONE, keyIndex };
  uint8_t val = 0;
  if (!readRegWithParam(cmd, 2, &val, 1)) return false;
  pressed = (val != 0);
  return true;
}

bool FREEPOET_keypad::readKeyXY(uint8_t x, uint8_t y, bool &pressed) {
  uint8_t idx = xyToIndex(x, y);
  if (idx == 255) return false;
  return readKey(idx, pressed);
}

uint8_t FREEPOET_keypad::readKeyBlock(uint8_t startKey, uint8_t count, uint8_t *results) {
  if (count == 0 || count > PAD_GRID_KEY_BLOCK_MAX || !results) return 0;

  uint8_t cmd[3] = { REG_KEY_READ_BLOCK, startKey, count };
  if (!readRegWithParam(cmd, 3, results, count)) return 0;
  return count;
}

bool FREEPOET_keypad::readAllKeys(uint8_t results[PAD_GRID_KEY_COUNT]) {
  // Read 128 keys in 4 blocks of 32
  for (uint8_t block = 0; block < 4; block++) {
    uint8_t startKey = block * PAD_GRID_KEY_BLOCK_MAX;
    if (readKeyBlock(startKey, PAD_GRID_KEY_BLOCK_MAX, &results[startKey]) == 0) {
      return false;
    }
  }
  return true;
}

// ============================================================
//  Utility
// ============================================================

uint8_t FREEPOET_keypad::xyToIndex(uint8_t x, uint8_t y) {
  if (x >= PAD_GRID_COLS || y >= PAD_GRID_ROWS) return 255;
  return x * PAD_GRID_ROWS + y;
}

void FREEPOET_keypad::indexToXY(uint8_t keyIndex, uint8_t &x, uint8_t &y) {
  x = keyIndex / PAD_GRID_ROWS;
  y = keyIndex % PAD_GRID_ROWS;
}

uint8_t FREEPOET_keypad::rgbToLevel(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t brightest = r;
  if (g > brightest) brightest = g;
  if (b > brightest) brightest = b;
  return (uint8_t)(((uint16_t)brightest * PAD_GRID_LEVEL_MAX + 127u) / 255u);
}

uint8_t FREEPOET_keypad::packLevelPair(uint8_t firstLevel, uint8_t secondLevel) {
  return (uint8_t)((clampLevel(firstLevel) << 4) | clampLevel(secondLevel));
}

void FREEPOET_keypad::packLevels(const uint8_t *levels, uint8_t count, uint8_t *packed) {
  if (!levels || !packed || count == 0) return;

  const uint8_t packedBytes = (uint8_t)((count + 1u) / 2u);
  for (uint8_t i = 0; i < packedBytes; i++) {
    const uint8_t firstIndex = (uint8_t)(i * 2u);
    const uint8_t secondIndex = (uint8_t)(firstIndex + 1u);
    const uint8_t firstLevel = levels[firstIndex];
    const uint8_t secondLevel = (secondIndex < count) ? levels[secondIndex] : 0;
    packed[i] = packLevelPair(firstLevel, secondLevel);
  }
}

bool FREEPOET_keypad::isConnected() {
  _wire->beginTransmission(_addr);
  return (_wire->endTransmission() == 0);
}

uint8_t FREEPOET_keypad::scanBus(uint8_t *foundAddrs, TwoWire *wire) {
  uint8_t count = 0;
  for (uint8_t addr = PAD_GRID_ADDR_MIN; addr <= PAD_GRID_ADDR_MAX; addr++) {
    wire->beginTransmission(addr);
    if (wire->endTransmission() == 0) {
      foundAddrs[count++] = addr;
    }
  }
  return count;
}

// ============================================================
//  Private I2C helpers
// ============================================================

uint8_t FREEPOET_keypad::clampLevel(uint8_t level) {
  return (level > PAD_GRID_LEVEL_MAX) ? PAD_GRID_LEVEL_MAX : level;
}

size_t FREEPOET_keypad::wireWriteCapacity() {
#if defined(BUFFER_LENGTH)
  return BUFFER_LENGTH;
#elif defined(WIRE_BUFFER_LENGTH)
  return WIRE_BUFFER_LENGTH;
#elif defined(I2C_BUFFER_LENGTH)
  return I2C_BUFFER_LENGTH;
#else
  return 32;
#endif
}

bool FREEPOET_keypad::writeCmd(const uint8_t *data, uint8_t len, bool sendStop) {
  _wire->beginTransmission(_addr);
  uint8_t written = 0;
  for (uint8_t i = 0; i < len; i++) {
    written += _wire->write(data[i]);
  }
  return (written == len) && (_wire->endTransmission(sendStop) == 0);
}

bool FREEPOET_keypad::readReg(uint8_t reg, uint8_t *buf, uint8_t len) {
  // Write register address (repeated-start)
  if (!writeCmd(&reg, 1, false)) return false;

  uint8_t n = _wire->requestFrom(_addr, len);
  if (n != len) return false;

  for (uint8_t i = 0; i < len; i++) {
    buf[i] = _wire->read();
  }
  return true;
}

bool FREEPOET_keypad::readRegWithParam(const uint8_t *cmd, uint8_t cmdLen,
                                   uint8_t *buf, uint8_t readLen) {
  // Write register + params (repeated-start)
  if (!writeCmd(cmd, cmdLen, false)) return false;

  uint8_t n = _wire->requestFrom(_addr, readLen);
  if (n != readLen) return false;

  for (uint8_t i = 0; i < readLen; i++) {
    buf[i] = _wire->read();
  }
  return true;
}
