#include <NimBLEDevice.h>

// =============================
// BLE Configuration
// =============================
#define DEVICE_NAME "NanoESP32-Willow-BLE"

#define SERVICE_UUID "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CHARACTERISTIC_UUID "a6a06cf5-71b2-489b-9f03-84dfe6fc6330"
#define PAIRING_CHAR_UUID "11111111-2222-3333-4444-555555555555"

// =============================
// BLE Objects
// =============================
NimBLECharacteristic* pCharacteristic;

// =============================
// Connection State
// =============================
bool deviceConnected = false;
int lastRSSI = 0;

// =============================
// Packet Format
// =============================
struct __attribute__((packed)) BlePacket {
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
  uint32_t timestampMs;

  uint16_t packetErrorRate;

  uint32_t avgLatency;
  uint32_t avgJitter;

  uint32_t throughputKbps_x100;  // kbps * 100
  int8_t rssi;
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
  void onConnect(NimBLEServer*, ble_gap_conn_desc* desc) {
    deviceConnected = true;
    lastRSSI = desc->rssi;
    Serial.println("BLE client connected");
  }

  void onDisconnect(NimBLEServer*) {
    deviceConnected = false;
    Serial.println("BLE client disconnected");
    NimBLEDevice::startAdvertising();
  }
};

// =============================
// Characteristic Callbacks
// =============================
class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string value = c->getValue();
    if (value.empty()) return;

    // =============================
    // Metric Packets (Binary)
    // =============================
    if (value.length() >= sizeof(BlePacket)) {
      BlePacket pkt;
      memcpy(&pkt, value.data(), sizeof(pkt));

      uint32_t nowUs = micros();
      packetsRx++;

      // Packet Error Rate
      if (pkt.seq != rxExpectedSeq) {
        packetsLost += pkt.seq - rxExpectedSeq;
      }
      rxExpectedSeq = pkt.seq + 1;

      // RTT-Based Latency
      uint32_t rttUs = nowUs - pkt.timestampUs;
      latencySumUs += rttUs;
      latencySamples++;

      // Jitter
      if (lastRxTimeUs > 0) {
        uint32_t delta = nowUs - lastRxTimeUs;
        jitterSumUs += abs((int32_t)delta - 10000);  // expected ~10ms
      }
      lastRxTimeUs = nowUs;

      // Throughput
      bytesRx += value.length();

      // Echo back for RTT
      c->setValue(value);
      c->notify();
    }
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

  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(SERVICE_UUID);

  pCharacteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

  pCharacteristic->setCallbacks(new CommandCallbacks());

  service->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setName(DEVICE_NAME);
  adv->setAppearance(0x0000);
  adv->enableScanResponse(true);
  adv->start();

  statsStartMs = millis();
  Serial.println("Advertising...");
}

// =============================
// Loop - Print Metrics
// =============================
void loop() {
  if (!deviceConnected) {
    delay(5000);
    return;
  }

  // Measure metrics once per second
  if (millis() - statsStartMs >= 1000) {
    MetricsPacket metrics;

    uint32_t totalPackets = packetsRx + packetsLost;

    metrics.timestampMs = millis();

    metrics.per_x10000 = (totalPackets > 0)
                           ? (packetsLost * 10000UL) / totalPackets
                           : 0;

    metrics.avgLatencyUs = latencySamples
                             ? latencySumUs / latencySamples
                             : 0;

    metrics.avgJitterUs = latencySamples
                            ? jitterSumUs / latencySamples
                            : 0;

    metrics.throughputKbps_x100 =
      (bytesRx * 8 * 100UL) / 1000;

    metrics.rssi = lastRSSI;

    // Send metrics to central device
    pCharacteristic->setValue(
      (uint8_t*)&metrics,
      sizeof(MetricsPacket));

    pCharacteristic->notify();

    Serial.println("---- BLE Metrics ----");
    Serial.printf("PER: %.3f\n", per);
    Serial.printf("Latency (RTT avg): %.2f ms\n", avgLatencyMs);
    Serial.printf("Jitter (avg): %.2f ms\n", avgJitterMs);
    Serial.printf("Throughput: %.2f kbps\n", kbps);
    Serial.printf("RSSI: %d dBm\n", lastRSSI);
    Serial.println("---------------------\n");

    // Reset window
    bytesRx = 0;
    packetsRx = 0;
    packetsLost = 0;
    latencySumUs = 0;
    latencySamples = 0;
    jitterSumUs = 0;

    statsStartMs = millis();
  }
}
