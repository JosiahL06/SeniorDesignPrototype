#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

struct Motor {
  int in1, in2;
  int pwmPin;
  int pwmChannel;
  int encA, encB;

  volatile long encoderCount;
  long startCount;
  long targetCount;
  int direction;
};

namespace MotorControl {

extern Motor motors[2];

void begin();                     // hardware only
void startMotors(float degrees);  // rotate motors by given degrees
void startMotion(float deg);      // internal helper
bool motionComplete();
void stopAllMotors();
void motionTask(void *pv);  // executes one motion then exits

}

#endif
