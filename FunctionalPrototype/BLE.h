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

// ACK notification
void BLE_notifyAck(const AckPacket& ack);