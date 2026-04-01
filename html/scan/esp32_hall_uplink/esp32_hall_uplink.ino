#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Wire.h>
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
 * 1) This sketch supports real MCP23017 hall scan on dual I2C.
 * 2) Set HALL_USE_MOCK=1 only for pure simulation.
 */

// BLE UUIDs must match scan/public/scan.html
static const char* SERVICE_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a01";
static const char* CHAR_DEVICE_ID_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a02";
static const char* CHAR_PROVISION_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a03";
static const char* CHAR_STATUS_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a04";
static const char* CHAR_COMMAND_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a05";
static const char* CHAR_WIFI_LIST_UUID = "2f57c5fe-910f-4de9-8d0f-a7efb7b89a06";

// Hall matrix config
// Board sensing area defaults to 10x11.
// You can reserve extra rows/cols for captured-piece parking area.
static const int HALL_BOARD_ROWS = 10;
static const int HALL_BOARD_COLS = 11;
static const int HALL_EXTRA_ROWS = 0;
static const int HALL_EXTRA_COLS = 0;

static const int HALL_ROWS = HALL_BOARD_ROWS + HALL_EXTRA_ROWS;
static const int HALL_COLS = HALL_BOARD_COLS + HALL_EXTRA_COLS;
static const int HALL_CELLS = HALL_ROWS * HALL_COLS;
// Binary hall values (0/1): any toggle should trigger changed=true.
static const int HALL_CHANGE_THRESHOLD = 0;
static const uint32_t HALL_PUSH_INTERVAL_DEFAULT_MS = 5000;
static const uint32_t HALL_PUSH_INTERVAL_MIN_MS = 100;
static const uint32_t HALL_PUSH_INTERVAL_MAX_MS = 60000;
static const uint32_t HALL_CONFIG_PULL_INTERVAL_MS = 15000;
#define HALL_USE_MOCK 0

// Dual-I2C wiring from SCH_PAS-SET_SCH_2026-01-14.pdf:
// IIC1 -> SDA=IO4, SCL=IO5
// IIC2 -> SDA=IO6, SCL=IO7
static const int IIC1_SDA_PIN = 4;
static const int IIC1_SCL_PIN = 5;
static const int IIC2_SDA_PIN = 6;
static const int IIC2_SCL_PIN = 7;
static const uint32_t I2C_FREQ_HZ = 100000;

// MCP23017 address range is 0x20~0x27 per bus.
// For 10 rows on two buses, default mapping is:
// row0~5 -> IIC1 addr 0x20~0x25, row6~9 -> IIC2 addr 0x20~0x23.
// If your hardware differs, edit the two arrays below.
static const int MCP_DEFAULT_ROWS = 10;
static const uint8_t MCP_ROW_BUS_DEFAULT[MCP_DEFAULT_ROWS] = {
  1, 1, 1, 1, 1, 1, 2, 2, 2, 2
};
static const uint8_t MCP_ROW_ADDR_DEFAULT[MCP_DEFAULT_ROWS] = {
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x20, 0x21, 0x22, 0x23
};

bool gHallActiveLow = true;
// Column-map mode 0 (default): from MCP schematic
// 11 hall columns wired as GPB0~GPB7 + GPA7~GPA5.
static const uint8_t HALL_COL_TO_MCP_BIT_MODE0[HALL_BOARD_COLS] = {
  8, 9, 10, 11, 12, 13, 14, 15, 7, 6, 5
};
// Column-map mode 1: compatibility with legacy test numbering
// (GPA0~GPA7 + GPB5~GPB7)
static const uint8_t HALL_COL_TO_MCP_BIT_MODE1[HALL_BOARD_COLS] = {
  0, 1, 2, 3, 4, 5, 6, 7, 13, 14, 15
};
static const uint8_t MCP_REG_IODIRA = 0x00;
static const uint8_t MCP_REG_IODIRB = 0x01;
static const uint8_t MCP_REG_GPPUA = 0x0C;
static const uint8_t MCP_REG_GPPUB = 0x0D;
static const uint8_t MCP_REG_GPIOA = 0x12;
static const uint8_t MCP_REG_GPIOB = 0x13;

uint8_t gRowBusNo[HALL_ROWS];
uint8_t gRowAddr[HALL_ROWS];
bool gRowPresent[HALL_ROWS];
uint16_t gRowBits[HALL_ROWS];
int gColMapMode = 0;

