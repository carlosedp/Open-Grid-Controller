/**
 * BasicLED.ino
 * Set individual LEDs to different brightness levels using FREEPOET_keypad.
 *
 * Wiring:
 *   SDA -> device SDA
 *   SCL -> device SCL
 *   GND -> device GND
 *   3.3V/5V -> device VCC
 *   SDA and SCL each need an external pull-up to 3.3V, typically 4.7kΩ
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;  // default address 0x2F

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad BasicLED Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  // Clear all LEDs first
  pad.clearLeds();
  pad.showLeds();

  // Set a few LEDs to different levels (no show until the end)
  pad.setLevel(0, 1);
  pad.setLevel(1, 3);
  pad.setLevel(2, 5);
  pad.setLevel(3, 7);
  pad.setLevel(4, 9);
  pad.setLevel(5, 11);
  pad.setLevel(6, 13);
  pad.setLevel(7, 15);

  // Now push the levels to the LEDs
  pad.showLeds();

  Serial.println("LED levels set!");
}
void loop() {
  // Nothing to do
  delay(1000);
}
