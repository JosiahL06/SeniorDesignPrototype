#line 1 "C:\\Users\\Josia\\OneDrive\\Documents\\SeniorDesignPrototype\\FunctionalPrototype\\MotorPair.h"
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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

    // Queue a motion request for the FreeRTOS control task.
    bool start(uint16_t degrees, uint8_t speedPercent, bool reverse);
    void update();
    void stop();

    bool isRunning() const;
    bool isDone() const;
    bool isStalled() const;
    long getSignedPositionCounts() const;

private:
    enum class State { IDLE, RUNNING, DONE, STALLED };
    volatile State _state = State::IDLE;

    struct MotionCommand {
        uint16_t degrees;
        uint8_t speedPercent;
        bool reverse;
    };

    // Pins
    int _pwma, _ain1, _ain2;
    int _pwmb, _bin1, _bin2;
    int _stby;
    int _ledcA, _ledcB;

    // Encoder
    volatile long* _encoderCount;
    int _cpr;
    volatile int _directionSign = 1;

    // FreeRTOS control
    QueueHandle_t _commandQueue = nullptr;
    TaskHandle_t _taskHandle = nullptr;
    bool _taskStarted = false;

    // Motion parameters
    long _targetCounts = 0;
    int _basePWM = 0;
    int _adaptiveBoost = 0;

    // Stall detection
    long _lastProgress = 0;
    long _lastSamplePos = 0;
    unsigned long _lastProgressMs = 0;

    // Tuning constants
    static constexpr int MIN_PWM = 60;
    static constexpr int MAX_PWM = 255;
    static constexpr int STALL_COUNTS = 20;
    static constexpr int STALL_TIMEOUT_MS = 2000;
    static constexpr int SLOW_ZONE_DIV = 12;
    static constexpr int FINAL_APPROACH_DIV = 48;
    static constexpr int FINAL_TOLERANCE_COUNTS = 2;
    static constexpr int FINAL_PWM = MIN_PWM;
    static constexpr int PRECISION_PWM_CAP = 160;
    static constexpr uint32_t BRAKE_PULSE_MS = 8;
    static constexpr int LOAD_BOOST_STEP = 5;
    static constexpr int LOAD_BOOST_DECAY = 1;
    static constexpr int LOAD_BOOST_MAX = MAX_PWM;
    static constexpr uint32_t CONTROL_PERIOD_MS = 5;
    static constexpr UBaseType_t TASK_PRIORITY = 5;
    static constexpr uint32_t COMMAND_QUEUE_LENGTH = 6;

    void setDirection(bool reverse);
    void setBrake();
    long getPositionCounts() const;
    void runTask();
    void executeCommand(const MotionCommand& command);
    void stopHardware();
    void flushPendingCommands();

    static void taskEntry(void* parameter);
};