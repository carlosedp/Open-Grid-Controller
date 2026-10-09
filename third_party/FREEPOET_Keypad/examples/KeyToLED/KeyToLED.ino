/**
 * KeyToLED.ino
 * Light up the corresponding LED when a key is pressed.
 * Uses block read for efficient key scanning.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

uint8_t keyStates[PAD_GRID_KEY_COUNT];

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad KeyToLED Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  pad.clearLeds();
  pad.showLeds();
  Serial.println("Press keys to light LEDs...");
}

void loop() {
  // Read all 128 keys at once
  if (!pad.readAllKeys(keyStates)) {
    Serial.println("Read error");
    delay(100);
    return;
  }

  // Update LEDs based on key states
  for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
    if (keyStates[i]) {
      pad.setLevel(i, 12);
    } else {
      pad.setLevel(i, 0);
    }
  }
  pad.showLeds();

  delay(20);
}
