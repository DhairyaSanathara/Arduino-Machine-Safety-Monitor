// strings.cpp - state names and stop reasons, stored in flash (PROGMEM)
//
// Short codes go to the PC (CSV and events); full text goes to the LCD.
//
// Why PROGMEM: the Uno has only 2 KB of RAM but 32 KB of flash. Normal string
// constants are copied into RAM at start-up; PROGMEM keeps them in flash only,
// and pgmText() (util.cpp) copies one into RAM when it is needed.
// The order of each table MUST match the enums in app.h.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

const char SN0[] PROGMEM = "IDLE";
const char SN1[] PROGMEM = "RUN";
const char SN2[] PROGMEM = "WARNING";
const char SN3[] PROGMEM = "ESTOP";
const char SN4[] PROGMEM = "FAULT";
const char* const STATE_NAME[] PROGMEM = { SN0, SN1, SN2, SN3, SN4 };

// LCD text (max 16 characters)
const char RT0[]  PROGMEM = "ALL OK";
const char RT1[]  PROGMEM = "E-STOP BUTTON";
const char RT2[]  PROGMEM = "REMOTE STOP";
const char RT3[]  PROGMEM = "RELAY STUCK ON";
const char RT4[]  PROGMEM = "NO MOTOR POWER";
const char RT5[]  PROGMEM = "TEMP SENSOR ERR";
const char RT6[]  PROGMEM = "MPU6050 ERROR";
const char RT7[]  PROGMEM = "INA219 ERROR";
const char RT8[]  PROGMEM = "PERSON IN ZONE";
const char RT9[]  PROGMEM = "OVERLOAD / JAM";
const char RT10[] PROGMEM = "MOTOR STALLED";
const char RT11[] PROGMEM = "MOTOR OVERHEAT";
const char RT12[] PROGMEM = "TEMP RISING FAST";
const char RT13[] PROGMEM = "HIGH VIBRATION";
const char RT14[] PROGMEM = "MACHINE TILTED";
const char* const REASON_TEXT[] PROGMEM = {
  RT0, RT1, RT2, RT3, RT4, RT5, RT6, RT7, RT8, RT9, RT10, RT11, RT12, RT13, RT14 };

// Short codes for the CSV log (no spaces, easy for software to read)
const char RC0[]  PROGMEM = "OK";
const char RC1[]  PROGMEM = "ESTOP_BTN";
const char RC2[]  PROGMEM = "REMOTE";
const char RC3[]  PROGMEM = "RELAY_STUCK";
const char RC4[]  PROGMEM = "NO_POWER";
const char RC5[]  PROGMEM = "SENS_TEMP";
const char RC6[]  PROGMEM = "SENS_MPU";
const char RC7[]  PROGMEM = "SENS_INA";
const char RC8[]  PROGMEM = "PROX";
const char RC9[]  PROGMEM = "OVERLOAD";
const char RC10[] PROGMEM = "STALL";
const char RC11[] PROGMEM = "OVERHEAT";
const char RC12[] PROGMEM = "TEMP_RATE";
const char RC13[] PROGMEM = "VIBRATION";
const char RC14[] PROGMEM = "TILT";
const char* const REASON_CODE[] PROGMEM = {
  RC0, RC1, RC2, RC3, RC4, RC5, RC6, RC7, RC8, RC9, RC10, RC11, RC12, RC13, RC14 };
