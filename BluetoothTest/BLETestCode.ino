// =============================
// BLE Testing Code
// Last Updated: 3/30 by Josiah Laakkonen
// TODO:
//  - Redesign metrics (99% chance they are irrelevant/garbage data atm)
//  - Reformat commands as binary
// =============================
#include <NimBLEDevice.h>

// =============================
// BLE Configuration
// =============================
#define DEVICE_NAME "NanoESP32-Willow-BLE"

#define SERVICE_UUID "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CMD_CHAR_UUID "30bce33a-6e60-4306-ad36-8718f82ee801"
#define DATA_CHAR_UUID "fef211ef-bc20-4f2f-9e9d-3cb6c7b6f772"
#define METRICS_CHAR_UUID "555ff5a9-d76d-4945-b7a2-f26612fc5be5"

// =============================
// BLE Objects
// =============================
NimBLECharacteristic* cmdChar;
NimBLECharacteristic* dataChar;
NimBLECharacteristic* metricsChar;

// =============================
// Connection State
// =============================
bool deviceConnected = false;
volatile bool bluetoothTestRunning = false;

// =============================
// Data Packet Format to receive data
// =============================
struct __attribute__((packed)) DataPacket {
  uint32_t seq;
  uint32_t timestampUs;
  uint8_t payload[8];  // adjustable
};

// =============================
// Metrics Packet to output data
// Metrics Recorded:
//  - Timestamp in miliseconds
//  - Packet Error Rate * 10000 (e.g., 1.23% PER becomes 123)
//  - Average Latency in microseconds
//  - Average Jitter in microseconds
//  - Throughput in kilobites per second * 100 (e.g., 312.05 kbps becomes 31205)
//  - Received Signal Strength Indicator
// =============================
struct __attribute__((packed)) MetricsPacket {
  uint32_t timestampMs;          // ms/miliseconds
  uint16_t per_x10000;           // Packet Error Rate * 10,000
  uint32_t avgLatencyUs;         // µs/microseconds
  uint32_t avgJitterUs;          // µs/microseconds
  uint32_t throughputKbps_x100;  // kilobits/second * 100
};

// =============================
// Metrics State
// =============================
uint32_t rxExpectedSeq = 0;
uint32_t packetsRx = 0;
uint32_t packetsLost = 0;

uint64_t latencySumUs = 0;
uint32_t latencySamples = 0;

uint32_t lastRxTimeUs = 0;
uint64_t jitterSumUs = 0;

uint32_t bytesRx = 0;
uint32_t statsStartMs = 0;

// =============================
// Server Callbacks
// =============================
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.printf("Client Connected: \n%s", connInfo.toString().c_str());
    server->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("Client Disconnected - Advertising Restarted");
    NimBLEDevice::startAdvertising();
  }
} serverCallbacks;

// =============================
// Characteristic Callbacks
// =============================

// Command Callbacks
class CmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    std::string value = c->getValue();

    if (value.length() == 0) {
      Serial.println("Received empty CMD string");
      return;
    }

    /* UNFINISHED - Code for when commands are formatted in binary
    switch (value) {
      case STOP_MOTOR:
        break;
      case START_MOTOR:
        break;
      case STOP_BT:
        break;
      case START_BT:
        break;
      default
    }
    */

    if (value == "START_MOTOR") {
      pinMode(D2, OUTPUT);
      digitalWrite(D2, HIGH);
      delay(2000);
      digitalWrite(D2, LOW);
    }
    else if (value == "STOP_MOTOR") {
      digitalWrite(D2, LOW);
    }
    else if (value == "START_BT") {
      bluetoothTestRunning = true;

      rxExpectedSeq = 0;
      packetsRx = 0;
      packetsLost = 0;
      latencySumUs = 0;
      latencySamples = 0;
      jitterSumUs = 0;
      bytesRx = 0;
      lastRxTimeUs = 0;
      statsStartMs = millis();

      Serial.println("START_BT command accepted");
    } else if (value == "STOP_BT") {
      bluetoothTestRunning = false;
      Serial.println("STOP_BT command accepted");
    } else {
      Serial.print("Unknown CMD string: ");
      Serial.println(value);
    }
  }
};

// Data Callbacks
class DataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* d, NimBLEConnInfo& connInfo) override {
    if (!bluetoothTestRunning) return;
    if (d->getValue().size() != sizeof(DataPacket)) return;

    DataPacket pkt;
    memcpy(&pkt, d->getValue().data(), sizeof(pkt));

    uint32_t nowUs = micros();
    packetsRx++;

    if (pkt.seq != rxExpectedSeq) {
      packetsLost += pkt.seq - rxExpectedSeq;
    }
    rxExpectedSeq = pkt.seq + 1;

    latencySumUs += (nowUs - pkt.timestampUs);
    latencySamples++;

    if (lastRxTimeUs > 0) {
      jitterSumUs += abs((int32_t)(nowUs - lastRxTimeUs) - 10000);
    }
    lastRxTimeUs = nowUs;

    bytesRx += sizeof(pkt);
  }
};

// =============================
// Setup - Initialize BLE Connection
// =============================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting NimBLE device...");

  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(247);

  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);

  NimBLEService* service = server->createService(SERVICE_UUID);

  cmdChar = service->createCharacteristic(
    CMD_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::WRITE_NR);

  cmdChar->setValue("Initial");

  dataChar = service->createCharacteristic(

    DATA_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE_NR);

  dataChar->setValue("Initial");

  metricsChar = service->createCharacteristic(
    METRICS_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY);

  metricsChar->setValue("Initial");

  cmdChar->setCallbacks(new CmdCallbacks());
  dataChar->setCallbacks(new DataCallbacks());

  service->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(DEVICE_NAME);
  adv->addServiceUUID(SERVICE_UUID);
  adv->setAppearance(0x0000);
  adv->enableScanResponse(true);
  adv->start();

  statsStartMs = millis();
  Serial.println("Advertising...");
}

// =============================
// Loop - Calculate Metrics and Send Packets
// =============================
void loop() {
  static uint32_t lastMetricsMs = 0;

  if (deviceConnected && bluetoothTestRunning) {
    if (millis() - lastMetricsMs >= 1000) {
      MetricsPacket metrics;
      uint32_t totalPackets = packetsRx + packetsLost;

      metrics.timestampMs = millis();
      metrics.per_x10000 = totalPackets ? (packetsLost * 10000) / totalPackets : 0;
      metrics.avgLatencyUs = latencySamples ? latencySumUs / latencySamples : 0;
      metrics.avgJitterUs = latencySamples ? jitterSumUs / latencySamples : 0;
      metrics.throughputKbps_x100 = (bytesRx * 8 * 100UL) / 1000;

      metricsChar->setValue(
        (uint8_t*)&metrics,
        sizeof(metrics));
      metricsChar->notify();

      packetsRx = packetsLost = latencySumUs = latencySamples = jitterSumUs = bytesRx = 0;
      lastMetricsMs = millis();
    }
  }

  delay(5);
}
