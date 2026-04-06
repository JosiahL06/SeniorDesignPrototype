#pragma once
#include <Arduino.h>
#include <string>

// =============================
// Command IDs (protocol-level)
// =============================
enum CommandId : uint8_t {
    STOP_MOTOR  = 0b00,
    START_MOTOR = 0b01,
    STOP_BT     = 0b10,
    START_BT    = 0b11
};

// =============================
// Command Packet
// =============================
struct CommandPacket {
    CommandId commandId;
    bool      inSteps;
    bool      reverse;
    uint8_t   degrees;
    uint8_t   speed;
    uint16_t  interval;
};

// =============================
// Data Packet
// =============================
struct __attribute__((packed)) DataPacket {
    uint32_t seq;
    uint32_t timestampUs;
    uint8_t  payload[8];
};

// =============================
// Metrics Packet
// =============================
struct __attribute__((packed)) MetricsPacket {
  // When this metrics snapshot was generated
  // (milliseconds)
  uint32_t timestampMs;

  // Total notifications sent since START_BT
  uint32_t txCount;

  // Total bytes sent as BLE notifications since START_BT
  uint32_t txBytes;

  // Mean actual interval between notifications
  // (microseconds, application-layer measured)
  uint32_t intervalMeanUs;

  // RMS jitter of the notification interval
  // (microseconds, application-layer measured)
  uint32_t intervalJitterUs;

  // Number of times the task missed its expected interval badly
  // (indicates scheduler / BLE backpressure issues)
  uint32_t sendOverruns;

  // Elapsed time since START_BT
  uint32_t uptimeMs;
};

// =============================
// ACK Packet
// =============================
enum AckStatus : uint8_t {
    ACK_SUCCESS = 0x00,
    ACK_ERROR   = 0x01
};

struct __attribute__((packed)) AckPacket {
    CommandId commandId;     // Which command is being acknowledged
    AckStatus status;        // Success or error status
    uint32_t timestampMs;    // Timestamp when ACK was generated
};

// =============================
// Packet utilities
// =============================
CommandPacket unpackCommand(const std::string& value);