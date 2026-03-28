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
- Device Name:
- Service UUID: 
- Characteristic UUID:
- Characteristic Properties: Write

---

## Supported Commands

The control panel sends plain-text commands over BLE:

| Command | Action |
|------|------|
| `*command*` | *command description* |

Commands are case-sensitive.
