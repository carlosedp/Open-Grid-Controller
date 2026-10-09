/**
 * BrightnessWave.ino
 * Create a pulsing brightness wave that moves across the grid.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

uint16_t phase = 0;
uint8_t levels[PAD_GRID_KEY_COUNT];
uint8_t packed[PAD_GRID_PACKED_FRAME_BYTES];

// Simple sine-like wave using integer math (0..255 output)
uint8_t waveBrightness(uint16_t angle) {
  // angle: 0..255 maps to 0..2*PI
  // Approximate with triangle wave for simplicity
  if (angle < 64)  return angle * 4;
  if (angle < 192) return (128 - (int16_t)angle + 64) * 4 / 2 + 128;
  // Actually let's just use a simple approach:
  // Map 0..255 to a smooth-ish 0->255->0 curve
  if (angle < 128) return angle * 2;
  return (255 - angle) * 2;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Brightness Wave Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  pad.clearLeds();
  pad.showLeds();
}

void loop() {
  for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
    uint8_t angle = (uint8_t)((phase + (uint16_t)i * 2) & 0xFF);
    uint8_t b = waveBrightness(angle);
    levels[i] = (uint8_t)(((uint16_t)b * PAD_GRID_LEVEL_MAX + 127u) / 255u);
  }
  FREEPOET_keypad::packLevels(levels, PAD_GRID_KEY_COUNT, packed);
  pad.setFullFrame(packed);
  pad.showLeds();

  phase += 4;
  delay(30);
}