Preferences prefs;
BLECharacteristic* charStatus = nullptr;
BLECharacteristic* charWifiList = nullptr;

String gDeviceId;
String gFwVersion = "A2.0.0";
String gProductId = "";
String gBackendUrl = "http://192.168.1.3:8866";
String gSoftApSsid = "mTabula-Setup";
WebServer gProvisionServer(80);

int gLastValues[HALL_CELLS];
uint32_t gLastHallPushMs = 0;
uint32_t gLastHeartbeatMs = 0;
uint32_t gHallPushIntervalMs = HALL_PUSH_INTERVAL_DEFAULT_MS;
uint32_t gLastHallConfigPullMs = 0;
bool gWifiReady = false;

String trimTrailingSlash(String text) {
  text.trim();
  while (text.endsWith("/")) {
    text.remove(text.length() - 1);
  }
  return text;
}

bool isValidBackendUrl(const String& backendUrl) {
  String u = backendUrl;
  u.trim();
  if (u.length() == 0) return false;

  String lower = u;
  lower.toLowerCase();
  if (!lower.startsWith("http://") && !lower.startsWith("https://")) return false;

  // ESP32 cannot reach backend if it is localhost on itself.
  if (lower.indexOf("localhost") >= 0 || lower.indexOf("127.0.0.1") >= 0) return false;
  return true;
}

uint32_t clampHallPushIntervalMs(uint32_t ms) {
  if (ms < HALL_PUSH_INTERVAL_MIN_MS) return HALL_PUSH_INTERVAL_MIN_MS;
  if (ms > HALL_PUSH_INTERVAL_MAX_MS) return HALL_PUSH_INTERVAL_MAX_MS;
  return ms;
}

uint32_t parseHallPushIntervalMs(const String& raw, uint32_t fallback) {
  String text = raw;
  text.trim();
  if (text.length() == 0) return fallback;
  for (int i = 0; i < text.length(); i++) {
    char ch = text.charAt(i);
    if (ch < '0' || ch > '9') return fallback;
  }
  unsigned long v = text.toInt();
  if (v == 0) return fallback;
  return clampHallPushIntervalMs((uint32_t)v);
}

void sendCors() {
  gProvisionServer.sendHeader("Access-Control-Allow-Origin", "*");
  gProvisionServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  gProvisionServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
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

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in.charAt(i);
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
  return out;
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
  Serial.printf("wifi connect start: ssid=%s timeout=%lu\n", ssid.c_str(), (unsigned long)timeoutMs);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("wifi connected: ip=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("wifi connect failed");
  }
  return WiFi.status() == WL_CONNECTED;
}

void loadConfig() {
  prefs.begin("mt_cfg", true);
  gProductId = prefs.getString("product", "");
  gBackendUrl = prefs.getString("backend", gBackendUrl);
  gHallPushIntervalMs = clampHallPushIntervalMs(prefs.getUInt("hall_ms", HALL_PUSH_INTERVAL_DEFAULT_MS));
  String ssid = prefs.getString("ssid", "");
  String pwd = prefs.getString("pwd", "");
  prefs.end();

  Serial.printf("cfg productId=%s backendUrl=%s hallPushMs=%lu ssid=%s\n",
                gProductId.c_str(), gBackendUrl.c_str(),
                (unsigned long)gHallPushIntervalMs, ssid.c_str());

  if (ssid.length() > 0) {
    bool ok = connectWiFi(ssid, pwd, 12000);
    gWifiReady = ok;
    if (ok) {
      notifyStatus("connected", "wifi reconnected", WiFi.localIP().toString());
    }
  }
}

void saveProvision(const String& productId, const String& ssid, const String& password,
                   const String& backendUrl, const String& hallIntervalMsText) {
  gHallPushIntervalMs = parseHallPushIntervalMs(hallIntervalMsText, gHallPushIntervalMs);
  prefs.begin("mt_cfg", false);
  prefs.putString("product", productId);
  prefs.putString("ssid", ssid);
  prefs.putString("pwd", password);
  prefs.putUInt("hall_ms", gHallPushIntervalMs);
  if (isValidBackendUrl(backendUrl)) {
    prefs.putString("backend", trimTrailingSlash(backendUrl));
  }
  prefs.end();
  Serial.printf("hall push interval set: %lu ms\n", (unsigned long)gHallPushIntervalMs);
}

