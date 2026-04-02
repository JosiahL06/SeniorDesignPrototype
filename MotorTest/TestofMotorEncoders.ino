#include "MotorControl.h"

void setup() {
    MotorControl::begin();
}

void loop() {
    MotorControl::startMotors(45);

    delay(2000);

    MotorControl::stopAllMotors();

    delay(2000);
}
