/**
 * MultiDevice.ino
 * Control two FREEPOET_keypad devices on the same I2C bus.
 *
 * Device A: address 0x2F (all address pins floating)
 * Device B: address 0x2E (GPIO8 = GND, others floating)
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad padA(0x2F);
FREEPOET_keypad padB(0x2E);

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Multi-Device Example");
  if (!padA.begin()) {
    Serial.println("WARNING: device A (0x2F) not found");
  } else {
    Serial.println("Device A (0x2F) connected");
  }

  if (!padB.begin()) {
    Serial.println("WARNING: device B (0x2E) not found");
  } else {
    Serial.println("Device B (0x2E) connected");
  }

  padA.fillLevel(4);
  padB.fillLevel(12);
}

void loop() {
  uint8_t keysA[PAD_GRID_KEY_COUNT];
  if (padA.readAllKeys(keysA)) {
    for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
      if (keysA[i]) {
        Serial.print("A key ");
        Serial.println(i);
      }
    }
  }

  uint8_t keysB[PAD_GRID_KEY_COUNT];
  if (padB.readAllKeys(keysB)) {
    for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
      if (keysB[i]) {
        Serial.print("B key ");
        Serial.println(i);
      }
    }
  }

  delay(50);
}
