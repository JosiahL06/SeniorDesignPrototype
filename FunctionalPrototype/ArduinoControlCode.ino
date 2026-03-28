// =============================
// Control Code
// Last Updated: 3/28
// TODO:
//       - 
// =============================
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>

// =============================
// BLE Configuration
// =============================
#define DEVICE_NAME         "NanoESP32-Willow-BLE"
#define SERVICE_UUID        "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CHARACTERISTIC_UUID "a6a06cf5-71b2-489b-9f03-84dfe6fc6330"

// =============================
// GPIO Pin Configuration
// NOTE: different from pin numbers printed on board; consult arduino datasheet
// =============================
#define LED_PIN 2

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// =============================
// Server Callbacks
// =============================
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE client connected");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("BLE client disconnected");
      pServer->startAdvertising();  // Restart advertising
    }
};

// =============================
// Command Callbacks
// =============================
class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.empty()) return;

      Serial.print("Received command: ");
      Serial.println(value.c_str());

      // =============================
      // Commands
      // =============================

      // ON
      if (value == "ON") {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Pin ON");
      }
      // OFF
      else if (value == "OFF") {
        digitalWrite(LED_PIN, LOW);
        Serial.println("Pin OFF");
      }
      // TOGGLE
      else if (value == "TOGGLE") {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        Serial.println("Pin TOGGLED");
      }
      else {
        Serial.println("Unknown command");
      }
    }
};


void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Starting BLE device...");

  // Initialize BLE
  BLEDevice::init("NanoESP32-Willow-BLE");

  // Enable BLE Security
  //BLESecurity *pSecurity = new BLESecurity();
  //pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  //pSecurity->setCapability(ESP_IO_CAP_NONE);
  //pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  // Require encryption
  //pCharacteristic->setAccessPermissions(
  //  ESP_GATT_PERM_READ_ENCRYPTED |
  //  ESP_GATT_PERM_WRITE_ENCRYPTED
  //);

  pCharacteristic->setCallbacks(new CommandCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  // Initial value
  pCharacteristic->setValue("Willow says meow (hi)");

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("Willow is advertising");
}

void loop() {
}
