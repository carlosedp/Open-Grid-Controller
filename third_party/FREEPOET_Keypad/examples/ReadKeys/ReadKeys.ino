/**
 * ReadKeys.ino
 * Read and display individual key states using FREEPOET_keypad.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad ReadKeys Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  Serial.println("Device connected. Press any key...");
}

void loop() {
  // Poll each key individually
  for (uint8_t x = 0; x < PAD_GRID_COLS; x++) {
    for (uint8_t y = 0; y < PAD_GRID_ROWS; y++) {
      bool pressed = false;
      if (pad.readKeyXY(x, y, pressed) && pressed) {
        Serial.print("Key pressed at (");
        Serial.print(x);
        Serial.print(", ");
        Serial.print(y);
        Serial.print(") -> index ");
        Serial.println(FREEPOET_keypad::xyToIndex(x, y));
      }
    }
  }

  delay(50);
}
