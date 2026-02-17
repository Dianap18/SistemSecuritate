# IoT Access Control System (ESP32)

## Overview
A security system that replaces physical keys with PIN and biometric authentication. It uses an ESP32 client to send data to a Node.js server for database validation.

## Tech Stack
* **Hardware:** ESP32, Keypad 4x4, Fingerprint Sensor, OLED Display.
* **Backend:** Node.js, Express, SQLite.
* **Communication:** HTTP / JSON over WiFi.

## How it Works
1. **Input:** User enters a PIN or scans a fingerprint.
2. **Validation:** ESP32 sends the data to the Node.js server.
3. **Database:** The server checks credentials in SQLite and returns a `true/false` response.
4. **Action:** ESP32 grants or denies access based on the server's response (OLED message & LEDs).

## Quick Start
1. Run `node server.js` in the server folder.
2. Upload the Arduino sketch to the ESP32.
3. Ensure both are on the same WiFi network.
