#include <Arduino.h>
#include <NimBLEDevice.h>
#include <stdio.h>
#include <string.h>

#include "dht11_math.h"

#ifndef DHT11_PIN
#define DHT11_PIN 4
#endif

// GPIO where the DHT11 data pin is connected. Override via build_flags in platformio.ini.
constexpr uint8_t kDhtPin = static_cast<uint8_t>(DHT11_PIN);

constexpr char kDeviceName[] = "ESP32-DHT11";
constexpr char kServiceUuid[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char kTempCharUuid[] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char kHumCharUuid[] = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char kJsonCharUuid[] = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";

NimBLEServer *gServer = nullptr;
NimBLECharacteristic *gTempChar = nullptr;
NimBLECharacteristic *gHumChar = nullptr;
NimBLECharacteristic *gJsonChar = nullptr;

SampleSmoother gSmoother;
portMUX_TYPE gDhtMux = portMUX_INITIALIZER_UNLOCKED;

uint32_t gReadsOk = 0;
uint32_t gReadsErr = 0;
int gLastRawTemp = 0;
int gLastRawHum = 0;
char gSerialLine[64];
size_t gSerialPos = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *) override {
    Serial.println("[BLE] Client connected");
    // Keep advertising to allow multiple concurrent centrals (NimBLE default max is 3).
    NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer *) override {
    Serial.println("[BLE] Client disconnected");
    NimBLEDevice::startAdvertising();
  }
};

ServerCallbacks gServerCallbacks;

// Expect a pulse of a given level and return the number of loops observed.
// Returns 0 on timeout.
static uint32_t expectPulse(bool level) {
  constexpr uint32_t kMaxCycles = 12000;  // ~120us safety ceiling on 240MHz ESP32.
  uint32_t count = 0;
  while (digitalRead(kDhtPin) == level) {
    if (++count >= kMaxCycles) {
      return 0;
    }
  }
  return count;
}

// Read temperature (C) and humidity (%) from DHT11. Returns true on success.
static bool readDht11(int &temperatureC, int &humidity) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  // Send host start signal outside the critical section so BLE/FreeRTOS ticks run normally.
  pinMode(kDhtPin, OUTPUT);
  digitalWrite(kDhtPin, HIGH);
  delay(20);
  digitalWrite(kDhtPin, LOW);
  delay(20);  // Datasheet: >= 18ms LOW pulse.
  digitalWrite(kDhtPin, HIGH);
  delayMicroseconds(35);
  pinMode(kDhtPin, INPUT_PULLUP);

  // Guard the ~4ms 40-bit microsecond pulse window from BLE/RTOS ISR jitter.
  uint32_t lowCycles[40];
  uint32_t highCycles[40];
  bool pulseOk = true;

  portENTER_CRITICAL(&gDhtMux);
  if (!expectPulse(LOW) || !expectPulse(HIGH)) {
    pulseOk = false;
  } else {
    for (int i = 0; i < 40; ++i) {
      lowCycles[i] = expectPulse(LOW);
      highCycles[i] = expectPulse(HIGH);
      if (lowCycles[i] == 0 || highCycles[i] == 0) {
        pulseOk = false;
        break;
      }
    }
  }
  portEXIT_CRITICAL(&gDhtMux);

  if (!pulseOk) {
    return false;
  }

  for (int i = 0; i < 40; ++i) {
    data[i / 8] <<= 1;
    if (highCycles[i] > lowCycles[i]) {
      data[i / 8] |= 1U;
    }
  }

  return decodeDht11Frame(data, temperatureC, humidity);
}

static bool readDht11WithRetry(int &temperatureC, int &humidity) {
  constexpr int kAttempts = 3;
  for (int i = 0; i < kAttempts; ++i) {
    if (readDht11(temperatureC, humidity)) {
      return true;
    }
    delay(60);
  }
  return false;
}

