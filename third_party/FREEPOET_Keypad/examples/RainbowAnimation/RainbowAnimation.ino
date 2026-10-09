/**
 * RainbowAnimation.ino
 * Display a scrolling grayscale pattern across all 128 LEDs using packed frame writes.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

uint8_t offset = 0;
uint8_t levels[PAD_GRID_KEY_COUNT];
uint8_t packed[PAD_GRID_PACKED_FRAME_BYTES];

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Packed Frame Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  pad.clearLeds();
  pad.showLeds();
}

void loop() {
  for (uint8_t x = 0; x < PAD_GRID_COLS; x++) {
    for (uint8_t y = 0; y < PAD_GRID_ROWS; y++) {
      const uint8_t index = FREEPOET_keypad::xyToIndex(x, y);
      levels[index] = (uint8_t)((x + y + offset) & PAD_GRID_LEVEL_MAX);
    }
  }

  FREEPOET_keypad::packLevels(levels, PAD_GRID_KEY_COUNT, packed);
  pad.setFullFrame(packed);
  pad.showLeds();

  offset = (uint8_t)((offset + 1) & PAD_GRID_LEVEL_MAX);
  delay(30);
}
