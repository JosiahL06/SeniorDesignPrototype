// =============================
// Motor Testing Code
// Last Updated: 3/29 by Josiah Laakkonen
// NOTE: Untested as of now
// TODO:
//  - add simple BLE connection to allow testing
//  - make circuit diagram to illustrate updated pin connections with encoders
// =============================

#include <Arduino.h>

/* =========================
   CONFIGURATION
   ========================= */

// Encoder counts per motor revolution
const int COUNTS_PER_REV = 1024;

// Control loop period (ms)
const int CONTROL_PERIOD_MS = 5;

// Simple proportional gain
const float KP = 0.4;

// PWM settings
const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int MAX_PWM  = 220;

/* =========================
   MOTOR STRUCT
   ========================= */

struct Motor {
  // Pins
  int in1, in2;
  int pwmPin;
  int pwmChannel;
  int encA, encB;

  // State
  volatile long encoderCount;
  long startCount;
  long targetCount;
  int direction;
};

/* =========================
   MOTOR DEFINITIONS
   Format:
   {IN1, IN2, PWM, ledcChannel, encoder A. encoder B, encoder count, start count, target count, direction}
   
   Only change IN1, IN2, PWM, encoder A, encoder B, direction as needed
   Do not change counts
   ========================= */

Motor motors[4] = {
  {1, 0, 2, 0, 17, 18, 0, 0, 0, 1},
  {3, 4, 5, 1, 19, 20, 0, 0, 0, 1},
  {6, 7, 8, 2, 21, 22, 0, 0, 0, -1},
  {9, 10, 11, 3, 23, 24, 0, 0, 0, -1}
};

// Driver standby pins
const int DRIVER1_STBY = 12;
const int DRIVER2_STBY = 13;

/* =========================
   FORWARD DECLARATIONS
   ========================= */

void motionTask(void *pv);
void startMotion(float degrees);
void stopAllMotors();
bool motionComplete();

/* =========================
   ENCODER ISRs
   ========================= */

void IRAM_ATTR encoderISR0() {
  motors[0].encoderCount += digitalRead(motors[0].encB) ? 1 : -1;
}
void IRAM_ATTR encoderISR1() {
  motors[1].encoderCount += digitalRead(motors[1].encB) ? 1 : -1;
}
void IRAM_ATTR encoderISR2() {
  motors[2].encoderCount += digitalRead(motors[2].encB) ? 1 : -1;
}
void IRAM_ATTR encoderISR3() {
  motors[3].encoderCount += digitalRead(motors[3].encB) ? 1 : -1;
}

/* =========================
   SETUP
   ========================= */

void setup() {
  pinMode(DRIVER1_STBY, OUTPUT);
  pinMode(DRIVER2_STBY, OUTPUT);

  digitalWrite(DRIVER1_STBY, HIGH);
  digitalWrite(DRIVER2_STBY, HIGH);

  for (int i = 0; i < 4; i++) {
    pinMode(motors[i].in1, OUTPUT);
    pinMode(motors[i].in2, OUTPUT);

    pinMode(motors[i].encA, INPUT_PULLUP);
    pinMode(motors[i].encB, INPUT_PULLUP);

    ledcSetup(motors[i].pwmChannel, PWM_FREQ, PWM_RES);
    ledcAttachPin(motors[i].pwmPin, motors[i].pwmChannel);
  }

  attachInterrupt(digitalPinToInterrupt(motors[0].encA), encoderISR0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motors[1].encA), encoderISR1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motors[2].encA), encoderISR2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motors[3].encA), encoderISR3, CHANGE);

  xTaskCreatePinnedToCore(
    motionTask,
    "MotionControl",
    4096,
    nullptr,
    2,
    nullptr,
    1
  );
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}

/* =========================
   MOTION CONTROL TASK
   ========================= */

void motionTask(void *pv) {
  for (;;) {
    startMotion(90.0);

    while (!motionComplete()) {
      for (int i = 0; i < 4; i++) {
        long error = motors[i].targetCount - motors[i].encoderCount;
        int pwm = constrain(abs(error) * KP, 0, MAX_PWM);
        ledcWrite(motors[i].pwmChannel, pwm);
      }
      vTaskDelay(CONTROL_PERIOD_MS / portTICK_PERIOD_MS);
    }

    stopAllMotors();

    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

/* =========================
   MOTION FUNCTIONS
   ========================= */

void startMotion(float degrees) {
  long deltaCounts = (degrees / 360.0) * COUNTS_PER_REV;

  for (int i = 0; i < 4; i++) {
    motors[i].startCount  = motors[i].encoderCount;
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

bool motionComplete() {
  for (int i = 0; i < 4; i++) {
    if (abs(motors[i].targetCount - motors[i].encoderCount) > 3) {
      return false;
    }
  }
  return true;
}

void stopAllMotors() {
  for (int i = 0; i < 4; i++) {
    ledcWrite(motors[i].pwmChannel, 0);
  }
}
``
