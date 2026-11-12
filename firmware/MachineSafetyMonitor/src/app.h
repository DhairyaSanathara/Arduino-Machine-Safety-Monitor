// app.h - shared types, state and functions of the firmware.
//
// Every module includes this one header. The machine's state lives in
// globals (defined in state.cpp) because an 8-bit microcontroller with 2 KB of
// RAM has no room for anything heavier, and the E-stop interrupt must be able
// to reach the outputs directly.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#define FIRMWARE_VERSION "2.2"

// ============================ STATES AND REASONS ==========================
enum : uint8_t { ST_IDLE, ST_RUN, ST_WARN, ST_ESTOP, ST_FAULT };

// Reasons in priority order: when two happen together, the lower number is shown
enum : uint8_t {
  R_NONE, R_ESTOP_BTN, R_REMOTE, R_RELAY, R_NOPOWER,
  R_SENS_TEMP, R_SENS_MPU, R_SENS_INA,
  R_PROX, R_CURRENT, R_STALL, R_TEMP, R_TRATE, R_VIB, R_TILT,
  R_COUNT
};

extern const char* const STATE_NAME[] PROGMEM;   // "IDLE", "RUN", ...
extern const char* const REASON_TEXT[] PROGMEM;  // LCD text, max 16 characters
extern const char* const REASON_CODE[] PROGMEM;  // short codes for the PC

// =============================== OUTPUT ROUTING ===========================
// All text output goes through LOG. While a graph is shown in the Serial
// Plotter, LOG is switched to "nowhere", so only graph values reach the plotter.
class NullOut : public Print {
 public:
  size_t write(uint8_t) { return 1; }
};
extern NullOut nullOut;
extern Print* logOut;
#define LOG (*logOut)

// What the serial port sends:
//   readable text (default) - a labelled report every 2 s, for the Serial Monitor
//   data (d)                - one CSV row every 0.5 s, for the dashboard / Excel
//   graphs (v / t / b)      - numbers only, for the IDE's Serial Plotter
#ifndef OUTPUT_DEFAULT_TEXT
#define OUTPUT_DEFAULT_TEXT 1
#endif

const uint8_t INA_ADDR = 0x40;
const uint8_t LCD_UNITS_PER_PASS = 6;   // LCD writes per loop pass (1 = a character or a cursor jump)

// ================================ SHARED STATE ============================
// (all defined in state.cpp)
extern bool     textMode;
extern uint8_t  plotMode;
extern uint32_t tPlot;
extern LiquidCrystal_I2C* lcd;
extern uint8_t  lcdAddr;
extern bool     lcdBusOk;
extern uint32_t lcdRetryMs, lcdResyncMs;
extern char     lcdCache[2][17];
extern char     lcdWant[2][17];

extern DHT      dht22;
extern DHT      dht11;
extern DHT*     dht;
extern uint8_t  dhtModel;
extern uint8_t  mpuAddr;

// Sensor health
extern bool mpuOk, tempOk;
#if HAS_INA219
extern bool inaOk;
#else
const bool inaOk = false;               // not fitted: its code is left out
#endif
#if HAS_12V_DIVIDER
extern bool senseOk;
#else
const bool senseOk = false;
#endif
extern bool mpuFail, tempFail, inaFail;
extern uint8_t mpuErrCount, mpuStuckCount, tempBadCount, inaErrCount;

// Measurements
extern float   distCm;
extern float   distRaw[3];
extern uint8_t distIdx;
extern float   tempC, humidity, tempRate;
extern bool    tempRateValid;
extern float   tBuf[TREND_N];
extern uint8_t tHead, tCount;
extern float   vibMg, tiltDeg;
extern float   accOff[3], accSum[3], accSq[3];
extern uint8_t nSamples;
extern float   tiltRef[3];
extern bool    tiltRefSet, rezeroTilt;
extern float   currentMa, busV, powerW;

// Learned baselines and the limits in force
extern bool     baselineReady;
extern float    vibBase, curBase, learnVibSum, learnCurSum;
extern uint8_t  learnVibN;
extern uint16_t learnCurN;
extern float    vibWarn, vibStop, curWarn, curStop;

