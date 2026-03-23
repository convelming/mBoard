#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/*
 * mTabula ESP32 Hall Uplink Test
 * - BLE provisioning: productId + wifi ssid/password (+ optional backendUrl)
 * - Persist credentials in NVS (Preferences)
 * - Wi-Fi connect + periodic heartbeat to backend
 * - Hall matrix scan and uplink every 5s or when changed
 *
 * IMPORTANT:
 * 1) This sketch defaults to MOCK hall values (HALL_USE_MOCK=1).
 * 2) Replace readHallValue() with your real hall-array scan code.
 */

// BLE UUIDs must match scan/public/scan.html
static const char* SERVICE_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a01";
static const char* CHAR_DEVICE_ID_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a02";
static const char* CHAR_PROVISION_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a03";
static const char* CHAR_STATUS_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a04";
static const char* CHAR_COMMAND_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a05";
static const char* CHAR_WIFI_LIST_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a06";

// Hall matrix config
static const int HALL_ROWS = 11;
static const int HALL_COLS = 10;
static const int HALL_CELLS = HALL_ROWS * HALL_COLS;
static const int HALL_CHANGE_THRESHOLD = 1; // report immediately if changed more than this
#define HALL_USE_MOCK 1

Preferences prefs;
BLECharacteristic* charStatus = nullptr;
BLECharacteristic* charWifiList = nullptr;

String gDeviceId;
String gFwVersion = "A2.0.0";
String gProductId = "";
String gBackendUrl = "http://192.168.1.100:8866";

int gLastValues[HALL_CELLS];
uint32_t gLastHallPushMs = 0;
uint32_t gLastHeartbeatMs = 0;
bool gWifiReady = false;

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

String getDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof(id), "MT%012llX", mac & 0xFFFFFFFFFFFFULL);
  return String(id);
}

String getMacText() {
  uint64_t mac = ESP.getEfuseMac();
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[5 - i] = (mac >> (8 * i)) & 0xFF;
  char out[18];
  snprintf(out, sizeof(out), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return String(out);
}

void notifyStatus(const String& stage, const String& msg, const String& ip = "") {
  if (!charStatus) return;
  String payload = "{\"stage\":\"" + stage + "\",\"msg\":\"" + msg + "\",\"ip\":\"" + ip + "\"}";
  charStatus->setValue(payload.c_str());
  charStatus->notify();
  Serial.println(payload);
}

void printDeviceInfoJson() {
  String payload = "{\"deviceId\":\"" + gDeviceId +
                   "\",\"mac\":\"" + getMacText() +
                   "\",\"chipModel\":\"" + String(ESP.getChipModel()) +
                   "\",\"fwVersion\":\"" + gFwVersion + "\"}";
  Serial.println(payload);
}

bool connectWiFi(const String& ssid, const String& password, uint32_t timeoutMs = 20000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
    delay(300);
  }
  return WiFi.status() == WL_CONNECTED;
}

void loadConfig() {
  prefs.begin("mt_cfg", true);
  gProductId = prefs.getString("product", "");
  gBackendUrl = prefs.getString("backend", gBackendUrl);
  String ssid = prefs.getString("ssid", "");
  String pwd = prefs.getString("pwd", "");
  prefs.end();

  if (ssid.length() > 0) {
    bool ok = connectWiFi(ssid, pwd, 12000);
    gWifiReady = ok;
    if (ok) {
      notifyStatus("connected", "wifi reconnected", WiFi.localIP().toString());
    }
  }
}

void saveProvision(const String& productId, const String& ssid, const String& password, const String& backendUrl) {
  prefs.begin("mt_cfg", false);
  prefs.putString("product", productId);
  prefs.putString("ssid", ssid);
  prefs.putString("pwd", password);
  if (backendUrl.length() > 0) prefs.putString("backend", backendUrl);
  prefs.end();
}

void notifyWifiList() {
  if (!charWifiList) return;
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
    if (count >= 15) break;
  }
  payload += "]}";
  charWifiList->setValue(payload.c_str());
  charWifiList->notify();
}

bool postJson(const String& url, const String& body, int* codeOut = nullptr) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t*)body.c_str(), body.length());
  if (codeOut) *codeOut = code;
  http.end();
  return code >= 200 && code < 300;
}

void sendHeartbeat() {
  if (!gWifiReady || gProductId.length() == 0) return;
  String url = gBackendUrl + "/api/boards/" + gProductId + "/heartbeat";
  String body = "{\"deviceId\":\"" + gDeviceId +
                "\",\"fwVersion\":\"" + gFwVersion +
                "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  postJson(url, body);
}

int readHallValue(int row, int col) {
#if HALL_USE_MOCK
  // Mock pattern: stable but moving hotspot
  int t = (millis() / 800) % (HALL_ROWS + HALL_COLS);
  int centerR = t % HALL_ROWS;
  int centerC = (t * 2) % HALL_COLS;
  int dr = abs(row - centerR);
  int dc = abs(col - centerC);
  int d = dr + dc;
  int v = 120 - d * 35;
  if (v < 0) v = 0;
  return v;
#else
  // TODO: replace with your real hall matrix scan logic
  // e.g., select row via mux, read columns by ADC/GPIO
  return 0;
#endif
}

