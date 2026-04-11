# Web BLE Control Panel

## Overview

The control panel is a browser-based interface for the functional prototype's BLE controller. It runs entirely in the web browser using the Web Bluetooth API, so no desktop app or local installation is required.

Open the control panel here:

[https://josiahl06.github.io/SeniorDesignPrototype/](https://josiahl06.github.io/SeniorDesignPrototype/)

## Requirements

- Bluetooth-capable device, such as a laptop, desktop, or phone
- Browser with Web Bluetooth support, such as Chrome, Edge, or another Chromium-based browser
- The Arduino / ESP32 device powered on and nearby, typically within about 5 to 15 meters
- Access to the connection password, which is shared in Teams chat

## Device Configuration Assumptions

This website expects the Arduino control code to advertise the following BLE configuration:

- Device name: `NanoESP32-Willow-BLE`
- Service UUID: `55f8a5ee-886f-4929-a3ab-5745cbbceab5`

If either value changes in the firmware, update the website to match.

## Shared BLE UUIDs

The website and Arduino firmware use the same BLE service and characteristic UUIDs.

| Type | Name | UUID |
| --- | --- | --- |
| Service | Main BLE service | `55f8a5ee-886f-4929-a3ab-5745cbbceab5` |
| Characteristic | Command input | `30bce33a-6e60-4306-ad36-8718f82ee801` |
| Characteristic | Data echo | `fef211ef-bc20-4f2f-9e9d-3cb6c7b6f772` |
| Characteristic | Metrics | `555ff5a9-d76d-4945-b7a2-f26612fc5be5` |
| Characteristic | ACK | `a2a1c8b3-4d7e-4f2a-9b1c-8e7d5f3a2b1c` |
| Characteristic | Motor position 1 | `d8cfb4a5-7d1b-4d40-8f7f-f6a7f4f0c4e1` |
| Characteristic | Motor position 2 | `4f5cc7fd-1f5c-4aab-82fb-0d6f3dbe7b2b` |

## How to Use

1. Open the control panel in a supported browser.
2. Make sure the Arduino / ESP32 device is powered on and advertising.
3. Connect to the device using the BLE prompt in the browser.
4. Choose the command you want to send.
5. Watch for position or metrics updates in the UI while the device is connected.

## Notes

- The site depends on Web Bluetooth, so unsupported browsers will not be able to connect.
- If the device does not appear, confirm that the firmware is running and advertising the expected BLE name.
- If commands do not have the expected effect, verify that the website and firmware are using the same command set and BLE service UUID.

