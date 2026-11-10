# ESP32 DHT11 BLE Android App

Minimal **Jetpack Compose** Android client that scans for the `ESP32-DHT11` BLE peripheral, subscribes to GATT notifications, and displays live temperature, relative humidity, and computed dew point.

## BLE GATT Profile

- **Device Name**: `ESP32-DHT11`
- **Service UUID**: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- **Characteristics (`READ | NOTIFY`)**:
  - **Temperature**: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
  - **Humidity**: `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

## Build & Run

1. Open `android-app` in **Android Studio** (AGP 8.5+, JDK 17).
2. Connect a physical Android device running **Android 8.0 (API 26)+**.
3. Build and launch the app, then tap **Scan & Connect**.
4. Tap **Disconnect** to stop notifications and release the GATT connection.

## Implementation Notes

- **Android 12+ Privacy**: `BLUETOOTH_SCAN` declares `usesPermissionFlags="neverForLocation"` so location permission is only requested on Android 11 and below.
- **Serialized GATT Queue**: All CCCD descriptor writes and initial characteristic reads are queued and dispatched strictly one operation at a time via `onDescriptorWrite` / `onCharacteristicRead`.
