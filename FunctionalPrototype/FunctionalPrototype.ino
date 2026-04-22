// =============================
// Functional Prototype Arduino Control Code
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

const int countsPerRevolution = 1727 * 2; // 1727 pulses per revolution * 2 counts per pulse

volatile long enc1_count = 0;
volatile long enc2_count = 0;

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

MotorPair motorPair1(
    PWMA_1, AIN1_1, AIN2_1,
    PWMB_1, BIN1_1, BIN2_1,
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

void IRAM_ATTR isrEncA_1()
{
  portENTER_CRITICAL_ISR(&encoderMux);
  bool b = digitalRead(ENCB_1);
  enc1_count += b ? +1 : -1;
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR isrEncB_1()
{
  portENTER_CRITICAL_ISR(&encoderMux);
  bool a = digitalRead(ENCA_1);
  enc1_count += a ? -1 : +1;
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR isrEncA_2()
{
  portENTER_CRITICAL_ISR(&encoderMux);
  bool b = digitalRead(ENCB_2);
  enc2_count += b ? +1 : -1;
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR isrEncB_2()
{
  portENTER_CRITICAL_ISR(&encoderMux);
  bool a = digitalRead(ENCA_2);
  enc2_count += a ? -1 : +1;
  portEXIT_CRITICAL_ISR(&encoderMux);
}

// =============================
// Motor Configuration
// =============================
const int motorRPM = 95; // 95 for robotshop motors, 60 for amazon motors
const int msPerRev = 60000 / motorRPM;

// =============================
// BLE Metrics
// =============================
uint32_t metricsIntervalMs = 0;
uint32_t positionIntervalMs = 50;

uint32_t txCount = 0;
uint32_t txBytes = 0;

uint64_t intervalSumUs = 0;
uint64_t intervalSqSumUs = 0;

uint32_t intervalSamples = 0;
uint32_t sendOverruns = 0;

uint64_t lastSendTimeUs = 0;
uint32_t testStartMs = 0;

int32_t lastReportedPosition1Counts = INT32_MIN;
int32_t lastReportedPosition2Counts = INT32_MIN;
uint32_t lastPosition1SendMs = 0;
uint32_t lastPosition2SendMs = 0;

// =============================
// Setup - Initialize BLE Connection
// =============================
void setup()
{
  // Configuring motors
  motorPair1.begin();
  motorPair2.begin();

  // Configuring encoder pins
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

  // Initialize BLE connection
  BLE_init();
}

// =============================
// Loop - Update motor instructions, calculate metrics and send packets
// =============================
void loop()
{
  BLE_update();

  while (BLE_hasNewCommand())
  {
    CommandPacket cmd = BLE_getCommand();

    switch (cmd.commandId)
    {
    case START_BT:
      metricsIntervalMs = cmd.interval;

      txCount = 0;
      txBytes = 0;
      intervalSumUs = 0;
      intervalSqSumUs = 0;
      intervalSamples = 0;
      sendOverruns = 0;
      lastSendTimeUs = 0;
      testStartMs = millis();
      break;

    case STOP_BT:
      metricsIntervalMs = 0;

      if (intervalSamples > 0)
      {
        double meanUs =
            (double)intervalSumUs / intervalSamples;

        double variance =
            ((double)intervalSqSumUs / intervalSamples) -
            (meanUs * meanUs);

        double jitterUs =
            variance > 0 ? sqrt(variance) : 0;

        // Store in final metrics packet or log
      }
      break;

    case START_MOTOR:
      lastReportedPosition1Counts = INT32_MIN;
      lastReportedPosition2Counts = INT32_MIN;
      lastPosition1SendMs = 0;
      lastPosition2SendMs = 0;
      motorPair1.start(cmd.degrees, cmd.speed, cmd.reverse);
      motorPair2.start(cmd.degrees, cmd.speed, !cmd.reverse);
      break;

    case STOP_MOTOR:
      motorPair1.stop();
      motorPair2.stop();
      break;

    default:
      break;
    }
  }

  if (BLE_isConnected())
  {
    uint32_t nowMs = millis();

    int32_t position1Counts = (int32_t)motorPair1.getSignedPositionCounts();
    if (position1Counts != lastReportedPosition1Counts ||
        nowMs - lastPosition1SendMs >= positionIntervalMs)
    {
      BLE_notifyMotorPosition1(position1Counts);
      lastReportedPosition1Counts = position1Counts;
      lastPosition1SendMs = nowMs;
    }

    int32_t position2Counts = (int32_t)motorPair2.getSignedPositionCounts();
    if (position2Counts != lastReportedPosition2Counts ||
        nowMs - lastPosition2SendMs >= positionIntervalMs)
    {
      BLE_notifyMotorPosition2(position2Counts);
      lastReportedPosition2Counts = position2Counts;
      lastPosition2SendMs = nowMs;
    }
  }

  static uint32_t lastMetricsMs = 0;
  if (
      BLE_isConnected() &&
      BLE_isBtTestRunning() &&
      metricsIntervalMs > 0 &&
      millis() - lastMetricsMs >= metricsIntervalMs)
  {
    uint64_t nowUs = micros();

    if (lastSendTimeUs != 0)
    {
      uint64_t deltaUs = nowUs - lastSendTimeUs;
      intervalSumUs += deltaUs;
      intervalSqSumUs += deltaUs * deltaUs;
      intervalSamples++;

      if (deltaUs > (uint64_t)metricsIntervalMs * 1000ULL * 2)
      {
        sendOverruns++;
      }
    }

    lastSendTimeUs = nowUs;

    MetricsPacket metrics{};
    metrics.timestampMs = millis();
    metrics.txCount = txCount + 1;
    metrics.txBytes = txBytes + sizeof(MetricsPacket);

    if (intervalSamples > 0)
    {
      double meanUs =
        (double)intervalSumUs / intervalSamples;

      double variance =
        ((double)intervalSqSumUs / intervalSamples) -
        (meanUs * meanUs);

      metrics.intervalMeanUs =
        (uint32_t)meanUs;
      metrics.intervalJitterUs =
        (uint32_t)(variance > 0 ? sqrt(variance) : 0);
    }
    else
    {
      metrics.intervalMeanUs = 0;
      metrics.intervalJitterUs = 0;
    }

    metrics.sendOverruns = sendOverruns;
    metrics.uptimeMs = millis() - testStartMs;

    BLE_notifyMetrics(metrics);

    txCount++;
    txBytes += sizeof(MetricsPacket);

    lastMetricsMs = millis();
  }
}
