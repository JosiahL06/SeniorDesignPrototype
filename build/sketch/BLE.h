#line 1 "C:\\Users\\Josia\\OneDrive\\Documents\\SeniorDesignPrototype\\FunctionalPrototype\\BLE.h"
#pragma once

#include <NimBLEDevice.h>
#include "Packets.h"

// =============================
// BLE Public API
// =============================

// Initialize BLE stack and start advertising
void BLE_init();

// Must be called from loop()
void BLE_update();

// Connection state
bool BLE_isConnected();
bool BLE_isBtTestRunning();

// Latest received command
bool BLE_hasNewCommand();
CommandPacket BLE_getCommand();

// Metrics notification
void BLE_notifyMetrics(const MetricsPacket& metrics);

// Compact motor position notifications
void BLE_notifyMotorPosition1(int32_t positionCounts);
void BLE_notifyMotorPosition2(int32_t positionCounts);

// Echo data packet back to the client for RTT/PER measurement
void BLE_notifyDataEcho(const DataPacket& packet);

// ACK notification
void BLE_notifyAck(const AckPacket& ack);