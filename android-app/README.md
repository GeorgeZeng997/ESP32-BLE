# ESP32 DHT11 BLE Android App

Minimal Jetpack Compose app that scans for the ESP32 published in this repo, connects over BLE, subscribes to the temperature and humidity characteristics, and shows the live values.

## BLE profile (matches the firmware)
- Device name: `ESP32-DHT11`
- Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Characteristics
  - Temperature (read + notify): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
  - Humidity (read + notify): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

## How to build and run
1. Open the `android-app` folder in Android Studio (AGP 8.5+, JDK 17).
2. Connect a phone (Android 8+; Android 12+ will prompt for BLE permissions).
3. Install/run the app. Tap “扫描并连接” to scan, connect, and start receiving values.

## Notes
- The app filters by service UUID *and* device name to reduce false matches.
- On first use you must grant Bluetooth permissions (and location on Android 11 and below).
- Press “断开连接” to stop notifications and close the GATT session.
