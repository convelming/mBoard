/*
  motor_key_hold_test.ino
  Purpose:
    - Standalone two-motor test for A4988 + CoreXY drivetrain.
    - Keyboard control over USB serial:
        'w' = Motor1 CW  + Motor2 CW
        's' = Motor1 CCW + Motor2 CCW
        'a' = Motor1 CCW + Motor2 CW
        'd' = Motor1 CW  + Motor2 CCW
    - Release key -> no more repeated key bytes -> auto stop by deadman timeout.

  Notes:
    - Set Serial Monitor to 115200 baud, "No line ending".
    - This sketch expects key repeat from terminal while key is held.
    - If direction is opposite, flip MOTOR*_CW_DIR_LEVEL below.
*/

#include <Arduino.h>

// -----------------------------
// Pin mapping (edit to your PCB)
// -----------------------------
// From SCH_PAS-SET_SCH_2026-01-14:
//   DTR1 -> IO15, STE1 -> IO16, DTR0 -> IO17, STE0 -> IO18
static const int MOTOR1_STEP_PIN = 18;  // STE0
static const int MOTOR1_DIR_PIN  = 17;  // DTR0
static const int MOTOR2_STEP_PIN = 16;  // STE1
static const int MOTOR2_DIR_PIN  = 15;  // DTR1

// Optional shared EN pin for A4988 (active LOW). Set to -1 if tied to GND.
static const int MOTOR_ENABLE_PIN = -1;

// Direction level for CW per motor (adjust if reversed).
static const bool MOTOR1_CW_DIR_LEVEL = HIGH;
static const bool MOTOR2_CW_DIR_LEVEL = HIGH;

// Step pulse timing.121231
static const uint32_t STEP_PULSE_US = 4;      // A4988 min >1us
static const uint32_t STEP_PERIOD_US = 1200;  // moderate speed for visible response

// Deadman timeout: if no key byte received within this window, stop motors.
static const uint32_t DEADMAN_TIMEOUT_MS = 2500;

// Boot self-test: each motor CW one rev, then CCW one rev.
// A4988 full-step is usually 200 steps/rev (1.8 deg motor).
// If MS1/MS2/MS3 are configured for microstepping, set MICROSTEPS accordingly.
static const uint16_t FULL_STEPS_PER_REV = 200;
static const uint8_t MICROSTEPS = 1;
static const uint32_t SELFTEST_STEPS_PER_REV = (uint32_t)FULL_STEPS_PER_REV * (uint32_t)MICROSTEPS;
static const uint16_t SELFTEST_PAUSE_MS = 250;
static const bool RUN_SELFTEST_ON_BOOT = true;

struct MotorState {
  int stepPin;
  int dirPin;
  bool cwDirLevel;
  int commandDir;         // -1, 0, +1
  bool stepHigh;
  uint32_t lastEdgeUs;
};

MotorState motor1 = {MOTOR1_STEP_PIN, MOTOR1_DIR_PIN, MOTOR1_CW_DIR_LEVEL, 0, false, 0};
MotorState motor2 = {MOTOR2_STEP_PIN, MOTOR2_DIR_PIN, MOTOR2_CW_DIR_LEVEL, 0, false, 0};

uint32_t gLastCommandMs = 0;

static void applyEnable(bool enable) {
  if (MOTOR_ENABLE_PIN < 0) return;
  // A4988 EN is active LOW.
  digitalWrite(MOTOR_ENABLE_PIN, enable ? LOW : HIGH);
}

static void setMotorCommand(MotorState &m, int dir) {
  if (dir > 0) {
    m.commandDir = 1;
    digitalWrite(m.dirPin, m.cwDirLevel);
  } else if (dir < 0) {
    m.commandDir = -1;
    digitalWrite(m.dirPin, !m.cwDirLevel);
  } else {
    m.commandDir = 0;
    if (m.stepHigh) {
      digitalWrite(m.stepPin, LOW);
      m.stepHigh = false;
    }
  }
}

static void stopAllMotors() {
  setMotorCommand(motor1, 0);
  setMotorCommand(motor2, 0);
  applyEnable(false);
}

static void rotateMotorBlocking(MotorState &m, int dir, uint32_t steps) {
  if (dir == 0 || steps == 0) return;

  digitalWrite(m.dirPin, (dir > 0) ? m.cwDirLevel : !m.cwDirLevel);
  for (uint32_t i = 0; i < steps; i++) {
    digitalWrite(m.stepPin, HIGH);
    delayMicroseconds(STEP_PULSE_US);
    digitalWrite(m.stepPin, LOW);
    delayMicroseconds(STEP_PERIOD_US);
  }
}

