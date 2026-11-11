#!/usr/bin/env python3
"""Verify BLE UUID consistency between ESP32 firmware and Android client."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent


class TestBleProfileConsistency(unittest.TestCase):
    def test_uuids_and_device_name_match(self) -> None:
        fw = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        kt = (
            ROOT / "android-app" / "app" / "src" / "main" / "java" / "com" / "example" / "esp32dht11" / "MainActivity.kt"
        ).read_text(encoding="utf-8")

        for uuid in (
            "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
            "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
            "6e400003-b5a3-f393-e0a9-e50e24dcca9e",
        ):
            self.assertIn(uuid, fw)
            self.assertIn(uuid, kt)

        self.assertIn('"ESP32-DHT11"', fw)
        self.assertIn('"ESP32-DHT11"', kt)

    def test_android_gatt_queue_serialized(self) -> None:
        kt = (
            ROOT / "android-app" / "app" / "src" / "main" / "java" / "com" / "example" / "esp32dht11" / "MainActivity.kt"
        ).read_text(encoding="utf-8")
        self.assertIn("GattOp.ReadCharacteristic", kt)
        self.assertIn("override fun onCleared()", kt)
        self.assertIsNone(re.search(r"[\u4e00-\u9fff]", kt))


if __name__ == "__main__":
    unittest.main()
