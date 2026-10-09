/**
 * ESP32_CustomPins.ino
 * Example for boards with custom I2C pins.
 */

#include <FREEPOET_keypad.h>

static const int MY_SDA_PIN = 21;
static const int MY_SCL_PIN = 22;

FREEPOET_keypad pad(PAD_GRID_DEFAULT_ADDR, &Wire);

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Custom Pins Example");

  if (!pad.begin(MY_SDA_PIN, MY_SCL_PIN)) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  uint8_t corners[] = {
    FREEPOET_keypad::xyToIndex(0, 0),
    FREEPOET_keypad::xyToIndex(15, 0),
    FREEPOET_keypad::xyToIndex(0, 7),
    FREEPOET_keypad::xyToIndex(15, 7),
  };

  pad.clearLeds();
  for (uint8_t i = 0; i < 4; i++) {
    pad.setLevel(corners[i], 15);
  }
  pad.showLeds();
}

void loop() {
  uint8_t keys[PAD_GRID_KEY_COUNT];
  if (pad.readAllKeys(keys)) {
    for (uint8_t i = 0; i < PAD_GRID_KEY_COUNT; i++) {
      if (keys[i]) {
        uint8_t x, y;
        FREEPOET_keypad::indexToXY(i, x, y);
        Serial.print("Key (");
        Serial.print(x);
        Serial.print(",");
        Serial.print(y);
        Serial.println(") pressed");
      }
    }
  }
  delay(50);
}
