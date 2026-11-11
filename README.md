# ESP32 DHT11 BLE Bridge

ESP32 NimBLE peripheral firmware and companion Jetpack Compose Android app for reading a **DHT11** temperature/humidity sensor and streaming live environmental telemetry over **Bluetooth Low Energy (BLE)**.

## Repository Structure

- `platformio.ini`, `include/dht11_math.h`, `src/main.cpp`: ESP32 Arduino firmware using `NimBLE-Arduino` with FreeRTOS critical-section pulse capture, 4-sample rolling smoothing, Dew Point & Heat Index calculation, and an interactive UART CLI.
- `android-app/`: Android 8.0+ (API 26+) Jetpack Compose client with serialized GATT operation queueing, live notifications, and Dew Point readout.
- `docs/dht11_reference.md`: DHT11 pinout, electrical ratings, and 40-bit single-wire timing reference.
- `tests/`: Host C++17 and Python unit tests for frame validation, smoothing, and BLE UUID consistency.

## Hardware Setup

- **Board**: `ESP32-DevKitC` (or compatible ESP32 board)
- **Sensor**: `DHT11`
  - `DATA` -> `GPIO 4` (configurable via `-DDHT11_PIN=4` in `platformio.ini`)
  - `VCC` -> `3.3 V`
  - `GND` -> `GND`
  - Add a `4.7 kOhm - 10 kOhm` pull-up resistor between `DATA` and `VCC` if using a bare 4-pin sensor rather than a breakout module.

## Build & Flash (ESP32 Firmware)

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

### Serial Console Commands (`115200` baud)
- `STATUS` — Print GPIO pin, connected BLE central count, read success/error counters, smoothed values, free heap, and uptime.
- `READ` — Trigger an immediate DHT11 sample and BLE notification.
- `RESET` — Reset read counters and clear the rolling average window.
- `HELP` — List supported commands.

## BLE GATT Profile

- **Device Name**: `ESP32-DHT11`
- **Service UUID**: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- **Characteristics (`READ | NOTIFY`)**:
  - **Temperature (`C`, UTF-8 integer)**: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
  - **Humidity (`% RH`, UTF-8 integer)**: `6e400003-b5a3-f393-e0a9-e50e24dcca9e`
  - **JSON Telemetry (`temp_c`, `hum_pct`, `heat_index_c`, `dew_point_c`, `reads_ok`, `reads_err`)**: `6e400004-b5a3-f393-e0a9-e50e24dcca9e`

## Android Client

Open `android-app/` in **Android Studio** (AGP 8.5+, JDK 17) or build via CLI:

```bash
cd android-app
./gradlew assembleDebug
```

See [`android-app/README.md`](android-app/README.md) for permissions and usage notes.
