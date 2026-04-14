#line 1 "C:\\Users\\Josia\\OneDrive\\Documents\\SeniorDesignPrototype\\FunctionalPrototype\\MotorPair.cpp"
#include "MotorPair.h"

extern portMUX_TYPE encoderMux;

// Constructor
MotorPair::MotorPair(
    int pwma, int ain1, int ain2,
    int pwmb, int bin1, int bin2,
    int stby,
    int ledcA, int ledcB,
    volatile long* encoderCount,
    int countsPerRev
)
: _pwma(pwma), _ain1(ain1), _ain2(ain2),
  _pwmb(pwmb), _bin1(bin1), _bin2(bin2),
  _stby(stby),
  _ledcA(ledcA), _ledcB(ledcB),
  _encoderCount(encoderCount),
  _cpr(countsPerRev)
{}

void MotorPair::begin() {
    if (_taskStarted) return;

    pinMode(_ain1, OUTPUT);
    pinMode(_ain2, OUTPUT);
    pinMode(_bin1, OUTPUT);
    pinMode(_bin2, OUTPUT);
    pinMode(_stby, OUTPUT);
    digitalWrite(_stby, LOW);

    ledcSetup(_ledcA, 20000, 8);
    ledcSetup(_ledcB, 20000, 8);

    ledcAttachPin(_pwma, _ledcA);
    ledcAttachPin(_pwmb, _ledcB);

    _commandQueue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(MotionCommand));
    if (!_commandQueue) {
        return;
    }

    xTaskCreatePinnedToCore(
        taskEntry,
        "MotorPairTask",
        4096,
        this,
        TASK_PRIORITY,
        &_taskHandle,
        1);

    if (_taskHandle == nullptr) {
        vQueueDelete(_commandQueue);
        _commandQueue = nullptr;
        return;
    }

    _taskStarted = true;
}

bool MotorPair::start(uint16_t degrees, uint8_t speedPercent, bool reverse) {
    if (!_taskStarted || !_commandQueue) return false;

    MotionCommand command{degrees, speedPercent, reverse};
    if (xQueueSendToBack(_commandQueue, &command, 0) != pdTRUE) {
        return false;
    }

    _directionSign = reverse ? -1 : 1;
    return true;
}

void MotorPair::update() {
    // Motion is handled by the FreeRTOS task.
}

void MotorPair::stop() {
    if (_taskHandle) {
        xTaskNotifyGive(_taskHandle);
    }
}

bool MotorPair::isRunning() const { return _state == State::RUNNING; }
bool MotorPair::isDone() const    { return _state == State::DONE; }
bool MotorPair::isStalled() const { return _state == State::STALLED; }

long MotorPair::getSignedPositionCounts() const {
    // Report physical encoder direction directly; do not remap by command direction.
    return getPositionCounts();
}

void MotorPair::taskEntry(void* parameter) {
    static_cast<MotorPair*>(parameter)->runTask();
}

void MotorPair::runTask() {
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            flushPendingCommands();
            stopHardware();
            continue;
        }

        MotionCommand command{};
        if (!_commandQueue || xQueueReceive(_commandQueue, &command, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }

        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            flushPendingCommands();
            stopHardware();
            continue;
        }

        executeCommand(command);
    }
}

void MotorPair::setDirection(bool reverse) {
    if (reverse) {
        digitalWrite(_ain1, LOW);
        digitalWrite(_ain2, HIGH);
        digitalWrite(_bin1, HIGH);
        digitalWrite(_bin2, LOW);
    } else {
        digitalWrite(_ain1, HIGH);
        digitalWrite(_ain2, LOW);
        digitalWrite(_bin1, LOW);
        digitalWrite(_bin2, HIGH);
    }
}

void MotorPair::setBrake() {
    digitalWrite(_ain1, LOW);
    digitalWrite(_ain2, LOW);
    digitalWrite(_bin1, LOW);
    digitalWrite(_bin2, LOW);
}

long MotorPair::getPositionCounts() const {
    if (!_encoderCount) return 0;

    portENTER_CRITICAL(&encoderMux);
    long pos = *_encoderCount;
    portEXIT_CRITICAL(&encoderMux);

    return pos;
}

