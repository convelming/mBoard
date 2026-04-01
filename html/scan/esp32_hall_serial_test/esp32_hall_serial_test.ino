#include <Wire.h>

/*
 * mTabula ESP32-S3 Hall Serial Test
 * - Pure serial diagnostics for dual-I2C MCP23017 hall matrix
 * - No Wi-Fi/BLE/backend dependency
 *
 * Wiring (from project SCH):
 * IIC1: SDA=IO4, SCL=IO5
 * IIC2: SDA=IO6, SCL=IO7
 */

static const int IIC1_SDA_PIN = 4;
static const int IIC1_SCL_PIN = 5;
static const int IIC2_SDA_PIN = 6;
static const int IIC2_SCL_PIN = 7;
static const uint32_t I2C_FREQ_HZ = 100000;

static const int HALL_ROWS = 10;
static const int HALL_COLS = 11;

// Default: 6 MCP on IIC1, 4 MCP on IIC2.
static const uint8_t ROW_BUS[HALL_ROWS] = {
  1, 1, 1, 1, 1, 1, 2, 2, 2, 2
};
static const uint8_t ROW_ADDR[HALL_ROWS] = {
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x20, 0x21, 0x22, 0x23
};

// mode 0: GPB0..GPB7 + GPA7..GPA5
static const uint8_t COL_MAP_MODE0[HALL_COLS] = {8, 9, 10, 11, 12, 13, 14, 15, 7, 6, 5};
// mode 1: GPA0..GPA7 + GPB5..GPB7
static const uint8_t COL_MAP_MODE1[HALL_COLS] = {0, 1, 2, 3, 4, 5, 6, 7, 13, 14, 15};

static const uint8_t MCP_REG_IODIRA = 0x00;
static const uint8_t MCP_REG_IODIRB = 0x01;
static const uint8_t MCP_REG_GPPUA = 0x0C;
static const uint8_t MCP_REG_GPPUB = 0x0D;
static const uint8_t MCP_REG_GPIOA = 0x12;
static const uint8_t MCP_REG_GPIOB = 0x13;

bool gActiveLow = true;
int gColMapMode = 0;
bool gStream = false;
uint32_t gLastStreamMs = 0;
uint32_t gStreamIntervalMs = 300;

bool gRowPresent[HALL_ROWS];
uint16_t gRowBits[HALL_ROWS];
int gLastBinary[HALL_ROWS * HALL_COLS];