static void runBootSelfTest() {
  if (!RUN_SELFTEST_ON_BOOT) return;

  Serial.println(F("Boot self-test: M1 CW/CCW, M2 CW/CCW, each 1 rev."));
  applyEnable(true);

  rotateMotorBlocking(motor1, +1, SELFTEST_STEPS_PER_REV);
  delay(SELFTEST_PAUSE_MS);
  rotateMotorBlocking(motor1, -1, SELFTEST_STEPS_PER_REV);
  delay(SELFTEST_PAUSE_MS);

  rotateMotorBlocking(motor2, +1, SELFTEST_STEPS_PER_REV);
  delay(SELFTEST_PAUSE_MS);
  rotateMotorBlocking(motor2, -1, SELFTEST_STEPS_PER_REV);
  delay(SELFTEST_PAUSE_MS);

  stopAllMotors();
  Serial.println(F("Boot self-test done."));
}

static void handleKeyCommand(char c) {
  switch (c) {
    case 'w':
    case 'W':
      applyEnable(true);
      setMotorCommand(motor1, +1);
      setMotorCommand(motor2, +1);
      Serial.println(F("CMD=W (M1 CW + M2 CW)"));
      break;
    case 's':
    case 'S':
      applyEnable(true);
      setMotorCommand(motor1, -1);
      setMotorCommand(motor2, -1);
      Serial.println(F("CMD=S (M1 CCW + M2 CCW)"));
      break;
    case 'd':
    case 'D':
      applyEnable(true);
      setMotorCommand(motor1, +1);
      setMotorCommand(motor2, -1);
      Serial.println(F("CMD=D (M1 CW + M2 CCW)"));
      break;
    case 'a':
    case 'A':
      applyEnable(true);
      setMotorCommand(motor1, -1);
      setMotorCommand(motor2, +1);
      Serial.println(F("CMD=A (M1 CCW + M2 CW)"));
      break;
    case '0':
    case ' ':
      stopAllMotors();
      Serial.println(F("CMD=STOP"));
      break;
    case 'h':
    case 'H':
    case '?':
      Serial.println();
      Serial.println(F("Keys:"));
      Serial.println(F("  W = M1 CW  + M2 CW"));
      Serial.println(F("  S = M1 CCW + M2 CCW"));
      Serial.println(F("  A = M1 CCW + M2 CW"));
      Serial.println(F("  D = M1 CW  + M2 CCW"));
      Serial.println(F("  0/space = Stop"));
      Serial.println();
      break;
    default:
      // Ignore other bytes.
      break;
  }
}

static void runMotor(MotorState &m, uint32_t nowUs) {
  if (m.commandDir == 0) return;

  if (!m.stepHigh) {
    if ((uint32_t)(nowUs - m.lastEdgeUs) >= STEP_PERIOD_US) {
      digitalWrite(m.stepPin, HIGH);
      m.stepHigh = true;
      m.lastEdgeUs = nowUs;
    }
  } else {
    if ((uint32_t)(nowUs - m.lastEdgeUs) >= STEP_PULSE_US) {
      digitalWrite(m.stepPin, LOW);
      m.stepHigh = false;
      m.lastEdgeUs = nowUs;
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(motor1.stepPin, OUTPUT);
  pinMode(motor1.dirPin, OUTPUT);
  pinMode(motor2.stepPin, OUTPUT);
  pinMode(motor2.dirPin, OUTPUT);

  digitalWrite(motor1.stepPin, LOW);
  digitalWrite(motor2.stepPin, LOW);

  if (MOTOR_ENABLE_PIN >= 0) {
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  }
  applyEnable(false);

  gLastCommandMs = millis();

  Serial.println();
  Serial.println(F("motor_key_hold_test ready."));
  Serial.print(F("M1 STEP/DIR = "));
  Serial.print(MOTOR1_STEP_PIN);
  Serial.print(F("/"));
  Serial.println(MOTOR1_DIR_PIN);
  Serial.print(F("M2 STEP/DIR = "));
  Serial.print(MOTOR2_STEP_PIN);
  Serial.print(F("/"));
  Serial.println(MOTOR2_DIR_PIN);
  if (MOTOR_ENABLE_PIN >= 0) {
    Serial.print(F("EN pin(active LOW) = "));
    Serial.println(MOTOR_ENABLE_PIN);
  } else {
    Serial.println(F("EN pin = disabled in sketch (assume tied LOW)."));
  }
  Serial.print(F("Self-test steps/rev = "));
  Serial.println(SELFTEST_STEPS_PER_REV);
  runBootSelfTest();
  Serial.println(F("Press W/A/S/D to drive BOTH motors. Hold key to keep running."));
  Serial.println(F("Release key -> deadman timeout -> stop."));
  Serial.println(F("Send 'h' for help."));
}

void loop() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    gLastCommandMs = millis();
    handleKeyCommand(c);
  }

  // Deadman stop: if host stops sending key bytes, stop both motors.
  if ((millis() - gLastCommandMs) > DEADMAN_TIMEOUT_MS) {
    if (motor1.commandDir != 0 || motor2.commandDir != 0) {
      stopAllMotors();
    }
  }

  const uint32_t nowUs = micros();
  runMotor(motor1, nowUs);
  runMotor(motor2, nowUs);
}
