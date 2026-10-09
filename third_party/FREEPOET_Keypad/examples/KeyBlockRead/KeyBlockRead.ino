/**
 * KeyBlockRead.ino
 * Demonstrates efficient block reading of key states,
 * reading 32 keys per I2C transaction instead of one at a time.
 */

#include <FREEPOET_keypad.h>

FREEPOET_keypad pad;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("FREEPOET_keypad Block Read Example");
  if (!pad.begin()) {
    Serial.println("ERROR: device not found!");
    while (1) { delay(1000); }
  }

  Serial.println("Reading keys in blocks of 32...");
}

void printKeyBlock(uint8_t startKey, uint8_t count, const uint8_t *data) {
  Serial.print("Keys ");
  Serial.print(startKey);
  Serial.print("..");
  Serial.print(startKey + count - 1);
  Serial.print(": ");

  for (uint8_t i = 0; i < count; i++) {
    Serial.print(data[i] ? '1' : '0');
    if ((i + 1) % 8 == 0 && i + 1 < count) Serial.print(' ');
  }
  Serial.println();
}

void loop() {
  // Read 4 blocks of 32 keys
  for (uint8_t block = 0; block < 4; block++) {
    uint8_t startKey = block * 32;
    uint8_t buf[32];

    uint8_t read = pad.readKeyBlock(startKey, 32, buf);
    if (read > 0) {
      // Check if any key in this block is pressed
      bool anyPressed = false;
      for (uint8_t i = 0; i < read; i++) {
        if (buf[i]) { anyPressed = true; break; }
      }
      if (anyPressed) {
        printKeyBlock(startKey, read, buf);
      }
    }
  }

  delay(100);
}