void handleProvisionOptions() {
  sendCors();
  gProvisionServer.send(204);
}

void handleProvisionHealth() {
  sendCors();
  String body = "{\"ok\":true,\"deviceId\":\"" + gDeviceId + "\",\"apSsid\":\"" + gSoftApSsid + "\"}";
  gProvisionServer.send(200, "application/json", body);
}

void handleProvisionRoot() {
  sendCors();
  String body = "mTabula SoftAP ready. POST /provision with JSON: {productId,ssid,password,backendUrl,hallIntervalMs}";
  gProvisionServer.send(200, "text/plain", body);
}

String wifiListJson(int maxItems, int maxBytes, bool includeOkField) {
  WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks();
  String payload = includeOkField ? "{\"ok\":true,\"list\":[" : "{\"list\":[";
  int count = 0;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    String item = "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    int extraComma = count > 0 ? 1 : 0;
    if (maxBytes > 0 && (int)(payload.length() + extraComma + item.length() + 2) > maxBytes) break;
    if (count > 0) payload += ",";
    payload += item;
    count++;
    if (maxItems > 0 && count >= maxItems) break;
  }
  payload += "]}";
  WiFi.scanDelete();
  return payload;
}

void handleWifiScan() {
  sendCors();
  gProvisionServer.send(200, "application/json", wifiListJson(20, 4096, true));
}

void handleProvisionPost() {
  String raw = gProvisionServer.arg("plain");
  String productId = jsonGet(raw, "productId");
  String ssid = jsonGet(raw, "ssid");
  String password = jsonGet(raw, "password");
  String backend = jsonGet(raw, "backendUrl");
  String hallIntervalMs = jsonGet(raw, "hallIntervalMs");

  if (productId.length() == 0) {
    sendCors();
    gProvisionServer.send(400, "application/json", "{\"ok\":false,\"error\":\"missing productId\"}");
    return;
  }
  if (ssid.length() == 0) {
    sendCors();
    gProvisionServer.send(400, "application/json", "{\"ok\":false,\"error\":\"missing ssid\"}");
    return;
  }

  saveProvision(productId, ssid, password, backend, hallIntervalMs);
  gProductId = productId;
  if (isValidBackendUrl(backend)) {
    gBackendUrl = trimTrailingSlash(backend);
    Serial.printf("backendUrl updated: %s\n", gBackendUrl.c_str());
  } else if (backend.length() > 0) {
    Serial.printf("ignore invalid backendUrl: %s\n", backend.c_str());
  }

  notifyStatus("connecting", "wifi connecting");
  bool ok = connectWiFi(ssid, password);
  gWifiReady = ok;
  if (ok) {
    notifyStatus("connected", "wifi connected", WiFi.localIP().toString());
    sendHeartbeat();
    pushHallFrame(true);
    sendCors();
    gProvisionServer.send(200, "application/json", "{\"ok\":true,\"stage\":\"connected\"}");
  } else {
    notifyStatus("failed", "wifi connect failed");
    sendCors();
    gProvisionServer.send(500, "application/json", "{\"ok\":false,\"error\":\"wifi connect failed\"}");
  }
}

void setupSoftApProvision() {
  gSoftApSsid = "mTabula-" + gDeviceId.substring(gDeviceId.length() - 4);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(gSoftApSsid.c_str());
  Serial.printf("SoftAP started: SSID=%s IP=%s\n", gSoftApSsid.c_str(), WiFi.softAPIP().toString().c_str());

  gProvisionServer.on("/", HTTP_GET, handleProvisionRoot);
  gProvisionServer.on("/health", HTTP_GET, handleProvisionHealth);
  gProvisionServer.on("/wifi-scan", HTTP_GET, handleWifiScan);
  gProvisionServer.on("/provision", HTTP_OPTIONS, handleProvisionOptions);
  gProvisionServer.on("/provision", HTTP_POST, handleProvisionPost);
  gProvisionServer.begin();
}

