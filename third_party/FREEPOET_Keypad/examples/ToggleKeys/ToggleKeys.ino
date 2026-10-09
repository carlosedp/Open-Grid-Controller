/**
 * ToggleKeys.ino
 * Toggle LED on/off each time a key is pressed.
 * Demonstrates using block reads and maintaining local toggle state.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

uint8_t prevKeys[PAD_GRID_KEY_COUNT];
bool ledState[PAD_GRID_KEY_COUNT];

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Toggle Keys Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  pad.clearLeds();
  pad.showLeds();

  memset(prevKeys, 0, sizeof(prevKeys));
  memset(ledState, 0, sizeof(ledState));

  Serial.println("Press keys to toggle LEDs on/off...");
}

void loop() {
  uint8_t curKeys[PAD_GRID_KEY_COUNT];
  if (!pad.readAllKeys(curKeys)) {
    delay(50);
    return;
  }

  bool changed = false;
  for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
    if (curKeys[i] && !prevKeys[i]) {
      ledState[i] = !ledState[i];
      changed = true;
      Serial.print("Toggle key ");
      Serial.print(i);
      Serial.print(" -> ");
      Serial.println(ledState[i] ? "ON" : "OFF");
    }
    prevKeys[i] = curKeys[i];
  }

  if (changed) {
    for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
      pad.setLevel(i, ledState[i] ? 10 : 0);
    }
    pad.showLeds();
  }

  delay(20);
}
