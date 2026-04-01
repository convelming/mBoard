/*
  esp32_cn4_test.ino

  Purpose:
    - Test the CN4 electromagnet output on the ESP32-S3 board.
    - Based on the schematic, CN4 is controlled by net PDI, which maps to GPIO8.
    - Send '1' over serial to activate for 5 seconds, then auto-off.
    - Send '0' over serial to force OFF immediately.

  Wiring / board note:
    - PDI -> Q4 -> CN4
    - ESP32 pin used here: GPIO8

  Serial:
    - Baud: 115200
    - '1' = activate for 5 seconds
    - '0' = force OFF
*/

#include <Arduino.h>

static const uint8_t MAG_PIN = 8;          // GPIO8 = PDI in the schematic
static const bool ACTIVE_HIGH = true;      // If behavior is reversed, change to false
static const unsigned long PULSE_MS = 10000;

bool magnetOn = false;
unsigned long magnetOffAt = 0;

void setMagnet(bool enabled) {
  const uint8_t level = enabled
    ? (ACTIVE_HIGH ? HIGH : LOW)
    : (ACTIVE_HIGH ? LOW : HIGH);

  digitalWrite(MAG_PIN, level);
  Serial.println(enabled ? F("CN4: ON") : F("CN4: OFF"));
}

void setup() {
  Serial.begin(115200);
  pinMode(MAG_PIN, OUTPUT);

  setMagnet(false);

  Serial.println();
  Serial.println(F("esp32_cn4_test ready"));
  Serial.println(F("board pin: GPIO8 (PDI -> CN4)"));
  Serial.print(F("trigger mode: "));
  Serial.println(ACTIVE_HIGH ? F("active_high") : F("active_low"));
  Serial.println(F("send 1 = ON for 5s"));
  Serial.println(F("send 0 = OFF now"));
}

void loop() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();

    if (c == '1') {
      setMagnet(true);
      magnetOn = true;
      magnetOffAt = millis() + PULSE_MS;
      Serial.println(F("timer started: 5 s"));
    } else if (c == '0') {
      setMagnet(false);
      magnetOn = false;
      magnetOffAt = 0;
    }
  }

  if (magnetOn && (long)(millis() - magnetOffAt) >= 0) {
    setMagnet(false);
    magnetOn = false;
    magnetOffAt = 0;
    Serial.println(F("timer done"));
  }
}