void notifyWifiList() {
  if (!charWifiList) return;
  String payload = wifiListJson(6, 180, false);
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

void pullHallConfigFromBackend(bool forcePull = false) {
  if (!gWifiReady || gProductId.length() == 0) return;
  uint32_t nowMs = millis();
  if (!forcePull && (nowMs - gLastHallConfigPullMs) < HALL_CONFIG_PULL_INTERVAL_MS) return;
  gLastHallConfigPullMs = nowMs;

  String url = gBackendUrl + "/api/boards/" + gProductId + "/hall-config";
  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  if (code >= 200 && code < 300) {
    String body = http.getString();
    String msText = jsonGet(body, "hallIntervalMsText");
    if (msText.length() > 0) {
      uint32_t nextMs = parseHallPushIntervalMs(msText, gHallPushIntervalMs);
      if (nextMs != gHallPushIntervalMs) {
        gHallPushIntervalMs = nextMs;
        prefs.begin("mt_cfg", false);
        prefs.putUInt("hall_ms", gHallPushIntervalMs);
        prefs.end();
        Serial.printf("hall push interval pulled from backend: %lu ms\n", (unsigned long)gHallPushIntervalMs);
      }
    }
  } else if (forcePull) {
    Serial.printf("hall-config pull failed: code=%d url=%s\n", code, url.c_str());
  }
  http.end();
}

void sendHeartbeat() {
  if (!gWifiReady || gProductId.length() == 0) return;
  String url = gBackendUrl + "/api/boards/" + gProductId + "/heartbeat";
  String body = "{\"deviceId\":\"" + gDeviceId +
                "\",\"fwVersion\":\"" + gFwVersion +
                "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  int code = 0;
  bool ok = postJson(url, body, &code);
  if (!ok) {
    Serial.printf("heartbeat failed: code=%d url=%s\n", code, url.c_str());
  }
}

TwoWire* pickI2cBus(uint8_t busNo) {
  return (busNo == 1) ? &Wire : &Wire1;
}

bool mcpWriteReg(TwoWire* bus, uint8_t addr, uint8_t reg, uint8_t value) {
  if (!bus) return false;
  bus->beginTransmission(addr);
  bus->write(reg);
  bus->write(value);
  return bus->endTransmission() == 0;
}

bool mcpReadReg(TwoWire* bus, uint8_t addr, uint8_t reg, uint8_t* out) {
  if (!bus || !out) return false;
  bus->beginTransmission(addr);
  bus->write(reg);
  if (bus->endTransmission(false) != 0) return false;
  int n = bus->requestFrom((int)addr, 1);
  if (n != 1) return false;
  *out = bus->read();
  return true;
}

bool mcpSetupInputs(TwoWire* bus, uint8_t addr) {
  if (!mcpWriteReg(bus, addr, MCP_REG_IODIRA, 0xFF)) return false;
  if (!mcpWriteReg(bus, addr, MCP_REG_IODIRB, 0xFF)) return false;
  // Keep line stable for open-drain style outputs.
  if (!mcpWriteReg(bus, addr, MCP_REG_GPPUA, 0xFF)) return false;
  if (!mcpWriteReg(bus, addr, MCP_REG_GPPUB, 0xFF)) return false;
  return true;
}

bool mcpReadBits(TwoWire* bus, uint8_t addr, uint16_t* outBits) {
  if (!outBits) return false;
  uint8_t a = 0;
  uint8_t b = 0;
  if (!mcpReadReg(bus, addr, MCP_REG_GPIOA, &a)) return false;
  if (!mcpReadReg(bus, addr, MCP_REG_GPIOB, &b)) return false;
  *outBits = ((uint16_t)b << 8) | a;
  return true;
}

void setupHallMappingDefaults() {
  for (int r = 0; r < HALL_ROWS; r++) {
    gRowBusNo[r] = 0;
    gRowAddr[r] = 0;
    gRowPresent[r] = false;
    gRowBits[r] = 0;
  }

  int n = HALL_ROWS < MCP_DEFAULT_ROWS ? HALL_ROWS : MCP_DEFAULT_ROWS;
  for (int r = 0; r < n; r++) {
    gRowBusNo[r] = MCP_ROW_BUS_DEFAULT[r];
    gRowAddr[r] = MCP_ROW_ADDR_DEFAULT[r];
  }
}

void printI2cScan(uint8_t busNo) {
  TwoWire* bus = pickI2cBus(busNo);
  Serial.printf("iic%u scan:", busNo);
  bool found = false;
  for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
    bus->beginTransmission(addr);
    uint8_t err = bus->endTransmission();
    if (err == 0) {
      Serial.printf(" 0x%02X", addr);
      found = true;
    }
  }
  if (!found) Serial.print(" (none)");
  Serial.println();
}

