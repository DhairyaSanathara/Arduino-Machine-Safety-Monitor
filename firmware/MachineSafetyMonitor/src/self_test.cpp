// self_test.cpp - power-up sensor check and event logging
//
// selfTest() finds every sensor at power-up. setup() then refuses to run
// (FAULT) if the MPU6050 or the DHT is missing.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================== 12. SELF-TEST ==============================
// Is the 12 V divider fitted? Turn on A3's internal pull-up (~35k to 5V) for a
// moment. With the divider, its 4.7k to GND holds the pin low (~0.6 V).
// With nothing connected, the pull-up wins and the pin reads ~5 V.
bool detect12VSense() {
  pinMode(PIN_12V_SENSE, INPUT_PULLUP);
  delay(5);
  analogRead(PIN_12V_SENSE);                 // first reading settles the ADC
  int raw = analogRead(PIN_12V_SENSE);
  pinMode(PIN_12V_SENSE, INPUT);
  return raw < 600;                          // below ~2.9 V = divider present
}

// Start an event line. Events are things that HAPPEN (state change, speed change,
// baseline learned) as opposed to the regular readings. F("...") keeps the text in flash.
void logEventStart(const __FlashStringHelper* what) {
  rowFinish();                     // never print into the middle of a data row
  if (textMode) {
    LOG.print(F(">> "));           // readable: ">> SPEED,60"
  } else {
    LOG.print(F("E,"));            // dashboard: "E,12345,SPEED,60"
    LOG.print(millis());
    LOG.print(',');
  }
  LOG.print(what);
}

// Check every sensor and report the result, e.g.
//   E,1205,SELFTEST,MPU6050=OK DHT22=OK INA219=not fitted 12V_SENSE=not fitted LCD=OK
void selfTest() {
  mpuOk = initMPU();                       // mpu6050.cpp
  tempOk = initTempSensor();               // dht_sensor.cpp
#if HAS_INA219
  inaOk = initINA();
#endif
#if HAS_12V_DIVIDER
  senseOk = !inaOk && USE_12V_SENSE && detect12VSense();
#endif
  // A missing sensor counts as "failed" so evaluate() treats it as a stop reason
  mpuFail = !mpuOk;
  tempFail = !tempOk;
  inaFail = false;
  mpuErrCount = mpuStuckCount = tempBadCount = inaErrCount = 0;

  logEventStart(F("SELFTEST"));
  LOG.print(F(",MPU6050=")); LOG.print(mpuOk ? F("OK") : F("MISSING"));
  LOG.print(!tempOk ? F(" DHT=MISSING") : (dhtModel == 11 ? F(" DHT11=OK") : F(" DHT22=OK")));
  LOG.print(F(" INA219="));  LOG.print(inaOk ? F("OK") : F("not fitted"));
  LOG.print(F(" 12V_SENSE=")); LOG.print(senseOk ? F("A3") : F("not fitted"));
  LOG.print(F(" LCD="));     LOG.println(lcd ? F("OK") : F("not found"));
}
