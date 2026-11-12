// Host-side stand-in for the Wire (I2C) library. Devices are modelled in sim.cpp.
#pragma once
#include "Arduino.h"
class WireMock {
 public:
  uint8_t addr = 0, reg = 0, nw = 0;
  uint8_t wbuf[8];
  uint8_t rbuf[8], rlen = 0, rpos = 0;
  void begin() {}
  void setClock(long) {}
  void setWireTimeout(uint32_t, bool) {}
  void beginTransmission(uint8_t a) { addr = a; nw = 0; }
  void write(uint8_t v) { if (nw < 8) wbuf[nw] = v; nw++; }
  uint8_t endTransmission(bool stop = true);
  uint8_t requestFrom(uint8_t a, uint8_t n);
  int read() { return rpos < rlen ? rbuf[rpos++] : 0xFF; }
};
extern WireMock Wire;
