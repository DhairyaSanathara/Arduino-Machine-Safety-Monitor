// dht_sensor.cpp - DHT22 / DHT11 temperature and humidity
//
// Works out which model is fitted by decoding the first frame both ways.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================== 11. DHT22 / DHT11 ==========================
// Both sensors use the same 3 wires but talk differently:
//   DHT22 (white): 1 ms wake-up pulse,  0.1 C steps, -40..80 C
//   DHT11 (blue) : 18 ms wake-up pulse, 1 C steps,     0..50 C
// To find out which one is fitted, read it both ways and keep the one that
// makes sense:
//   a DHT11 read as a DHT22 gives humidity over 100 %  (or does not answer)
//   a DHT22 read as a DHT11 gives a temperature below 3 C (impossible indoors)
// Each sensor answers at most once every 2 s, and needs ~2 s after power-up.
// Waits are chopped into 100 ms pieces so the watchdog stays happy.
// delay() that keeps feeding the watchdog (only used during the power-up self-test).
void waitFed(uint16_t ms) {
  for (uint16_t i = 0; i < ms; i += 100) { delay(100); wdt_reset(); }
}

// Read the sensor as model 11 or 22 and say whether the numbers make sense.
// t and h are passed "by reference" (&), so the function fills in the caller's variables.
bool readDhtAs(uint8_t model, float& t, float& h) {
  DHT& s = (model == 11) ? dht11 : dht22;
  t = s.readTemperature(false, true);      // (false = Celsius, true = force a fresh reading)
  h = s.readHumidity();                    // from the same reading, no second wait
  if (isnan(t) || isnan(h) || h > 100.5) return false;   // no answer, or humidity over 100 %
  if (model == 11) return t >= 3 && t <= 60;
  return t > -40 && t < 80;
}

// Find the sensor at power-up: try one model, then the other, up to 4 attempts.
// On success it also stores the first reading as the start of the temperature trend.
bool initTempSensor() {
  dht22.begin();
  dht11.begin();
  tCount = 0; tHead = 0; tempRateValid = false;
  uint8_t first  = (dhtModel == 11) ? 11 : 22;   // try last time's type first
  uint8_t second = (first == 11) ? 22 : 11;
  uint8_t model = 0;
  for (uint8_t attempt = 0; attempt < 4; attempt++) {
    if (attempt) waitFed(model == 22 ? 1100 : 2100);   // DHT11 is ready again after 1 s
    model = (attempt % 2 == 0) ? first : second;
    float t, h;
    if (readDhtAs(model, t, h)) {
      dhtModel = model;
      dht = (model == 11) ? &dht11 : &dht22;
      tempC = t;
      humidity = h;
      tBuf[0] = t; tHead = 1; tCount = 1;
      tTemp = millis();
      return true;
    }
  }
  return false;
}
