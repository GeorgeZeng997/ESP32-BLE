# Repository Guidelines

## Project Structure & Module Organization
- `src/`, `include/`, `platformio.ini`: ESP32 Arduino firmware publishing DHT11 readings over BLE (`NimBLE-Arduino`).
- `android-app/`: Jetpack Compose Android BLE client; open this folder in Android Studio.
- `docs/dht11_reference.md`: Electrical and single-wire 40-bit timing specification for the DHT11 sensor.
- `tests/`: Host-side C++17 and Python unit tests verifying DHT11 checksum/bounds decoding, smoothing, psychrometric math, and BLE UUID alignment.
- `.pio/`, `android-app/build/`, `.gradle/`: Generated build outputs (ignored in Git).

## Build, Test, and Development Commands
- Firmware build: `pio run`
- Flash firmware: `pio run -t upload`
- Serial monitor (`115200` baud): `pio device monitor`
- Host unit tests:
  - `g++ -std=c++17 -Wall -Wextra -Werror tests/test_dht11_math.cpp -o build_test && ./build_test`
  - `python3 -m unittest discover -s tests -v`
- Android build: from `android-app/` run `./gradlew assembleDebug`
- Android install to device: `./gradlew installDebug`

## Coding Style & Naming Conventions
- C++ firmware: prefer `constexpr` and `static` storage; avoid heap allocation in steady-state loops. Use `camelCase` for functions/variables, `kPascalCase` for constants, and `gPrefix` for module globals.
- Kotlin/Compose: follow standard Kotlin style with `camelCase` members and `UpperCamelCase` composables. Serialize all `BluetoothGatt` descriptor writes and characteristic reads through a single operation queue.
- Indentation: 2 spaces in C++ and Kotlin files; no tabs.

## Security & Configuration Tips
- Never commit secrets, keystores, or Wi-Fi credentials. Keep `local.properties` untracked.
- Keep BLE UUIDs strictly synchronized between `src/main.cpp` and `android-app/.../MainActivity.kt`.