bool scanHall(int* outValues, int* outActive, int* outMax) {
  int active = 0;
  int maxV = 0;
  bool changed = false;
  for (int r = 0; r < HALL_ROWS; r++) {
    for (int c = 0; c < HALL_COLS; c++) {
      int idx = r * HALL_COLS + c;
      int v = readHallValue(r, c);
      outValues[idx] = v;
      if (v > 0) active++;
      if (v > maxV) maxV = v;
      if (abs(v - gLastValues[idx]) > HALL_CHANGE_THRESHOLD) changed = true;
    }
  }
  *outActive = active;
  *outMax = maxV;
  return changed;
}

void pushHallFrame(bool forcePush) {
  if (!gWifiReady || gProductId.length() == 0) return;

  int values[HALL_CELLS];
  int active = 0;
  int maxV = 0;
  bool changed = scanHall(values, &active, &maxV);

  uint32_t nowMs = millis();
  bool periodic = (nowMs - gLastHallPushMs) >= 5000;
  if (!forcePush && !changed && !periodic) return;

  String valuesJson = "[";
  for (int i = 0; i < HALL_CELLS; i++) {
    if (i > 0) valuesJson += ",";
    valuesJson += String(values[i]);
  }
  valuesJson += "]";

  String body = "{\"esp32Id\":\"" + gDeviceId +
                "\",\"deviceId\":\"" + gDeviceId +
                "\",\"rows\":" + String(HALL_ROWS) +
                ",\"cols\":" + String(HALL_COLS) +
                ",\"mode\":\"intensity\"" +
                ",\"activeCount\":" + String(active) +
                ",\"maxValue\":" + String(maxV) +
                ",\"values\":" + valuesJson + "}";

  String url = gBackendUrl + "/api/boards/" + gProductId + "/hall-frame";
  int code = 0;
  bool ok = postJson(url, body, &code);
  if (ok) {
    for (int i = 0; i < HALL_CELLS; i++) gLastValues[i] = values[i];
    gLastHallPushMs = nowMs;
  } else {
    Serial.printf("hall-frame post failed: %d\n", code);
  }
}

class ProvisionCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string value = characteristic->getValue();
    if (value.empty()) return;
    String s = String(value.c_str());

    String productId = jsonGet(s, "productId");
    String ssid = jsonGet(s, "ssid");
    String password = jsonGet(s, "password");
    String backend = jsonGet(s, "backendUrl");

    if (productId.length() == 0) {
      notifyStatus("failed", "missing productId");
      return;
    }
    if (ssid.length() == 0) {
      notifyStatus("failed", "missing ssid");
      return;
    }

    saveProvision(productId, ssid, password, backend);
    if (backend.length() > 0) gBackendUrl = backend;
    gProductId = productId;

    notifyStatus("connecting", "wifi connecting");
    bool ok = connectWiFi(ssid, password);
    gWifiReady = ok;
    if (ok) {
      notifyStatus("connected", "wifi connected", WiFi.localIP().toString());
      sendHeartbeat();
      pushHallFrame(true);
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

  BLECharacteristic* charDeviceId = service->createCharacteristic(
      CHAR_DEVICE_ID_UUID, BLECharacteristic::PROPERTY_READ);
  charDeviceId->setValue(gDeviceId.c_str());

  BLECharacteristic* charProvision = service->createCharacteristic(
      CHAR_PROVISION_UUID, BLECharacteristic::PROPERTY_WRITE);
  charProvision->setCallbacks(new ProvisionCallback());

  charStatus = service->createCharacteristic(
      CHAR_STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  charStatus->addDescriptor(new BLE2902());
  charStatus->setValue("{\"stage\":\"idle\",\"msg\":\"ready\"}");

  BLECharacteristic* charCommand = service->createCharacteristic(
      CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  charCommand->setCallbacks(new CommandCallback());

  charWifiList = service->createCharacteristic(
      CHAR_WIFI_LIST_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  charWifiList->addDescriptor(new BLE2902());
  charWifiList->setValue("{\"list\":[]}");

  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  gDeviceId = getDeviceId();
  printDeviceInfoJson();
  for (int i = 0; i < HALL_CELLS; i++) gLastValues[i] = -9999;
  loadConfig();
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

  if (WiFi.status() == WL_CONNECTED) {
    gWifiReady = true;
  } else if (gWifiReady) {
    gWifiReady = false;
    notifyStatus("failed", "wifi disconnected");
  }

  uint32_t nowMs = millis();
  if (gWifiReady && (nowMs - gLastHeartbeatMs) >= 20000) {
    sendHeartbeat();
    gLastHeartbeatMs = nowMs;
  }

  pushHallFrame(false);
  delay(80);
}
