// =============================
// Prototype Control Code
// https://github.com/JosiahL06/SeniorDesignPrototype/blob/main/FunctionalPrototype/ArduinoControlCode.ino
// Last Updated: 3/29 by Josiah Laakkonen
// TODO:
//        - Add BLE security encryption
//        - Define commands to allow motor testing
//        - Define commands to allow BLE testing
// =============================
// LED logic during runtime:
//        - LED blinks blue when searching for connection
//        - LED turns green when connected
//        - LED blinks red when a command is running
// =============================

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>
#include <BLE2902.h>

// =============================
// BLE Configuration
// =============================
#define DEVICE_NAME "NanoESP32-Willow-BLE"
#define SERVICE_UUID "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CHARACTERISTIC_UUID "a6a06cf5-71b2-489b-9f03-84dfe6fc6330"

// =============================
// Arduino Pin Configurations
// =============================
#define LED_PIN 2

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// =============================
// Server Callbacks
// =============================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("BLE client disconnected");
    pServer->startAdvertising();  // Restart advertising
  }
};

// =============================
// Command Callbacks
// =============================
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
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
    } else {
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
  BLEDevice::init(DEVICE_NAME);

  // Enable BLE Security (currently not working)
  //BLESecurity *pSecurity = new BLESecurity();
  //pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  //pSecurity->setCapability(ESP_IO_CAP_NONE);
  //pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  // Create BLE Server and set server callbacks
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  //pCharacteristic->setAccessPermissions(
  //  ESP_GATT_PERM_WRITE_ENCRYPTED
  //);

  // Set command callbacks
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