TwoWire* busRef(uint8_t busNo) {
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

uint8_t colToBit(int col) {
  if (col < 0 || col >= HALL_COLS) return 0xFF;
  return (gColMapMode == 0) ? COL_MAP_MODE0[col] : COL_MAP_MODE1[col];
}

int hallBinary(int row, int col) {
  if (row < 0 || row >= HALL_ROWS || col < 0 || col >= HALL_COLS) return 0;
  uint8_t bit = colToBit(col);
  if (bit >= 16) return 0;
  bool rawHigh = ((gRowBits[row] >> bit) & 0x01) != 0;
  bool active = gActiveLow ? (!rawHigh) : rawHigh;
  return active ? 1 : 0;
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  HELP       - show this help");
  Serial.println("  INFO       - show current config");
  Serial.println("  I2CSCAN    - scan 0x20~0x27 on IIC1/IIC2");
  Serial.println("  MAP        - show row->bus/address map");
  Serial.println("  RAW        - read and print raw 16-bit row values");
  Serial.println("  READ       - read and print 10x11 binary matrix");
  Serial.println("  STREAM1    - start periodic READ");
  Serial.println("  STREAM0    - stop periodic READ");
  Serial.println("  POL0       - active_high");
  Serial.println("  POL1       - active_low");
  Serial.println("  COL0       - use column map mode 0");
  Serial.println("  COL1       - use column map mode 1");
}

void printInfo() {
  Serial.printf("IIC1 SDA=%d SCL=%d | IIC2 SDA=%d SCL=%d | freq=%lu\n",
                IIC1_SDA_PIN, IIC1_SCL_PIN, IIC2_SDA_PIN, IIC2_SCL_PIN, (unsigned long)I2C_FREQ_HZ);
  Serial.printf("active_%s | colMapMode=%d | stream=%d intervalMs=%lu\n",
                gActiveLow ? "low" : "high", gColMapMode, gStream ? 1 : 0, (unsigned long)gStreamIntervalMs);
}

void printI2CScan(uint8_t busNo) {
  TwoWire* bus = busRef(busNo);
  Serial.printf("IIC%u:", busNo);
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

void setupRows() {
  for (int r = 0; r < HALL_ROWS; r++) {
    gRowPresent[r] = false;
    gRowBits[r] = 0;
    uint8_t busNo = ROW_BUS[r];
    uint8_t addr = ROW_ADDR[r];
    if (busNo < 1 || busNo > 2 || addr < 0x20 || addr > 0x27) {
      Serial.printf("row[%d] invalid mapping bus=%u addr=0x%02X\n", r, busNo, addr);
      continue;
    }
    bool ok = mcpSetupInputs(busRef(busNo), addr);
    gRowPresent[r] = ok;
    Serial.printf("row[%d] -> IIC%u 0x%02X %s\n", r, busNo, addr, ok ? "OK" : "MISS");
  }
}

void printMap() {
  for (int r = 0; r < HALL_ROWS; r++) {
    Serial.printf("row[%d] -> IIC%u 0x%02X %s\n",
                  r, ROW_BUS[r], ROW_ADDR[r], gRowPresent[r] ? "online" : "offline");
  }
  String line = "col->bit:";
  for (int c = 0; c < HALL_COLS; c++) {
    line += " ";
    line += String(c);
    line += ">";
    line += String(colToBit(c));
  }
  Serial.println(line);
}

void refreshRows() {
  for (int r = 0; r < HALL_ROWS; r++) {
    if (!gRowPresent[r]) {
      gRowBits[r] = 0;
      continue;
    }
    uint16_t bits = 0;
    if (mcpReadBits(busRef(ROW_BUS[r]), ROW_ADDR[r], &bits)) {
      gRowBits[r] = bits;
    } else {
      gRowBits[r] = 0;
      gRowPresent[r] = false;
      Serial.printf("row[%d] read fail -> offline\n", r);
    }
  }
}

void printRaw() {
  refreshRows();
  for (int r = 0; r < HALL_ROWS; r++) {
    Serial.printf("row[%d] bits=0x%04X\n", r, gRowBits[r]);
  }
}

int readMatrix(int* out) {
  refreshRows();
  int active = 0;
  for (int r = 0; r < HALL_ROWS; r++) {
    for (int c = 0; c < HALL_COLS; c++) {
      int v = hallBinary(r, c);
      out[r * HALL_COLS + c] = v;
      if (v) active++;
    }
  }
  return active;
}

void printMatrix(const int* mat) {
  Serial.println("binary matrix (1=active):");
  for (int r = 0; r < HALL_ROWS; r++) {
    String line = "";
    for (int c = 0; c < HALL_COLS; c++) {
      if (c) line += " ";
      line += String(mat[r * HALL_COLS + c]);
    }
    Serial.println(line);
  }
}

void readAndPrint(bool onlyOnChange) {
  int mat[HALL_ROWS * HALL_COLS];
  int active = readMatrix(mat);
  bool changed = false;
  for (int i = 0; i < HALL_ROWS * HALL_COLS; i++) {
    if (mat[i] != gLastBinary[i]) {
      changed = true;
      break;
    }
  }
  if (!onlyOnChange || changed) {
    Serial.printf("active=%d\n", active);
    printMatrix(mat);
  }
  for (int i = 0; i < HALL_ROWS * HALL_COLS; i++) gLastBinary[i] = mat[i];
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "HELP") {
    printHelp();
    return;
  }
  if (cmd == "INFO") {
    printInfo();
    return;
  }
  if (cmd == "I2CSCAN") {
    printI2CScan(1);
    printI2CScan(2);
    return;
  }
  if (cmd == "MAP") {
    printMap();
    return;
  }
  if (cmd == "RAW") {
    printRaw();
    return;
  }
  if (cmd == "READ") {
    readAndPrint(false);
    return;
  }
  if (cmd == "STREAM1") {
    gStream = true;
    Serial.println("stream=ON");
    return;
  }
  if (cmd == "STREAM0") {
    gStream = false;
    Serial.println("stream=OFF");
    return;
  }
  if (cmd == "POL0") {
    gActiveLow = false;
    Serial.println("polarity=active_high");
    readAndPrint(false);
    return;
  }
  if (cmd == "POL1") {
    gActiveLow = true;
    Serial.println("polarity=active_low");
    readAndPrint(false);
    return;
  }
  if (cmd == "COL0") {
    gColMapMode = 0;
    Serial.println("colMapMode=0");
    printMap();
    readAndPrint(false);
    return;
  }
  if (cmd == "COL1") {
    gColMapMode = 1;
    Serial.println("colMapMode=1");
    printMap();
    readAndPrint(false);
    return;
  }

  Serial.print("Unknown cmd: ");
  Serial.println(cmd);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(IIC1_SDA_PIN, IIC1_SCL_PIN, I2C_FREQ_HZ);
  Wire1.begin(IIC2_SDA_PIN, IIC2_SCL_PIN, I2C_FREQ_HZ);

  for (int i = 0; i < HALL_ROWS * HALL_COLS; i++) gLastBinary[i] = -1;

  Serial.println("mTabula hall serial test ready");
  printInfo();
  printI2CScan(1);
  printI2CScan(2);
  setupRows();
  printMap();
  printHelp();
}

void loop() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }

  if (gStream) {
    uint32_t now = millis();
    if (now - gLastStreamMs >= gStreamIntervalMs) {
      gLastStreamMs = now;
      readAndPrint(true);
    }
  }
  delay(10);
}