// Decision logic
extern uint8_t  state;
extern uint8_t  lvl[R_COUNT];
extern uint8_t  worstLevel, worstReason, latchedReason;
extern uint8_t  vibStopCount, curStopCount, powerBadCount, relayBadCount, stallCount;
extern uint32_t runStartMs, relayChangeMs, lastWarnMs;
extern bool     relayOn;
extern uint8_t  pwmNow;
extern volatile bool estopIrq;          // set by the E-stop interrupt, cleared by loop()
extern bool     goRequest;

// Speed and per-speed baselines (10 % steps)
extern uint8_t  speedPct;
extern uint32_t speedChangeMs;
extern float    baseVib[11], baseCur[11];
extern uint16_t baseKnown;
extern uint16_t estopCount;
extern int16_t  numIn;
extern uint32_t numInMs;

// Data row being sent in pieces
extern char     rowBuf[260];
extern uint8_t  rowLen, rowPos;

// Buttons
struct Button {
  uint8_t pin; bool nc; bool stable; bool last; uint32_t changedMs; bool pressedEvent;
};
extern Button btnGo, btnStop;

// Task timers
extern uint32_t tSampleUs, tPing, tTemp, tIna, tLogic, tLog, tLcd, tLcdPage, tMotor, beepUntil;
extern uint8_t  lcdPage;

// ================================== FUNCTIONS =============================
// util.cpp
void pgmText(const char* const* table, uint8_t i, char* out, uint8_t len);
void printPgm(const char* const* table, uint8_t i);
bool isRunning();
bool motorChecksActive(uint32_t now);
uint8_t pctToPwm(uint8_t pct);
uint8_t pwmToPct(uint8_t pwm);
uint8_t runPwm();
uint8_t warnPwm();
void motorPwm(uint8_t v);
void setRelay(bool on);
void setBuzzer(bool on);
void motorStopNow();
void motorDrive(uint8_t pwm);
void beep(uint16_t ms);
float median3(float a, float b, float c);
uint8_t levelHigh(float v, float warn, float stop, float hyst, uint8_t prev);
uint8_t levelLow(float v, float warn, float stop, float hyst, uint8_t prev);

// i2c_bus.cpp
bool i2cPresent(uint8_t addr);
bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val);
bool i2cWrite16(uint8_t addr, uint8_t reg, uint16_t val);
bool i2cRead16(uint8_t addr, uint8_t reg, uint16_t &out);

// mpu6050.cpp
bool readAccel(int16_t a[3]);
bool initMPU();

// ina219.cpp
bool initINA();
bool readINA();

// dht_sensor.cpp
void waitFed(uint16_t ms);
bool readDhtAs(uint8_t model, float& t, float& h);
bool initTempSensor();

// self_test.cpp
bool detect12VSense();
void logEventStart(const __FlashStringHelper* what);
void selfTest();

// sensors.cpp
void taskUltrasonic(uint32_t now);
float trendSlopePerMin();
void taskTemperature(uint32_t now);
void updateLimits();
void finishLearning();
void onVibrationWindow(uint32_t now);
void finishWindow(uint32_t now);
void taskMPU(uint32_t nowUs, uint32_t now);
void taskPower(uint32_t now);

// safety.cpp
void evaluate(uint32_t now);
void changeState(uint8_t s, uint8_t reason);
void clearMotorChecks();
void enterIdle();
void startLearning();
void selectBaseline();
void enterRun();
void enterEstop(uint8_t reason);
void enterFault(uint8_t reason);
void runStateMachine(uint32_t now);

// motor.cpp
void taskMotor(uint32_t now);
void onEstopInterrupt();

// commands.cpp
void updateButton(Button &b, uint32_t now);
void printHelp();
void setSpeed(int16_t pct);
void taskSerialIn();

// reports.cpp
void setPlotMode(uint8_t m);
void textReport();
void plotPair(const __FlashStringHelper* name, float v, uint8_t dec, bool last);
void taskPlot(uint32_t now);
void printLimitPair(float w, float st, uint8_t dec);
void printStatus();

// indicators.cpp
void taskIndicators(uint32_t now);
void lampTest();

// lcd.cpp
void lcdLine(uint8_t row, const char* text);
void lcdNibble(uint8_t n);
void lcdResync();
bool lcdFlush(uint8_t budget);
void valuePage(uint8_t p, char* out);
void taskLCD(uint32_t now);
void initLCD();

// datalog.cpp
char* addNum(char* p, float v, uint8_t dec);
void rowPump();
void rowFinish();
void taskLog(uint32_t now);
void printDataHeader();
void printHeader();