void MotorPair::executeCommand(const MotionCommand& command) {
    _state = State::RUNNING;
    _directionSign = command.reverse ? -1 : 1;
    _targetCounts = (long)command.degrees * _cpr / 360;
    _basePWM = map(command.speedPercent, 1, 100, MIN_PWM, MAX_PWM);
    _basePWM = constrain(_basePWM, MIN_PWM, MAX_PWM);
    _adaptiveBoost = 0;

    portENTER_CRITICAL(&encoderMux);
    if (_encoderCount) {
        *_encoderCount = 0;
    }
    portEXIT_CRITICAL(&encoderMux);

    _lastProgress = _targetCounts;
    _lastSamplePos = 0;
    _lastProgressMs = millis();

    bool driveReverse = command.reverse;
    setDirection(driveReverse);
    digitalWrite(_stby, HIGH);

    long slowZone = _cpr / SLOW_ZONE_DIV;
    if (slowZone < 1) {
        slowZone = 1;
    }

    long finalApproachZone = _cpr / FINAL_APPROACH_DIV;
    if (finalApproachZone < 1) {
        finalApproachZone = 1;
    }

    long brakeZone = _cpr / 96;
    if (brakeZone < 1) {
        brakeZone = 1;
    }

    int precisionCruisePWM = min(_basePWM, PRECISION_PWM_CAP);

    while (true) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            flushPendingCommands();
            stopHardware();
            return;
        }

        long pos = _directionSign * getPositionCounts();
        long remaining = _targetCounts - pos;
        long remainingAbs = remaining < 0 ? -remaining : remaining;
        long sampleDelta = pos - _lastSamplePos;

        if (remainingAbs <= FINAL_TOLERANCE_COUNTS) {
            stopHardware();
            _state = State::DONE;
            return;
        }

        if (remainingAbs <= brakeZone) {
            setBrake();
            ledcWrite(_ledcA, MAX_PWM);
            ledcWrite(_ledcB, MAX_PWM);
            vTaskDelay(pdMS_TO_TICKS(BRAKE_PULSE_MS));
            _lastSamplePos = pos;
            continue;
        }

        if (remainingAbs < _lastProgress) {
            _lastProgress = remainingAbs;
            _lastProgressMs = millis();
        } else if (millis() - _lastProgressMs > STALL_TIMEOUT_MS) {
            stopHardware();
            _state = State::STALLED;
            return;
        }

        if (sampleDelta <= 0 && remaining > slowZone) {
            _adaptiveBoost = min(_adaptiveBoost + LOAD_BOOST_STEP, LOAD_BOOST_MAX);
        } else if (remaining > slowZone && _adaptiveBoost > 0) {
            _adaptiveBoost = max(_adaptiveBoost - LOAD_BOOST_DECAY, 0);
        } else if (remaining <= slowZone) {
            _adaptiveBoost = 0;
        }

        bool nextDriveReverse = remaining < 0 ? !command.reverse : command.reverse;
        if (nextDriveReverse != driveReverse) {
            driveReverse = nextDriveReverse;
            setDirection(driveReverse);
        }

        int pwm = FINAL_PWM;
        if (remainingAbs > slowZone) {
            pwm = constrain(_basePWM + _adaptiveBoost, MIN_PWM, MAX_PWM);
        } else if (remainingAbs > finalApproachZone) {
            pwm = map(remainingAbs, finalApproachZone, slowZone, FINAL_PWM, precisionCruisePWM);
            pwm = constrain(pwm, FINAL_PWM, precisionCruisePWM);
        } else {
            pwm = FINAL_PWM;
        }

        ledcWrite(_ledcA, pwm);
        ledcWrite(_ledcB, pwm);

        _lastSamplePos = pos;

        vTaskDelay(pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

void MotorPair::stopHardware() {
    ledcWrite(_ledcA, 0);
    ledcWrite(_ledcB, 0);
    digitalWrite(_stby, LOW);
    _state = State::IDLE;
}

void MotorPair::flushPendingCommands() {
    if (!_commandQueue) return;

    MotionCommand discarded{};
    while (xQueueReceive(_commandQueue, &discarded, 0) == pdTRUE) {
    }
}
