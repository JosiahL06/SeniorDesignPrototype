// =============================
// BLE Testing Code
// Last Updated: 4/1 by Josiah Laakkonen
// TODO:
//  - Redesign how metrics are measured/calculated (99% chance they are irrelevant/garbage data atm)
//  - Add ACK (acknowledgement) characteristic to confirm when other characteristics are received
//    (This will replace Serial as main debug communication)
//  - (Optional?) Reformat code to move BLE and motor control into different .cpp and .h files
// =============================
#include <NimBLEDevice.h>

// =============================
// Pin Configuration
// =============================
const int PWMA = 8;
const int AIN2 = 7;
const int AIN1 = 6;
const int STBY = 5;
const int BIN1 = 4;
const int BIN2 = 3;
const int PWMB = 2;

const int ledcChannelA = 0;
const int ledcChannelB = 1;
const int freq = 5000;
const int resolution = 8;

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
// Command IDs
// =============================
enum CommandId : uint8_t {
  STOP_MOTOR = 0b00,
  START_MOTOR = 0b01,
  STOP_BT = 0b10,
  START_BT = 0b11
};

// =============================
// Command Packet Format to receive data
// =============================
struct CommandPacket {
  uint8_t commandId;
  bool inSteps;
  bool reverse;       // valid for motor only
  uint8_t degrees;    // valid for motor only
  uint8_t speed;      // valid for motor only
  uint16_t interval;  // valid for bluetooth only
};

CommandPacket cmd;

// =============================
// Function to unpack command info
// =============================
CommandPacket unpackCommand(const std::string& value) {
  CommandPacket cmd;

  const uint8_t* data =
    reinterpret_cast<const uint8_t*>(value.data());

  uint32_t packet =
    ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2]);

  cmd.commandId = (packet >> 18) & 0b11;
  cmd.inSteps = (packet >> 17) & 0b1;
  cmd.reverse = (packet >> 16) & 0b1;
  cmd.interval = packet & 0xFFFF;
  cmd.degrees = (cmd.interval >> 8) & 0xFF;
  cmd.speed = cmd.interval & 0xFF;

  return cmd;
}

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
    const std::string& value = c->getValue();

    if (value.length() != 3) {
      Serial.println("Invalid CMD string");
      return;
    }

    cmd = unpackCommand(value);

    switch (cmd.commandId) {

      case STOP_MOTOR:
        {
          Serial.println("Command Received: STOP_MOTOR");

          ledcWrite(ledcChannelA, 0);
          ledcWrite(ledcChannelB, 0);
          digitalWrite(STBY, LOW);
          break;
        }
      case START_MOTOR:
        {
          Serial.println("Command Received: START_MOTOR");
          Serial.printf(
            "  Degrees: %u | Speed: %u | InSteps: %d | Reverse: %d\n",
            cmd.degrees, cmd.speed, cmd.inSteps, cmd.reverse);

          int runTimeMs = 1000 * cmd.degrees / 360 * 100 / cmd.speed;
          int pwm = 255 * cmd.speed / 100;

          // Direction control
          if (cmd.reverse) {
            digitalWrite(AIN1, LOW);
            digitalWrite(AIN2, HIGH);
            digitalWrite(BIN1, HIGH);
            digitalWrite(BIN2, LOW);
          } else {
            digitalWrite(AIN1, HIGH);
            digitalWrite(AIN2, LOW);
            digitalWrite(BIN1, LOW);
            digitalWrite(BIN2, HIGH);
          }

          // Set speed and start motors
          ledcWrite(ledcChannelA, pwm);
          ledcWrite(ledcChannelB, pwm);
          digitalWrite(STBY, HIGH);

          // Allow motors to run
          delay(runTimeMs);

          // Stop motors
          ledcWrite(ledcChannelA, 0);
          ledcWrite(ledcChannelB, 0);
          digitalWrite(STBY, LOW);
          break;
        }
      case START_BT:
        {
          Serial.println("Command Received: START_BT");
          Serial.printf(
            "  Interval: %u ms | InSteps: %d\n",
            cmd.interval, cmd.inSteps);

          bluetoothTestRunning = true;

          // Reset stats
          rxExpectedSeq = 0;
          packetsRx = 0;
          packetsLost = 0;
          latencySumUs = 0;
          latencySamples = 0;
          jitterSumUs = 0;
          bytesRx = 0;
          lastRxTimeUs = 0;
          statsStartMs = millis();
          break;
        }
      case STOP_BT:
        {
          Serial.println("Command Received: STOP_BT");
          bluetoothTestRunning = false;
          break;
        }
      default:
        {
          Serial.println("Unknown CMD ID");
          break;
        }
    }
  }
} cmdCallbacks;

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
} dataCallbacks;

// =============================
// Setup - Initialize BLE Connection
// =============================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting NimBLE device...");

  // Configuring motor pins
  for (int i = 2; i <= 8; i++) {
    pinMode(i, OUTPUT);
  }

  ledcSetup(ledcChannelA, freq, resolution);
  ledcSetup(ledcChannelB, freq, resolution);
  ledcAttachPin(PWMA, ledcChannelA);
  ledcAttachPin(PWMB, ledcChannelB);

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
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

  cmdChar->setValue("Initial");

  dataChar = service->createCharacteristic(

    DATA_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE_NR);

  dataChar->setValue("Initial");

  metricsChar = service->createCharacteristic(
    METRICS_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY);

  metricsChar->setValue("Initial");

  cmdChar->setCallbacks(&cmdCallbacks);
  dataChar->setCallbacks(&dataCallbacks);

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
    if (millis() - lastMetricsMs >= cmd.interval) {
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
