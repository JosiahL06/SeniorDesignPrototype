// =============================
// Prototype Control Code
// https://github.com/JosiahL06/SeniorDesignPrototype/blob/main/FunctionalPrototype/ArduinoControlCode.ino
// Last Updated: 3/28 by Josiah Laakkonen
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

#include <NimBLEDevice.h>

// =============================
// BLE Configuration
// =============================
#define DEVICE_NAME         "NanoESP32-Willow-BLE"
#define SERVICE_UUID        "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CHARACTERISTIC_UUID "a6a06cf5-71b2-489b-9f03-84dfe6fc6330"
#define PAIRING_CHAR_UUID   "11111111-2222-3333-4444-555555555555"

// =============================
// Arduino Pin Configurations
// =============================
// #define ...

NimBLECharacteristic* pCharacteristic;
NimBLECharacteristic* pPairingChar;
bool deviceConnected = false;

// =============================
// Server Callbacks
// =============================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE client disconnected");
        NimBLEDevice::startAdvertising();
    }
};

// =============================
// Command Callbacks
// =============================
class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        Serial.print("Received command: ");
        Serial.println(value.c_str());

        if (value == "ON") {
            analogWrite(LED_RED, 255);
            analogWrite(LED_GREEN, 0);
            analogWrite(LED_BLUE, 255);
            Serial.println("Pin ON");
        }
        else if (value == "OFF") {
            analogWrite(LED_RED, 0);
            analogWrite(LED_GREEN, 255);
            analogWrite(LED_BLUE, 255);
            Serial.println("Pin OFF");
        }
        else if (value == "TOGGLE") {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            Serial.println("Pin TOGGLED");
        }
        else {
            Serial.println("Unknown command");
        }
    }
};

// =============================
// Setup
// =============================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Starting NimBLE device...");

    // Initialize NimBLE
    NimBLEDevice::init(DEVICE_NAME);

    // Enable security encryption
    NimBLEDevice::setSecurityAuth(
      true,   // bonding
      false,  // MITM (must be false for Web Bluetooth)
      true    // LE Secure Connections
    );
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    // Optional power tuning
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Create server
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Create characteristic
    pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::NOTIFY
    );

    pCharacteristic->setCallbacks(new CommandCallbacks());

    pPairingChar = pService->createCharacteristic(
      PAIRING_CHAR_UUID,
      NIMBLE_PROPERTY::READ
    );

    pCharacteristic->setValue("Willow says meow (hi)");
    pPairingChar->setValue("pair");

    // Start the service
    pService->start();

    // Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();


    analogWrite(LED_RED, 255);
    analogWrite(LED_GREEN, 255);
    analogWrite(LED_BLUE, 0);
    Serial.println("Willow is advertising");
}

void loop() {
    // No loop logic required
}
