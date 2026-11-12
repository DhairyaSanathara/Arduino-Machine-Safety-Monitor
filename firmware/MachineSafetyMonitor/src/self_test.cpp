// self_test.cpp - power-up sensor check and event logging
//
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

void selfTest() {
  mpuOk = initMPU();
  tempOk = initTempSensor();
#if HAS_INA219
  inaOk = initINA();
#endif
#if HAS_12V_DIVIDER
  senseOk = !inaOk && USE_12V_SENSE && detect12VSense();
#endif
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
