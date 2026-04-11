#pragma once
#include <Arduino.h>

class MotorPair {
public:
    MotorPair(
        int pwma, int ain1, int ain2,
        int pwmb, int bin1, int bin2,
        int stby,
        int ledcA, int ledcB,
        volatile long* encoderCount,
        int countsPerRev
    );

    void begin();

    // Non-blocking control
    void start(uint16_t degrees, uint8_t speedPercent, bool reverse);
    void update();
    void stop();

    bool isRunning() const;
    bool isDone() const;
    bool isStalled() const;
    long getSignedPositionCounts() const;

private:
    enum class State { IDLE, RUNNING, DONE, STALLED };
    State _state = State::IDLE;

    // Pins
    int _pwma, _ain1, _ain2;
    int _pwmb, _bin1, _bin2;
    int _stby;
    int _ledcA, _ledcB;

    // Encoder
    volatile long* _encoderCount;
    int _cpr;
    int _directionSign = 1;

    // Motion parameters
    long _targetCounts = 0;
    int _basePWM = 0;

    // Stall detection
    long _lastProgress = 0;
    unsigned long _lastProgressMs = 0;

    // Tuning constants
    static constexpr int MIN_PWM = 60;
    static constexpr int MAX_PWM = 255;
    static constexpr int STALL_COUNTS = 2;
    static constexpr int STALL_TIMEOUT_MS = 200;
    static constexpr int SLOW_ZONE_DIV = 8;

    void setDirection(bool reverse);
    long getPositionCounts() const;
};