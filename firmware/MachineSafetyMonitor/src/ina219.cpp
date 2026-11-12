// ina219.cpp - optional INA219 motor current sensor (HAS_INA219)
//
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================== 10. INA219 =================================
bool initINA() {
  if (!i2cPresent(INA_ADDR)) return false;
  // CONFIG = 0x39FF: 32 V range, +/-320 mV shunt range, bus 12-bit,
  // shunt averaged over 128 samples (68 ms) -> smooths the motor's PWM ripple
  return i2cWrite16(INA_ADDR, 0x00, 0x39FF);
}

bool readINA() {
  uint16_t sh, bus;
  if (!i2cRead16(INA_ADDR, 0x01, sh)) return false;   // shunt voltage, 10 uV per count
  if (!i2cRead16(INA_ADDR, 0x02, bus)) return false;  // bus voltage, 4 mV per count (bits 15..3)
  float shunt_mV = (int16_t)sh * 0.01;
  currentMa = fabs(shunt_mV / SHUNT_OHMS);             // Ohm's law: I = V / R
  busV = (bus >> 3) * 0.004;
  powerW = busV * currentMa / 1000.0;                  // P = V x I
  return true;
}