void setupHallHardware() {
  setupHallMappingDefaults();
  Wire.begin(IIC1_SDA_PIN, IIC1_SCL_PIN, I2C_FREQ_HZ);
  Wire1.begin(IIC2_SDA_PIN, IIC2_SCL_PIN, I2C_FREQ_HZ);

  Serial.printf("hall-i2c setup: iic1(sda=%d,scl=%d) iic2(sda=%d,scl=%d)\n",
                IIC1_SDA_PIN, IIC1_SCL_PIN, IIC2_SDA_PIN, IIC2_SCL_PIN);
  Serial.println("note: MCP23017 valid address range is 0x20~0x27 per i2c bus");
  printI2cScan(1);
  printI2cScan(2);

  for (int r = 0; r < HALL_ROWS; r++) {
    uint8_t busNo = gRowBusNo[r];
    uint8_t addr = gRowAddr[r];
    if (busNo < 1 || busNo > 2 || addr < 0x20 || addr > 0x27) {
      Serial.printf("hall row[%d] mapping invalid (bus=%u addr=0x%02X)\n", r, busNo, addr);
      continue;
    }
    TwoWire* bus = pickI2cBus(busNo);
    bool ok = mcpSetupInputs(bus, addr);
    gRowPresent[r] = ok;
    Serial.printf("hall row[%d] -> iic%u addr=0x%02X %s\n",
                  r, busNo, addr, ok ? "ok" : "missing");
  }
}

void refreshHallRows() {
  for (int r = 0; r < HALL_ROWS; r++) {
    if (!gRowPresent[r]) {
      gRowBits[r] = 0;
      continue;
    }
    TwoWire* bus = pickI2cBus(gRowBusNo[r]);
    uint16_t bits = 0;
    if (mcpReadBits(bus, gRowAddr[r], &bits)) {
      gRowBits[r] = bits;
    } else {
      gRowBits[r] = 0;
      gRowPresent[r] = false;
      Serial.printf("hall row[%d] read failed, mark offline (iic%u 0x%02X)\n",
                    r, gRowBusNo[r], gRowAddr[r]);
    }
  }
}

void printHallMap() {
  for (int r = 0; r < HALL_ROWS; r++) {
    Serial.printf("row[%d] => iic%u 0x%02X %s\n",
                  r, gRowBusNo[r], gRowAddr[r], gRowPresent[r] ? "online" : "offline");
  }
}

void printHallRaw() {
  refreshHallRows();
  for (int r = 0; r < HALL_ROWS; r++) {
    Serial.printf("row[%d] bits=0x%04X\n", r, gRowBits[r]);
  }
}

void printHallPolarity() {
  Serial.printf("hall polarity: active_%s\n", gHallActiveLow ? "low" : "high");
}

uint8_t mapColToBit(int col) {
  if (col < 0 || col >= HALL_COLS || col >= 16) return 0xFF;
  if (col < HALL_BOARD_COLS) {
    return (gColMapMode == 0) ? HALL_COL_TO_MCP_BIT_MODE0[col] : HALL_COL_TO_MCP_BIT_MODE1[col];
  }
  return (uint8_t)col;
}

void printHallColMap() {
  Serial.printf("hall col-map mode=%d\n", gColMapMode);
  String line = "col->bit:";
  for (int c = 0; c < HALL_BOARD_COLS; c++) {
    line += " ";
    line += String(c);
    line += ">";
    line += String(mapColToBit(c));
  }
  Serial.println(line);
}

int readHallValue(int row, int col) {
#if HALL_USE_MOCK
  // Mock pattern: moving binary hotspot
  int t = (millis() / 800) % (HALL_ROWS + HALL_COLS);
  int centerR = t % HALL_ROWS;
  int centerC = (t * 2) % HALL_COLS;
  int dr = abs(row - centerR);
  int dc = abs(col - centerC);
  int d = dr + dc;
  return (d <= 1) ? 1 : 0;
#else
  if (row < 0 || row >= HALL_ROWS || col < 0 || col >= HALL_COLS || col >= 16) return 0;
  uint16_t bits = gRowBits[row];
  uint8_t bitIndex = mapColToBit(col);
  if (bitIndex >= 16) return 0;
  bool rawHigh = ((bits >> bitIndex) & 0x01) != 0;
  bool active = gHallActiveLow ? (!rawHigh) : rawHigh;
  return active ? 1 : 0;
#endif
}

