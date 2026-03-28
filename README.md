## Web BLE Control Panel

Open the control panel at:

https://josiahl06.github.io/SeniorDesignPrototype/

The control panel runs entirely in the web browser using the Web Bluetooth API—no software installation required.

---

## Requirements:
- Bluetooth-capable device (PC, laptop, or phone)
- Open link in Chrome or Edge (or any chromium browser should work)
- Arduino powered on and nearby (approx. 5-15 meters)

---

## Device Configuration Assumptions
This control panel expects the ESP32 firmware to advertise the following:
- Device Name: NanoESP32-Willow-BLE
- Service UUID: 55f8a5ee-886f-4929-a3ab-5745cbbceab5
- Characteristic UUID: a6a06cf5-71b2-489b-9f03-84dfe6fc6330
- Characteristic Properties: Read, Write, Notify

---

## Supported Commands

The control panel buttons send plain-text commands to the arduino over BLE:

| Command | Action |
|------|------|
| `*command*` | *command description* |
