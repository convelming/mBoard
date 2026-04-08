/*
  uno_mosfet_test.ino

  Purpose:
    - Arduino Uno drives a MOSFET gate from D3.
    - Send '1' over serial to turn the electromagnet ON for 5 seconds.
    - It will then turn OFF automatically.
    - Send '0' over serial to force it OFF immediately.

  Wiring assumption:
    - Arduino D3 -> MOSFET gate control input
    - Arduino GND -> MOSFET module GND / power GND
    - Electromagnet power is supplied externally through the MOSFET path
*/

#include <Arduino.h>

static const uint8_t MOSFET_PIN = 3;
static const uint8_t STATUS_LED_PIN = LED_BUILTIN;
// Most UNO MOSFET modules are "low-side (negative-side) switching",
// but their control input is typically active-HIGH.
// If your specific module is low-level trigger, set this to true.
static const bool ACTIVE_LOW = false;
static const unsigned long PULSE_MS = 5000;

bool magnetOn = false;
unsigned long magnetOffAt = 0;

void setMagnet(bool enabled) {
  const uint8_t level = enabled
    ? (ACTIVE_LOW ? LOW : HIGH)
    : (ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(MOSFET_PIN, level);
  digitalWrite(STATUS_LED_PIN, enabled ? HIGH : LOW);
  Serial.println(enabled ? F("Electromagnet: ON") : F("Electromagnet: OFF"));
}

void setup() {
  Serial.begin(115200);
  pinMode(MOSFET_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  // Default to OFF at boot.
  setMagnet(false);

  Serial.println(F("uno_mosfet_test ready"));
  Serial.println(F("mosfet pin = D3"));
  Serial.println(F("status led = LED_BUILTIN"));
  Serial.print(F("trigger mode = "));
  Serial.println(ACTIVE_LOW ? F("active_low") : F("active_high"));
  Serial.println(F("send 1 = ON for 5s"));
  Serial.println(F("send 0 = OFF now"));
  Serial.println(F("serial baud = 115200"));
}

void loop() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '1') {
      setMagnet(true);
      magnetOn = true;
      magnetOffAt = millis() + PULSE_MS;
      Serial.println(F("timer started: 5000 ms"));
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
