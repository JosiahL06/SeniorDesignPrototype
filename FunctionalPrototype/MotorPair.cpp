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
    return _directionSign * getPositionCounts();
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

    _lastProgress = 0;
    _lastSamplePos = 0;
    _lastProgressMs = millis();

    setDirection(command.reverse);
    digitalWrite(_stby, HIGH);

    while (true) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            flushPendingCommands();
            stopHardware();
            return;
        }

        long pos = _directionSign * getPositionCounts();
        long remaining = _targetCounts - pos;
        long sampleDelta = pos - _lastSamplePos;

        if (remaining <= 0) {
            stopHardware();
            _state = State::DONE;
            return;
        }

        if (pos - _lastProgress >= STALL_COUNTS) {
            _lastProgress = pos;
            _lastProgressMs = millis();
        } else if (millis() - _lastProgressMs > STALL_TIMEOUT_MS) {
            stopHardware();
            _state = State::STALLED;
            return;
        }

        long slowZone = _cpr / SLOW_ZONE_DIV;

        if (sampleDelta <= 0 && remaining > slowZone) {
            _adaptiveBoost = min(_adaptiveBoost + LOAD_BOOST_STEP, LOAD_BOOST_MAX);
        } else if (_adaptiveBoost > 0) {
            _adaptiveBoost = max(_adaptiveBoost - LOAD_BOOST_DECAY, 0);
        }

        int pwm = constrain(_basePWM + _adaptiveBoost, MIN_PWM, MAX_PWM);
        if (slowZone > 0 && remaining < slowZone) {
            pwm = map(remaining, 0, slowZone, MIN_PWM, _basePWM);
            pwm = max(pwm, MIN_PWM);
            pwm = constrain(pwm + _adaptiveBoost, MIN_PWM, MAX_PWM);
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
