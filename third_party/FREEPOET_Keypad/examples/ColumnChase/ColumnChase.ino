/**
 * ColumnChase.ino
 * Light up one column at a time and chase across the 16-column grid.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

uint8_t currentCol = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Column Chase Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  pad.clearLeds();
  pad.showLeds();
}

void loop() {
  pad.clearLeds();

  // Light up all 8 LEDs in the current column
  for (uint8_t row = 0; row < PAD_GRID_ROWS; row++) {
    uint8_t idx = FREEPOET_keypad::xyToIndex(currentCol, row);
    uint8_t level = (uint8_t)(2 + row * 2);
    if (level > PAD_GRID_LEVEL_MAX) {
      level = PAD_GRID_LEVEL_MAX;
    }
    pad.setLevel(idx, level);
  }

  pad.showLeds();

  currentCol = (currentCol + 1) % PAD_GRID_COLS;
  delay(100);
}
