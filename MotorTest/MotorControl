#include "MotorControl.h"

/* =========================
   CONFIGURATION
   ========================= */

const int COUNTS_PER_REV = 1176 * 4;
const int CONTROL_PERIOD_MS = 5;
const float KP = 0.4;
const int PWM_FREQ = 20000;
const int PWM_RES = 8;
const int MAX_PWM = 255;

const int DRIVER1_STBY = 5;

/* =========================
   MOTOR DEFINITIONS
   ========================= */

Motor MotorControl::motors[2] = {
  { 6, 7, 8, 0, 17, 18, 0, 0, 0, 1 },
  { 4, 3, 2, 1, 19, 20, 0, 0, 0, 1 }
};

/* =========================
   ENCODER ISRs
   ========================= */

void IRAM_ATTR encoderISR0() {
  MotorControl::motors[0].encoderCount += digitalRead(MotorControl::motors[0].encB) ? 1 : -1;
}

void IRAM_ATTR encoderISR1() {
  MotorControl::motors[1].encoderCount += digitalRead(MotorControl::motors[1].encB) ? 1 : -1;
}

/* =========================
   INITIALIZATION
   ========================= */

void MotorControl::begin() {
  pinMode(DRIVER1_STBY, OUTPUT);
  digitalWrite(DRIVER1_STBY, LOW);

  for (int i = 0; i < 2; i++) {
    pinMode(motors[i].in1, OUTPUT);
    pinMode(motors[i].in2, OUTPUT);

    pinMode(motors[i].encA, INPUT_PULLUP);
    pinMode(motors[i].encB, INPUT_PULLUP);

    ledcSetup(motors[i].pwmChannel, PWM_FREQ, PWM_RES);
    ledcAttachPin(motors[i].pwmPin, motors[i].pwmChannel);
  }

  attachInterrupt(digitalPinToInterrupt(motors[0].encA), encoderISR0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motors[1].encA), encoderISR1, CHANGE);
}

/* =========================
   START MOTORS (NEW)
   ========================= */

void MotorControl::startMotors(float degrees) {
  digitalWrite(DRIVER1_STBY, HIGH);
  float* degPtr = new float(degrees);
  // Launch a one‑shot task that rotates motors by "degrees"
  xTaskCreatePinnedToCore(
    MotorControl::motionTask,
    "MotionControl",
    4096,
    degPtr,  // pass degrees to task
    2,
    nullptr,
    1);
}

/* =========================
   MOTION FUNCTIONS
   ========================= */

void MotorControl::startMotion(float degrees) {
  long deltaCounts = (degrees / 360.0) * COUNTS_PER_REV;

  for (int i = 0; i < 2; i++) {
    motors[i].startCount = motors[i].encoderCount;
    motors[i].targetCount = motors[i].startCount + deltaCounts * motors[i].direction;

    if (motors[i].direction > 0) {
      digitalWrite(motors[i].in1, HIGH);
      digitalWrite(motors[i].in2, LOW);
    } else {
      digitalWrite(motors[i].in1, LOW);
      digitalWrite(motors[i].in2, HIGH);
    }
  }
}

bool MotorControl::motionComplete() {
  for (int i = 0; i < 2; i++) {
    if (abs(motors[i].targetCount - motors[i].encoderCount) > 3) {
      return false;
    }
  }
  digitalWrite(DRIVER1_STBY, LOW);
  return true;
}

void MotorControl::stopAllMotors() {
  for (int i = 0; i < 2; i++) {
    ledcWrite(motors[i].pwmChannel, 0);
  }
  digitalWrite(DRIVER1_STBY, LOW);
}

/* =========================
   MOTION CONTROL TASK
   ========================= */

void MotorControl::motionTask(void* pv) {
  float degrees = *(float*)pv;
  delete (float*)pv;

  startMotion(degrees);

  while (!motionComplete()) {
    for (int i = 0; i < 2; i++) {
      long error = motors[i].targetCount - motors[i].encoderCount;
      int pwm = constrain(abs(error) * KP, 0, MAX_PWM);
      ledcWrite(motors[i].pwmChannel, pwm);
    }
    vTaskDelay(CONTROL_PERIOD_MS / portTICK_PERIOD_MS);
  }

  stopAllMotors();

  vTaskDelete(nullptr);  // task ends after one motion
}
