// =============================
// Prototype Control Code
// OUTDATED AS OF 3/30
// https://github.com/JosiahL06/SeniorDesignPrototype/blob/main/FunctionalPrototype/ArduinoControlCode.ino
// Last Updated: 3/28 by Josiah Laakkonen
// TODO:
//        - Update BLE connection to reflect changes per BLETestCode.ino
//        - Update Motor control to reflect changes per MotorControlCode.ino
// =============================

#include <NimBLEDevice.h>

// =============================
// BLE Configuration
// =============================
#define DEVICE_NAME         "NanoESP32-Willow-BLE"
#define SERVICE_UUID        "55f8a5ee-886f-4929-a3ab-5745cbbceab5"
#define CHARACTERISTIC_UUID "a6a06cf5-71b2-489b-9f03-84dfe6fc6330"

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

        // Restart advertising
        pServer->getAdvertising()->stop();
        pServer->getAdvertising()->start();
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

    pCharacteristic->setValue("Willow says meow (hi)");

    // Start the service
    pService->start();

    // Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setName(DEVICE_NAME);
    pAdvertising->setAppearance(0x0000);
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
