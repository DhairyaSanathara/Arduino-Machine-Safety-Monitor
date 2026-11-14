// ina219.cpp - optional INA219 motor current sensor (HAS_INA219)
//
// NOT fitted on our prototype (HAS_INA219 is 0 in config.h, so most of this is
// compiled out). With it, the motor's current flows through a 0.1 ohm "shunt"
// resistor; the INA219 measures the tiny voltage across it and works out the current.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================== 10. INA219 =================================
// Is an INA219 answering? If so, configure it.
bool initINA() {
  if (!i2cPresent(INA_ADDR)) return false;
  // CONFIG = 0x39FF: 32 V range, +/-320 mV shunt range, bus 12-bit,
  // shunt averaged over 128 samples (68 ms) -> smooths the motor's PWM ripple
  return i2cWrite16(INA_ADDR, 0x00, 0x39FF);
}

// Read current, bus voltage and power. Returns false if the chip did not answer.
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
