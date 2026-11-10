#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

// Rolling average filter to smooth discrete DHT11 readings.
struct SampleSmoother {
  static constexpr size_t kWindow = 4;
  int temps[kWindow] = {0};
  int hums[kWindow] = {0};
  size_t count = 0;
  size_t idx = 0;

  void reset() {
    for (size_t i = 0; i < kWindow; ++i) {
      temps[i] = 0;
      hums[i] = 0;
    }
    count = 0;
    idx = 0;
  }

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
    const int n = static_cast<int>(count);
    return (sum >= 0) ? ((sum + n / 2) / n) : ((sum - n / 2) / n);
  }

  int avgHum() const {
    if (count == 0) return 0;
    int sum = 0;
    for (size_t i = 0; i < count; ++i) sum += hums[i];
    return (sum + static_cast<int>(count / 2)) / static_cast<int>(count);
  }
};

// Validate 5-byte DHT11 frame checksum and physical bounds.
inline bool decodeDht11Frame(const uint8_t data[5], int &temperatureC, int &humidityPct) {
  const uint8_t checksum = static_cast<uint8_t>((data[0] + data[1] + data[2] + data[3]) & 0xFFU);
  if (checksum != data[4]) {
    return false;
  }
  const int hum = static_cast<int>(data[0]);
  int temp = static_cast<int>(data[2]);
  if ((data[3] & 0x80U) != 0U) {
    temp = -temp;
  }
  if (hum < 1 || hum > 100 || temp < -20 || temp > 60) {
    return false;
  }
  humidityPct = hum;
  temperatureC = temp;
  return true;
}

// Magnus-Tetens approximation for dew point (Celsius).
inline float computeDewPointC(float tempC, float relHumidityPct) {
  const float rh = (relHumidityPct < 1.0f) ? 1.0f : ((relHumidityPct > 100.0f) ? 100.0f : relHumidityPct);
  constexpr float kA = 17.625f;
  constexpr float kB = 243.04f;
  const float alpha = logf(rh / 100.0f) + (kA * tempC) / (kB + tempC);
  return (kB * alpha) / (kA - alpha);
}

// NOAA Steadman / Rothfusz heat index approximation converted to Celsius.
inline float computeHeatIndexC(float tempC, float relHumidityPct) {
  const float rh = (relHumidityPct < 0.0f) ? 0.0f : ((relHumidityPct > 100.0f) ? 100.0f : relHumidityPct);
  const float tempF = tempC * 1.8f + 32.0f;
  float hiF = 0.5f * (tempF + 61.0f + ((tempF - 68.0f) * 1.2f) + (rh * 0.094f));
  if (hiF >= 80.0f) {
    hiF = -42.379f + 2.04901523f * tempF + 10.14333127f * rh
          - 0.22475541f * tempF * rh - 0.00683783f * tempF * tempF
          - 0.05481717f * rh * rh + 0.00122874f * tempF * tempF * rh
          + 0.00085282f * tempF * rh * rh - 0.00000199f * tempF * tempF * rh * rh;
  }
  return (hiF - 32.0f) * (5.0f / 9.0f);
}
