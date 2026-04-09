#include "BLE.h"
#include <cstring>

// =============================
// BLE UUID Definitions
// =============================
#define DEVICE_NAME        "NanoESP32-Willow-BLE"
#define SERVICE_UUID       "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CMD_CHAR_UUID      "30bce33a-6e60-4306-ad36-8718f82ee801"
#define DATA_CHAR_UUID     "fef211ef-bc20-4f2f-9e9d-3cb6c7b6f772"
#define METRICS_CHAR_UUID  "555ff5a9-d76d-4945-b7a2-f26612fc5be5"
#define ACK_CHAR_UUID      "a2a1c8b3-4d7e-4f2a-9b1c-8e7d5f3a2b1c"

// =============================
// BLE State
// =============================
static NimBLECharacteristic* cmdChar;
static NimBLECharacteristic* dataChar;
static NimBLECharacteristic* metricsChar;
static NimBLECharacteristic* ackChar;

static bool deviceConnected = false;
static bool bluetoothTestRunning = false;

static CommandPacket latestCmd{};
static volatile bool newCommandAvailable = false;

// =============================
// Server Callbacks
// =============================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(
        NimBLEServer* server,
        NimBLEConnInfo& connInfo
    ) override {
        deviceConnected = true;
        server->updateConnParams(
            connInfo.getConnHandle(),
            24, 48, 0, 180
        );
    }

    void onDisconnect(
        NimBLEServer*,
        NimBLEConnInfo&,
        int
    ) override {
        deviceConnected = false;
        NimBLEDevice::startAdvertising();
    }
};

static ServerCallbacks serverCallbacks;

// =============================
// Command Characteristic Callback
// =============================
class CmdCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(
        NimBLECharacteristic* c,
        NimBLEConnInfo&
    ) override {
        const std::string& value = c->getValue();
        if (value.length() != 3) return;

        latestCmd = unpackCommand(value);
        newCommandAvailable = true;

        if (latestCmd.commandId == START_BT)
            bluetoothTestRunning = true;
        else if (latestCmd.commandId == STOP_BT)
            bluetoothTestRunning = false;

        // Send ACK notification
        AckPacket ack{};
        ack.commandId = latestCmd.commandId;
        ack.status = ACK_SUCCESS;
        ack.timestampMs = millis();
        BLE_notifyAck(ack);
    }
};

static CmdCallbacks cmdCallbacks;

// =============================
// Data Characteristic Callback
// =============================
class DataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(
        NimBLECharacteristic* d,
        NimBLEConnInfo&
    ) override {
        const std::string& value = d->getValue();
        if (value.length() != sizeof(DataPacket)) return;

        if (!deviceConnected || !bluetoothTestRunning) return;

        // Echo immediately so packet matching stays accurate
        // even at higher write rates.
        d->setValue(
            (uint8_t*)value.data(),
            sizeof(DataPacket)
        );
        d->notify();
    }
};

static DataCallbacks dataCallbacks;

// =============================
// BLE Public Functions
// =============================

void BLE_init() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setSecurityAuth(false, false, false);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&serverCallbacks);

    NimBLEService* service =
        server->createService(SERVICE_UUID);

    cmdChar = service->createCharacteristic(
        CMD_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::WRITE_NR
    );
    cmdChar->setCallbacks(&cmdCallbacks);

    dataChar = service->createCharacteristic(
        DATA_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE_NR |
        NIMBLE_PROPERTY::NOTIFY
    );
    dataChar->setCallbacks(&dataCallbacks);

    metricsChar = service->createCharacteristic(
        METRICS_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    ackChar = service->createCharacteristic(
        ACK_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    service->start();

    NimBLEAdvertising* adv =
        NimBLEDevice::getAdvertising();

    adv->setName(DEVICE_NAME);
    adv->addServiceUUID(SERVICE_UUID);
    adv->enableScanResponse(true);
    adv->start();
}

void BLE_update() {
    // placeholder for future periodic BLE tasks
}

bool BLE_isConnected() {
    return deviceConnected;
}

bool BLE_isBtTestRunning() {
    return bluetoothTestRunning;
}

bool BLE_hasNewCommand() {
    return newCommandAvailable;
}

CommandPacket BLE_getCommand() {
    newCommandAvailable = false;
    return latestCmd;
}

void BLE_notifyMetrics(const MetricsPacket& metrics) {
    if (!deviceConnected) return;

    metricsChar->setValue(
        (uint8_t*)&metrics,
        sizeof(MetricsPacket)
    );
    metricsChar->notify();
}

void BLE_notifyDataEcho(const DataPacket& packet) {
    if (!deviceConnected) return;

    dataChar->setValue(
        (uint8_t*)&packet,
        sizeof(DataPacket)
    );
    dataChar->notify();
}

void BLE_notifyAck(const AckPacket& ack) {
    if (!deviceConnected) return;

    ackChar->setValue(
        (uint8_t*)&ack,
        sizeof(AckPacket)
    );
    ackChar->notify();
}