static void publishReading(int rawTempC, int rawHumPct) {
  gLastRawTemp = rawTempC;
  gLastRawHum = rawHumPct;
  gSmoother.add(rawTempC, rawHumPct);

  const int smoothTemp = gSmoother.avgTemp();
  const int smoothHum = gSmoother.avgHum();
  const float heatIdxC = computeHeatIndexC(static_cast<float>(smoothTemp), static_cast<float>(smoothHum));
  const float dewPointC = computeDewPointC(static_cast<float>(smoothTemp), static_cast<float>(smoothHum));

  char tempBuf[8];
  char humBuf[8];
  char jsonBuf[160];
  snprintf(tempBuf, sizeof(tempBuf), "%d", smoothTemp);
  snprintf(humBuf, sizeof(humBuf), "%d", smoothHum);
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"temp_c\":%d,\"hum_pct\":%d,\"heat_index_c\":%.1f,\"dew_point_c\":%.1f,\"reads_ok\":%lu,\"reads_err\":%lu}",
           smoothTemp, smoothHum, heatIdxC, dewPointC,
           static_cast<unsigned long>(gReadsOk),
           static_cast<unsigned long>(gReadsErr));

  gTempChar->setValue(reinterpret_cast<uint8_t *>(tempBuf), strlen(tempBuf));
  gHumChar->setValue(reinterpret_cast<uint8_t *>(humBuf), strlen(humBuf));
  gJsonChar->setValue(reinterpret_cast<uint8_t *>(jsonBuf), strlen(jsonBuf));
  gTempChar->notify();
  gHumChar->notify();
  gJsonChar->notify();

  Serial.printf("[DHT11] Temp raw/smooth: %d/%d C | Hum raw/smooth: %d/%d %% | HI: %.1f C | Dew: %.1f C\n",
                rawTempC, smoothTemp, rawHumPct, smoothHum, heatIdxC, dewPointC);
}

static void handleSerialCommand(const char *cmd) {
  if (strcasecmp(cmd, "STATUS") == 0) {
    const unsigned peers = gServer ? gServer->getConnectedCount() : 0U;
    Serial.printf("STATUS pin=%u ble_peers=%u ok=%lu err=%lu temp_smooth=%d hum_smooth=%d heap=%lu uptime_ms=%lu\n",
                  static_cast<unsigned>(kDhtPin),
                  peers,
                  static_cast<unsigned long>(gReadsOk),
                  static_cast<unsigned long>(gReadsErr),
                  gSmoother.avgTemp(),
                  gSmoother.avgHum(),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(millis()));
  } else if (strcasecmp(cmd, "READ") == 0) {
    int t = 0, h = 0;
    if (readDht11WithRetry(t, h)) {
      ++gReadsOk;
      publishReading(t, h);
    } else {
      ++gReadsErr;
      Serial.println("ERR DHT11 immediate read failed");
    }
  } else if (strcasecmp(cmd, "RESET") == 0) {
    gReadsOk = 0;
    gReadsErr = 0;
    gSmoother.reset();
    Serial.println("OK stats and smoother reset");
  } else if (strcasecmp(cmd, "HELP") == 0) {
    Serial.println("Commands: STATUS | READ | RESET | HELP");
  } else if (cmd[0] != '\0') {
    Serial.printf("ERR unknown command \"%s\" (type HELP)\n", cmd);
  }
}

static void pollSerial() {
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r' || ch == '\n') {
      if (gSerialPos > 0) {
        gSerialLine[gSerialPos] = '\0';
        handleSerialCommand(gSerialLine);
        gSerialPos = 0;
      }
    } else if (gSerialPos + 1 < sizeof(gSerialLine)) {
      gSerialLine[gSerialPos++] = static_cast<char>(ch);
    }
  }
}

static void setupBle() {
  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_N3);  // Moderate TX power to reduce brownout risk.
  gServer = NimBLEDevice::createServer();
  gServer->setCallbacks(&gServerCallbacks);
  NimBLEService *service = gServer->createService(kServiceUuid);

  gTempChar = service->createCharacteristic(
      kTempCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  gHumChar = service->createCharacteristic(
      kHumCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  gJsonChar = service->createCharacteristic(
      kJsonCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  advertising->start();
}

void setup() {
  Serial.begin(115200);
  pinMode(kDhtPin, INPUT_PULLUP);
  setupBle();
  Serial.println("[BOOT] ESP32 DHT11 BLE publisher started (commands: STATUS, READ, RESET, HELP)");
}

void loop() {
  pollSerial();

  static unsigned long lastReadMs = 0;
  const unsigned long now = millis();
  if (now - lastReadMs < 2000) {
    delay(20);
    return;
  }
  lastReadMs = now;

  int temperatureC = 0;
  int humidity = 0;
  if (!readDht11WithRetry(temperatureC, humidity)) {
    ++gReadsErr;
    Serial.printf("[WARN] DHT11 read failed after retries (ok=%lu err=%lu)\n",
                  static_cast<unsigned long>(gReadsOk),
                  static_cast<unsigned long>(gReadsErr));
    return;
  }

  ++gReadsOk;
  publishReading(temperatureC, humidity);
}
