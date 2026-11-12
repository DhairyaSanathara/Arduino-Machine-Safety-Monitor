// mpu6050.cpp - MPU6050 accelerometer, read straight from its registers
//
// Wakes the chip, sets +/-4 g and the 44 Hz low-pass filter, reads X / Y / Z.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================= 9. MPU6050 ==================================
bool readAccel(int16_t a[3]) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);                                    // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(mpuAddr, (uint8_t)6) != 6) return false;
  for (uint8_t k = 0; k < 3; k++) {
    uint8_t hi = Wire.read();                          // read in two steps: C++ does not
    uint8_t lo = Wire.read();                          // promise the order inside one line
    a[k] = (int16_t)(((uint16_t)hi << 8) | lo);
  }
  return true;
}

bool initMPU() {
  if (i2cPresent(0x68)) mpuAddr = 0x68;
  else if (i2cPresent(0x69)) mpuAddr = 0x69;
  else return false;
  bool ok = i2cWrite8(mpuAddr, 0x6B, 0x01);  // PWR_MGMT_1: wake up, clock from gyro PLL
  delay(10);
  ok = ok && i2cWrite8(mpuAddr, 0x1A, 0x03); // CONFIG: digital low-pass filter ~44 Hz
  ok = ok && i2cWrite8(mpuAddr, 0x1C, 0x08); // ACCEL_CONFIG: +/-4 g (8192 counts per g)
  delay(20);
  int16_t a[3];
  if (!ok || !readAccel(a)) return false;
  for (uint8_t k = 0; k < 3; k++) { accOff[k] = a[k]; accSum[k] = 0; accSq[k] = 0; }
  nSamples = 0;
  return true;
}
