// =============================
// Arduino Control Code
// Last Updated: 4/4 by Josiah Laakkonen
// TODO:
//  - Fine-tune encoder control of motor movement (slightly inaccurate at higher speed)
//  - Redesign how metrics are measured/calculated (99% chance they are irrelevant/garbage data atm)
//  - Reformat code to move BLE and motor control into different .cpp and .h files
// =============================
#include "MotorPair.h"
#include "Packets.h"
#include "BLE.h"

// =============================
// Motor Initialization/Pin Configuration
// =============================
const int ledcChannelA_1 = 0;
const int ledcChannelB_1 = 1;
const int ledcChannelA_2 = 2;
const int ledcChannelB_2 = 3;
const int freq = 20000;
const int resolution = 8;

// Motor Pair 1
const int PWMA_1 = 6;
const int AIN2_1 = 5;
const int AIN1_1 = 4;
const int STBY_1 = 3;
const int BIN1_1 = 2;
const int BIN2_1 = 0;
const int PWMB_1 = 1;

// Motor Pair 2
const int PWMA_2 = 12;
const int AIN2_2 = 11;
const int AIN1_2 = 10;
const int STBY_2 = 13;
const int BIN1_2 = 9;
const int BIN2_2 = 8;
const int PWMB_2 = 7;

const int countsPerRevolution = 1727 * 2;  // 1727 pulses per revolution * 2 counts per pulse

volatile long enc1_count = 0;
volatile long enc2_count = 0;

MotorPair motorPair1(
  PWMA_1, AIN1_1, AIN2_1,
  PWMB_1, BIN1_1, BIN1_1,
  STBY_1,
  ledcChannelA_1, ledcChannelB_1,
  &enc1_count,
  countsPerRevolution);

MotorPair motorPair2(
  PWMA_2, AIN1_2, AIN2_2,
  PWMB_2, BIN1_2, BIN2_2,
  STBY_2,
  ledcChannelA_2, ledcChannelB_2,
  &enc2_count,
  countsPerRevolution);

// =============================
// Encoder Configuration
// =============================
const int ENCA_1 = 17;
const int ENCB_1 = 18;
const int ENCA_2 = 19;
const int ENCB_2 = 20;

void IRAM_ATTR isrEncA_1() {
  bool b = digitalRead(ENCB_1);
  enc1_count += b ? +1 : -1;
}

void IRAM_ATTR isrEncB_1() {
  bool a = digitalRead(ENCA_1);
  enc1_count += a ? -1 : +1;
}

void IRAM_ATTR isrEncA_2() {
  bool b = digitalRead(ENCB_2);
  enc2_count += b ? +1 : -1;
}

void IRAM_ATTR isrEncB_2() {
  bool a = digitalRead(ENCA_2);
  enc2_count += a ? -1 : +1;
}

// =============================
// Motor Configuration
// =============================
const int motorRPM = 95;  // 95 for robotshop motors, 60 for amazon motors
const int msPerRev = 60000 / motorRPM;

// =============================
// Metrics State
// =============================
uint16_t metricsIntervalMs = 0;

uint32_t rxExpectedSeq = 0;
uint32_t packetsRx = 0;
uint32_t packetsLost = 0;

uint64_t latencySumUs = 0;
uint32_t latencySamples = 0;

uint32_t lastRxTimeUs = 0;
uint64_t jitterSumUs = 0;

uint32_t bytesRx = 0;
uint32_t statsStartMs = 0;

// =============================
// Setup - Initialize BLE Connection
// =============================
void setup() {
  // Configuring motor pins
  for (int i = 0; i <= 13; i++) { pinMode(i, OUTPUT); }

  pinMode(ENCA_1, INPUT_PULLUP);
  pinMode(ENCB_1, INPUT_PULLUP);
  pinMode(ENCA_2, INPUT_PULLUP);
  pinMode(ENCB_2, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(ENCA_1),
    isrEncA_1,
    RISING);
  attachInterrupt(
    digitalPinToInterrupt(ENCB_1),
    isrEncB_1,
    RISING);
  attachInterrupt(
    digitalPinToInterrupt(ENCA_2),
    isrEncA_2,
    RISING);
  attachInterrupt(
    digitalPinToInterrupt(ENCB_2),
    isrEncB_2,
    RISING);

  ledcSetup(ledcChannelA_1, freq, resolution);
  ledcSetup(ledcChannelB_1, freq, resolution);
  ledcSetup(ledcChannelA_2, freq, resolution);
  ledcSetup(ledcChannelB_2, freq, resolution);
  ledcAttachPin(PWMA_1, ledcChannelA_1);
  ledcAttachPin(PWMB_1, ledcChannelB_1);
  ledcAttachPin(PWMA_2, ledcChannelA_2);
  ledcAttachPin(PWMB_2, ledcChannelB_2);

  BLE_init();
}

// =============================
// Loop - Update motor instructions, calculate metrics and send packets
// =============================
void loop() {
  BLE_update();

  motorPair1.update();
  motorPair2.update();

  if (BLE_hasNewCommand()) {
    CommandPacket cmd = BLE_getCommand();

    switch (cmd.commandId) {
      case START_BT:
        metricsIntervalMs = cmd.interval;
        break;

      case STOP_BT:
        metricsIntervalMs = 0;
        break;

      case START_MOTOR:
        motorPair1.start(cmd.degrees, cmd.speed, cmd.reverse);
        motorPair2.start(cmd.degrees, cmd.speed, !cmd.reverse);
        break;

      default:
        break;
    }
  }

  static uint32_t lastMetricsMs = 0;
  if (BLE_isConnected() && BLE_isBtTestRunning() && metricsIntervalMs > 0 && millis() - lastMetricsMs >= metricsIntervalMs) {

    MetricsPacket metrics{};
    metrics.timestampMs = millis();
    BLE_notifyMetrics(metrics);
    lastMetricsMs = millis();
  }
}
