## Web BLE Control Panel

Open the control panel at:

https://josiahl06.github.io/SeniorDesignPrototype/

The control panel runs entirely in the web browser using the Web Bluetooth API—no software installation required.

---

## Requirements:
- Bluetooth-capable device (PC, laptop, or phone)
- Browser with Web Bluetooth (Chrome/Edge/any chromium browser should work, or a Web Bluetooth browser extension)
- Arduino powered on and nearby (approx. 5-15 meters)
- Password (sent in teams chat)

---

## Device Configuration Assumptions
This control panel expects the Arduino Control Code
- Device Name: NanoESP32-Willow-BLE
- Service UUID: 55f8a5ee-886f-4929-a3ab-5745cbbceab5

---

## Supported Commands

The control panel buttons send plain-text commands to the arduino over BLE:

| Command | Action |
|------|------|
| `*command*` | *command description* |
