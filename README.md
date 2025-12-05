# ESP32 DHT11 BLE Bridge

Firmware and Android app for reading a DHT11 sensor with an ESP32 and publishing temperature/humidity over BLE.

## Contents
- `platformio.ini` + `src/`: ESP32 Arduino firmware using NimBLE to expose DHT11 readings.
- `android-app/`: Jetpack Compose client that scans, connects, and displays values.
- `CJDHT11说明书及例程/`: Vendor PDF and STM32 Keil example kept for reference; not required to build this project.

## Hardware setup
- Board: ESP32-DevKit (or compatible).
- Sensor: DHT11, data pin -> GPIO 4 by default (`DHT11_PIN` set via `platformio.ini` build_flags).
- Power: 3.3V and GND to the DHT11. Keep leads short for stability.

## Build and flash (ESP32 firmware)
Prereqs: PlatformIO CLI or VS Code + PlatformIO extension.
1) Connect the ESP32 by USB.
2) From repo root run:
   - Build: `pio run`
   - Flash: `pio run -t upload`
   - Serial monitor (115200 baud): `pio device monitor`

## BLE profile
- Device name: `ESP32-DHT11`
- Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Characteristics:
  - Temperature (read + notify): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
  - Humidity (read + notify): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

## Android app
Open `android-app` in Android Studio (AGP 8.5+, JDK 17). Build/run on Android 8+. The app scans for the service UUID and device name above, connects, and subscribes to notifications. See `android-app/README.md` for details.

## Notes
- The firmware uses a small rolling average to smooth DHT11 noise and retries reads up to three times.
- Reduce TX power or add a larger capacitor if you see brownouts during BLE activity.
