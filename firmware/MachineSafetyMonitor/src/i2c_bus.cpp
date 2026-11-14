// i2c_bus.cpp - low-level I2C register access
//
// Used by the MPU6050 and the optional INA219.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// =========================== 8. I2C LOW LEVEL ==============================
// I2C basics: two wires (SDA data, SCL clock) shared by every device. Each device
// has an address (MPU6050 0x68, LCD 0x27, INA219 0x40). The Arduino sends the
// address; the device with that address answers with an ACK. Inside each device
// are numbered "registers" (settings and readings) that we write or read.
// endTransmission() returns 0 when the device acknowledged everything.

// Is a device answering at this address? (sends just the address)
bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Write one byte into register reg of device addr.
bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// Write a 16-bit value, high byte first.
bool i2cWrite16(uint8_t addr, uint8_t reg, uint16_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val & 0xFF));
  return Wire.endTransmission() == 0;
}

// Read a 16-bit register: first tell the device which register, then ask for 2 bytes.
// Returns false if the device did not answer (loose wire), so the caller can count errors.
bool i2cRead16(uint8_t addr, uint8_t reg, uint16_t &out) {
  Wire.beginTransmission(addr);
  Wire.write(reg);                                  // "I want register reg"
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)2) != 2) return false;   // "send me 2 bytes"
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  out = ((uint16_t)hi << 8) | lo;                   // join the two bytes into one number
  return true;
}
