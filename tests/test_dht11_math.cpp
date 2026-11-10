#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../include/dht11_math.h"

int main() {
  // 1. Valid DHT11 frame: 55% RH, 24 C, checksum = 79
  const uint8_t okFrame[5] = {55, 0, 24, 0, 79};
  int temp = 0, hum = 0;
  assert(decodeDht11Frame(okFrame, temp, hum));
  assert(temp == 24 && hum == 55);

  // 2. Bad checksum rejected
  const uint8_t badChecksum[5] = {55, 0, 24, 0, 80};
  assert(!decodeDht11Frame(badChecksum, temp, hum));

  // 3. Out-of-bounds physical values rejected even with matching modulo-256 checksum
  const uint8_t bogusHum[5] = {150, 0, 24, 0, 174};
  assert(!decodeDht11Frame(bogusHum, temp, hum));

  // 4. Rolling smoother window and reset
  SampleSmoother sm;
  sm.add(24, 50);
  sm.add(26, 54);
  assert(sm.avgTemp() == 25);
  assert(sm.avgHum() == 52);
  sm.reset();
  assert(sm.count == 0 && sm.avgTemp() == 0);

  // 5. Dew point at 100% RH equals dry-bulb temperature
  const float dp100 = computeDewPointC(25.0f, 100.0f);
  assert(fabsf(dp100 - 25.0f) < 0.05f);

  // 6. Heat index increases above dry-bulb temperature at warm/humid conditions
  const float hi = computeHeatIndexC(32.0f, 70.0f);
  assert(hi > 35.0f && hi < 45.0f);

  printf("All C++ dht11_math unit tests passed.\n");
  return 0;
}
