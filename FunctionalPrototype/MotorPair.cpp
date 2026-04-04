#include "MotorPair.h"

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
    pinMode(_ain1, OUTPUT);
    pinMode(_ain2, OUTPUT);
    pinMode(_bin1, OUTPUT);
    pinMode(_bin2, OUTPUT);
    pinMode(_stby, OUTPUT);
    digitalWrite(_stby, LOW);
}

void MotorPair::start(uint16_t degrees, uint8_t speedPercent, bool reverse) {
    if (_state == State::RUNNING) return;

    // Store direction once
    _directionSign = reverse ? -1 : 1;

    // Compute target
    _targetCounts = (long)degrees * _cpr / 360;

    // PWM scale
    _basePWM = map(speedPercent, 1, 100, MIN_PWM, MAX_PWM);
    _basePWM = constrain(_basePWM, MIN_PWM, MAX_PWM);

    // Reset encoder atomically
    noInterrupts();
    if (_encoderCount) *_encoderCount = 0;
    interrupts();

    _lastProgress = 0;
    _lastProgressMs = millis();

    setDirection(reverse);
    digitalWrite(_stby, HIGH);

    _state = State::RUNNING;
}

void MotorPair::update() {
    if (_state != State::RUNNING) return;

    // Direction-aware position
    long pos = _directionSign * getPositionCounts();
    long remaining = _targetCounts - pos;

    // Target reached
    if (remaining <= 0) {
        stop();
        _state = State::DONE;
        return;
    }

    // Stall detection
    if (pos - _lastProgress >= STALL_COUNTS) {
        _lastProgress = pos;
        _lastProgressMs = millis();
    } else if (millis() - _lastProgressMs > STALL_TIMEOUT_MS) {
        stop();
        _state = State::STALLED;
        return;
    }

    // Deceleration
    int pwm = _basePWM;
    long slowZone = _cpr / SLOW_ZONE_DIV;
    if (remaining < slowZone) {
        pwm = map(remaining, 0, slowZone, MIN_PWM, _basePWM);
        pwm = max(pwm, MIN_PWM);
    }

    ledcWrite(_ledcA, pwm);
    ledcWrite(_ledcB, pwm);
}

void MotorPair::stop() {
    ledcWrite(_ledcA, 0);
    ledcWrite(_ledcB, 0);
    digitalWrite(_stby, LOW);
    _state = State::IDLE;
}

bool MotorPair::isRunning() const { return _state == State::RUNNING; }
bool MotorPair::isDone() const    { return _state == State::DONE; }
bool MotorPair::isStalled() const { return _state == State::STALLED; }

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
    noInterrupts();
    long pos = *_encoderCount;
    interrupts();
    return pos;
}