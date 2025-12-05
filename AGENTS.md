# Repository Guidelines

## Project Structure & Module Organization
- `src/`, `platformio.ini`: ESP32 Arduino firmware publishing DHT11 readings over BLE.
- `include/`: Shared headers for the firmware.
- `android-app/`: Jetpack Compose client; open this folder in Android Studio.
- `CJDHT11说明书及例程/`: Vendor PDF and STM32 Keil example; reference only, not part of the build.
- `.pio/`, `android-app/build/`, `.gradle/`: Generated outputs (ignored in git).

## Build, Test, and Development Commands
- Firmware build: `pio run`
- Flash firmware: `pio run -t upload`
- Serial monitor (115200 baud): `pio device monitor`
- Android build: from `android-app/` run `./gradlew assembleDebug`
- Android install to device: `./gradlew installDebug`
- Quick clean: `pio run -t clean` (firmware) or `./gradlew clean` (Android)

## Coding Style & Naming Conventions
- C++ firmware: prefer `constexpr`, `static` where appropriate; avoid dynamic allocation. Use `camelCase` for functions/variables, `kPascalCase` for constants, and `gPrefix` for globals (matches current code).
- Kotlin/Compose: follow standard Kotlin style with `camelCase` members and `UpperCamelCase` composables/screens.
- Indentation: 2 spaces in C++ and Kotlin files; no tabs. Keep line length reasonable for serial logging readability.

## Testing Guidelines
- Firmware: manual verification via `pio device monitor`; no automated tests present. Add small hardware smoke tests under `test/` if needed.
- Android: rely on Android Studio instrumentation/unit tests; none included. If adding, mirror package structure and name tests with `*Test`.

## Commit & Pull Request Guidelines
- Commit messages: short imperative summaries (e.g., “Add project README”, “Update gitignore”).
- PRs should describe the change, note build/flash status, and include screenshots or logs for Android UI/BLE flows when relevant.

## Security & Configuration Tips
- Do not commit secrets or Wi-Fi credentials. Keep `local.properties` (Android) untracked.
- Keep BLE UUIDs aligned between firmware and app; update both when changing characteristics.
