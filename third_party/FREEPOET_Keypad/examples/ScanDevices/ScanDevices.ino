/**
 * ScanDevices.ino
 * Scan the I2C bus for all connected FREEPOET_keypad devices.
 */

#include <FREEPOET_keypad.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Bus Scanner");
  FREEPOET_keypad pad;
  pad.begin();

  uint8_t addrs[16] = {0};
  uint8_t count = FREEPOET_keypad::scanBus(addrs);

  if (count == 0) {
    Serial.println("No devices found.");
  } else {
    Serial.print("Found ");
    Serial.print(count);
    Serial.println(" device(s):");

    for (uint8_t i = 0; i < count; i++) {
      Serial.print("  [");
      Serial.print(i);
      Serial.print("] Address: 0x");
      Serial.println(addrs[i], HEX);
    }
  }
}

void loop() {
  delay(5000);
}
