#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UUIDs must match scan/public/scan.html
static const char* SERVICE_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a01";
static const char* CHAR_DEVICE_ID_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a02";
static const char* CHAR_PROVISION_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a03";
static const char* CHAR_STATUS_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a04";
static const char* CHAR_COMMAND_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a05";
static const char* CHAR_WIFI_LIST_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a06";

Preferences prefs;
BLECharacteristic* charDeviceId = nullptr;
BLECharacteristic* charProvision = nullptr;
BLECharacteristic* charStatus = nullptr;
BLECharacteristic* charCommand = nullptr;
BLECharacteristic* charWifiList = nullptr;

String gDeviceId;
String gFwVersion = "A1.0.0";

String getDeviceId() {
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

void printDeviceInfoJson() {
  String chipModel = ESP.getChipModel();
  String json = "{\"deviceId\":\"" + gDeviceId + "\",\"mac\":\"" + getMacText() + "\",\"chipModel\":\"" + chipModel + "\",\"fwVersion\":\"" + gFwVersion + "\"}";
  Serial.println(json);
}

String jsonGet(const String& src, const char* key) {
  String k = String("\"") + key + "\"";
  int p = src.indexOf(k);
  if (p < 0) return "";
  p = src.indexOf(':', p);
  if (p < 0) return "";
  int q1 = src.indexOf('"', p + 1);
  if (q1 < 0) return "";
  int q2 = src.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  return src.substring(q1 + 1, q2);
}

void notifyStatus(const String& stage, const String& msg, const String& ip = "") {
  String payload = "{\"stage\":\"" + stage + "\",\"msg\":\"" + msg + "\",\"ip\":\"" + ip + "\"}";
  charStatus->setValue(payload.c_str());
  charStatus->notify();
  Serial.println(payload);
}

bool connectWiFi(const String& ssid, const String& password, uint32_t timeoutMs = 20000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(300);
  }
  return WiFi.status() == WL_CONNECTED;
}

void notifyWifiList() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  String payload = "{\"list\":[";
  int count = 0;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    if (count > 0) payload += ",";
    payload += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    count++;
    if (count >= 15) break; // keep payload small for BLE notification MTU
  }
  payload += "]}";
  charWifiList->setValue(payload.c_str());
  charWifiList->notify();
}

class ProvisionCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string value = characteristic->getValue();
    if (value.empty()) return;

    String s = String(value.c_str());
    String deviceId = jsonGet(s, "deviceId");
    String ssid = jsonGet(s, "ssid");
    String password = jsonGet(s, "password");

    if (deviceId != gDeviceId) {
      notifyStatus("failed", "device id mismatch");
      return;
    }
    if (ssid.length() == 0) {
      notifyStatus("failed", "missing ssid");
      return;
    }

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pwd", password);
    prefs.end();

    notifyStatus("connecting", "wifi connecting");
    bool ok = connectWiFi(ssid, password);
    if (ok) {
      notifyStatus("connected", "wifi connected", WiFi.localIP().toString());
    } else {
      notifyStatus("failed", "wifi connect failed");
    }
  }
};

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string value = characteristic->getValue();
    if (value.empty()) return;
    String cmd = String(value.c_str());
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "SCAN_WIFI") {
      notifyStatus("scanning", "wifi scanning");
      notifyWifiList();
    }
  }
};

void setupBLE() {
  BLEDevice::init(("mTabula-" + gDeviceId.substring(gDeviceId.length() - 4)).c_str());
  BLEServer* server = BLEDevice::createServer();
  BLEService* service = server->createService(SERVICE_UUID);

  charDeviceId = service->createCharacteristic(
      CHAR_DEVICE_ID_UUID,
      BLECharacteristic::PROPERTY_READ
  );
  charDeviceId->setValue(gDeviceId.c_str());

  charProvision = service->createCharacteristic(
      CHAR_PROVISION_UUID,
      BLECharacteristic::PROPERTY_WRITE
  );
  charProvision->setCallbacks(new ProvisionCallback());

  charStatus = service->createCharacteristic(
      CHAR_STATUS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  charStatus->addDescriptor(new BLE2902());
  charStatus->setValue("{\"stage\":\"idle\",\"msg\":\"ready\"}");

  charCommand = service->createCharacteristic(
      CHAR_COMMAND_UUID,
      BLECharacteristic::PROPERTY_WRITE
  );
  charCommand->setCallbacks(new CommandCallback());

  charWifiList = service->createCharacteristic(
      CHAR_WIFI_LIST_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  charWifiList->addDescriptor(new BLE2902());
  charWifiList->setValue("{\"list\":[]}");

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();
}

void tryAutoReconnectWiFi() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("pwd", "");
  prefs.end();

  if (ssid.length() == 0) return;

  bool ok = connectWiFi(ssid, password, 12000);
  if (ok) {
    Serial.printf("WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi auto reconnect failed");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  gDeviceId = getDeviceId();
  printDeviceInfoJson();

  tryAutoReconnectWiFi();
  setupBLE();
}

void loop() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toUpperCase();
    if (line == "INFO") {
      printDeviceInfoJson();
    }
  }
  delay(200);
}
