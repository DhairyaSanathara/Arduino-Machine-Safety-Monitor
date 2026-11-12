// Host-side stand-in for the Adafruit DHT library. The fitted sensor is chosen at
// run time (world.dhtModel); reading it as the wrong model garbles the numbers the
// same way the real library would, so the firmware's auto-detection is tested.
#pragma once
#include "Arduino.h"
#define DHT11 11
#define DHT22 22
struct World;
extern World world;
float worldTempC();
bool worldDhtPresent();
int worldDhtModel();
class DHT {
  uint8_t type; float lastT = NAN, lastH = NAN;
 public:
  DHT(uint8_t, uint8_t t) : type(t) {}
  void begin() {}
  float readTemperature(bool = false, bool = false) {
    simUs += (type == 11) ? 25000 : 5000;          // a real read takes this long
    if (!worldDhtPresent()) { lastT = lastH = NAN; return NAN; }
    float t = worldTempC(); int fitted = worldDhtModel();
    if (fitted == 22 && type == 22) { lastT = std::round(t * 10) / 10.0f; lastH = 45.0f; }
    if (fitted == 11 && type == 11) { lastT = std::round(t); lastH = 45.0f; }
    if (fitted == 11 && type == 22) { lastT = ((int)std::round(t) << 8) * 0.1f; lastH = (45 << 8) * 0.1f; }
    if (fitted == 22 && type == 11) { int r = std::round(t * 10); lastT = (r >> 8) + (r & 0x0F) * 0.1f; lastH = 450 / 256 + (450 % 256) * 0.1f; }
    return lastT;
  }
  float readHumidity(bool = false) { return lastH; }
};
