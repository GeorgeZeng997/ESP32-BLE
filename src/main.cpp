#include <Arduino.h>
#include <NimBLEDevice.h>

// GPIO where the DHT11 data pin is connected. Override via build_flags in platformio.ini.
constexpr uint8_t kDhtPin = static_cast<uint8_t>(DHT11_PIN);

constexpr char kDeviceName[] = "ESP32-DHT11";
constexpr char kServiceUuid[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char kTempCharUuid[] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char kHumCharUuid[] = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

NimBLECharacteristic *gTempChar = nullptr;
NimBLECharacteristic *gHumChar = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *) override {
    Serial.println("BLE client connected");
    // Keep advertising to allow multiple concurrent centrals (NimBLE default max is 3).
    NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer *) override {
    Serial.println("BLE client disconnected");
    NimBLEDevice::startAdvertising();
  }
};

// Simple rolling average to smooth the sometimes noisy DHT11 values.
struct SampleSmoother {
  static constexpr size_t kWindow = 4;
  int temps[kWindow] = {0};
  int hums[kWindow] = {0};
  size_t count = 0;
  size_t idx = 0;

  void add(int t, int h) {
    temps[idx] = t;
    hums[idx] = h;
    idx = (idx + 1) % kWindow;
    if (count < kWindow) {
      ++count;
    }
  }

  int avgTemp() const {
    if (count == 0) return 0;
    int sum = 0;
    for (size_t i = 0; i < count; ++i) sum += temps[i];
    return (sum + static_cast<int>(count / 2)) / static_cast<int>(count);
  }

  int avgHum() const {
    if (count == 0) return 0;
    int sum = 0;
    for (size_t i = 0; i < count; ++i) sum += hums[i];
    return (sum + static_cast<int>(count / 2)) / static_cast<int>(count);
  }
} gSmoother;

// Expect a pulse of a given level and return the number of loops observed.
// Returns 0 on timeout.
static uint32_t expectPulse(bool level) {
  constexpr uint32_t kMaxCycles = 10000;  // Roughly ~100us safety ceiling on ESP32.
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

  // Send start signal.
  pinMode(kDhtPin, OUTPUT);
  digitalWrite(kDhtPin, HIGH);
  delay(250);
  digitalWrite(kDhtPin, LOW);
  delay(20);  // Datasheet: at least 18ms.
  digitalWrite(kDhtPin, HIGH);
  delayMicroseconds(40);
  pinMode(kDhtPin, INPUT_PULLUP);

  // Sensor response: LOW then HIGH.
  if (!expectPulse(LOW) || !expectPulse(HIGH)) {
    return false;
  }

  // Read 40 bits.
  for (int i = 0; i < 40; ++i) {
    uint32_t lowCycles = expectPulse(LOW);
    if (!lowCycles) {
      return false;
    }
    uint32_t highCycles = expectPulse(HIGH);
    if (!highCycles) {
      return false;
    }
    data[i / 8] <<= 1;
    if (highCycles > lowCycles) {  // Longer HIGH pulse means bit 1.
      data[i / 8] |= 1;
    }
  }

  uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
  if (checksum != data[4]) {
    return false;
  }

  humidity = data[0];
  temperatureC = data[2];
  return true;
}

static bool readDht11WithRetry(int &temperatureC, int &humidity) {
  constexpr int kAttempts = 3;
  for (int i = 0; i < kAttempts; ++i) {
    if (readDht11(temperatureC, humidity)) {
      return true;
    }
    delay(50);  // Short pause before retry.
  }
  return false;
}

static void setupBle() {
  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_N3);  // Moderate TX power to reduce brownout risk.
  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService *service = server->createService(kServiceUuid);

  gTempChar = service->createCharacteristic(
      kTempCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  gHumChar = service->createCharacteristic(
      kHumCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

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
  Serial.println("DHT11 BLE publisher started");
}

void loop() {
  static unsigned long lastReadMs = 0;
  const unsigned long now = millis();
  if (now - lastReadMs < 2000) {
    delay(50);
    return;
  }
  lastReadMs = now;

  int temperatureC = 0;
  int humidity = 0;
  if (!readDht11WithRetry(temperatureC, humidity)) {
    Serial.println("DHT11 read failed after retries");
    return;
  }

  gSmoother.add(temperatureC, humidity);
  const int smoothTemp = gSmoother.avgTemp();
  const int smoothHum = gSmoother.avgHum();

  char tempBuf[8];
  char humBuf[8];
  snprintf(tempBuf, sizeof(tempBuf), "%d", smoothTemp);
  snprintf(humBuf, sizeof(humBuf), "%d", smoothHum);

  gTempChar->setValue((uint8_t *)tempBuf, strlen(tempBuf));
  gHumChar->setValue((uint8_t *)humBuf, strlen(humBuf));
  gTempChar->notify();
  gHumChar->notify();

  Serial.printf("Temp raw/smooth: %d / %d C, Humidity raw/smooth: %d / %d %%\n",
                temperatureC, smoothTemp, humidity, smoothHum);
}