bool scanHall(int* outValues, int* outActive, int* outMax) {
  int active = 0;
  int maxV = 0;
  bool changed = false;
#if !HALL_USE_MOCK
  refreshHallRows();
#endif
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
  if (!gWifiReady || gProductId.length() == 0) {
    if (forcePush) {
      Serial.printf("skip hall push: wifiReady=%d productId=%s\n",
                    gWifiReady ? 1 : 0, gProductId.c_str());
    }
    return;
  }

  int values[HALL_CELLS];
  int active = 0;
  int maxV = 0;
  bool changed = scanHall(values, &active, &maxV);

  uint32_t nowMs = millis();
  bool periodic = (nowMs - gLastHallPushMs) >= gHallPushIntervalMs;
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
                ",\"mode\":\"binary\"" +
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
    Serial.printf("hall-frame post failed: %d url=%s\n", code, url.c_str());
  }
}

class ProvisionCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String s = characteristic->getValue();
    if (s.length() == 0) return;

    String productId = jsonGet(s, "productId");
    String ssid = jsonGet(s, "ssid");
    String password = jsonGet(s, "password");
    String backend = jsonGet(s, "backendUrl");
    String hallIntervalMs = jsonGet(s, "hallIntervalMs");

    if (productId.length() == 0) {
      notifyStatus("failed", "missing productId");
      return;
    }
    if (ssid.length() == 0) {
      notifyStatus("failed", "missing ssid");
      return;
    }

    saveProvision(productId, ssid, password, backend, hallIntervalMs);
    if (isValidBackendUrl(backend)) {
      gBackendUrl = trimTrailingSlash(backend);
      Serial.printf("backendUrl updated: %s\n", gBackendUrl.c_str());
    } else if (backend.length() > 0) {
      Serial.printf("ignore invalid backendUrl: %s\n", backend.c_str());
    }
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
    String cmd = characteristic->getValue();
    if (cmd.length() == 0) return;
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
  setupHallHardware();
  setupSoftApProvision();
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
      Serial.printf("cfg productId=%s backendUrl=%s hallPushMs=%lu wifi=%d ip=%s pol=%s colMap=%d\n",
                    gProductId.c_str(), gBackendUrl.c_str(),
                    (unsigned long)gHallPushIntervalMs,
                    gWifiReady ? 1 : 0, WiFi.localIP().toString().c_str(),
                    gHallActiveLow ? "active_low" : "active_high", gColMapMode);
      pushHallFrame(true);
    } else if (line == "I2CSCAN") {
      printI2cScan(1);
      printI2cScan(2);
    } else if (line == "HALLMAP") {
      printHallMap();
    } else if (line == "HALLRAW") {
      printHallRaw();
    } else if (line == "HALLPOL") {
      printHallPolarity();
      printHallColMap();
    } else if (line == "HALLPOL0") {
      gHallActiveLow = false;
      printHallPolarity();
      pushHallFrame(true);
    } else if (line == "HALLPOL1") {
      gHallActiveLow = true;
      printHallPolarity();
      pushHallFrame(true);
    } else if (line == "HALLCOL0") {
      gColMapMode = 0;
      printHallColMap();
      pushHallFrame(true);
    } else if (line == "HALLCOL1") {
      gColMapMode = 1;
      printHallColMap();
      pushHallFrame(true);
    } else if (line == "HALLINT") {
      Serial.printf("hall push interval: %lu ms\n", (unsigned long)gHallPushIntervalMs);
    } else if (line.startsWith("HALLINT=")) {
      String v = line.substring(8);
      uint32_t nextMs = parseHallPushIntervalMs(v, gHallPushIntervalMs);
      gHallPushIntervalMs = nextMs;
      prefs.begin("mt_cfg", false);
      prefs.putUInt("hall_ms", gHallPushIntervalMs);
      prefs.end();
      Serial.printf("hall push interval updated: %lu ms\n", (unsigned long)gHallPushIntervalMs);
    } else if (line == "HALLCFG") {
      pullHallConfigFromBackend(true);
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

  pullHallConfigFromBackend(false);

  pushHallFrame(false);
  gProvisionServer.handleClient();
  delay(80);
}
