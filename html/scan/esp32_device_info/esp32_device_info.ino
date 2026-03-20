#include <Arduino.h>
#include <WiFi.h>

// Dedicated sketch for PID page USB read:
// - prints JSON with eFuse-derived deviceId/mac/chipModel/fwVersion
// - prints once on boot and when serial command "INFO" is received

static const char* FW_VERSION = "A1.0.0";

String getDeviceIdFromEfuse() {
  uint64_t mac = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof(id), "MT%012llX", mac & 0xFFFFFFFFFFFFULL);
  return String(id);
}

String getMacText() {
  uint64_t mac = ESP.getEfuseMac();
  uint8_t b[6];
  for (int i = 0; i < 6; i++) {
    b[5 - i] = (mac >> (8 * i)) & 0xFF;
  }
  char out[18];
  snprintf(out, sizeof(out), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return String(out);
}

void printInfoJson() {
  String payload = "{\"deviceId\":\"" + getDeviceIdFromEfuse() +
                   "\",\"mac\":\"" + getMacText() +
                   "\",\"chipModel\":\"" + String(ESP.getChipModel()) +
                   "\",\"fwVersion\":\"" + String(FW_VERSION) + "\"}";
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  printInfoJson();
}

void loop() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "INFO") {
      printInfoJson();
    }
  }
  delay(50);
}
