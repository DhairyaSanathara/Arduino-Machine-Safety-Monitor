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
// The five states of the machine (an "enum" gives each name a number 0, 1, 2...):
//   ST_IDLE  motor off, ready, waiting for GO
//   ST_RUN   motor running at the speed set from the laptop
//   ST_WARN  motor slowed to 45 % because a warning limit is crossed
//   ST_ESTOP motor off and LATCHED: needs the danger gone AND a GO press
//   ST_FAULT motor off: a sensor was missing at power-up
enum : uint8_t { ST_IDLE, ST_RUN, ST_WARN, ST_ESTOP, ST_FAULT };

// Reasons in priority order: when two happen together, the lower number is shown
enum : uint8_t {
  R_NONE, R_ESTOP_BTN, R_REMOTE, R_RELAY, R_NOPOWER,
  R_SENS_TEMP, R_SENS_MPU, R_SENS_INA,
  R_PROX, R_CURRENT, R_STALL, R_TEMP, R_TRATE, R_VIB, R_TILT,
  R_COUNT
};

// Text for each state and reason, kept in flash memory (see strings.cpp).
extern const char* const STATE_NAME[] PROGMEM;   // "IDLE", "RUN", ...
extern const char* const REASON_TEXT[] PROGMEM;  // LCD text, max 16 characters
extern const char* const REASON_CODE[] PROGMEM;  // short codes for the PC

// =============================== OUTPUT ROUTING ===========================
// All text output goes through LOG. While a graph is shown in the Serial
// Plotter, LOG is switched to "nowhere", so only graph values reach the plotter.
// NullOut is a "Print" that throws everything away: printing to it costs nothing.
class NullOut : public Print {
 public:
  size_t write(uint8_t) { return 1; }
};
extern NullOut nullOut;
extern Print* logOut;                   // points at Serial, or at nullOut while plotting
#define LOG (*logOut)                   // so LOG.print(...) goes wherever logOut points

// What the serial port sends:
//   readable text (default) - a labelled report every 2 s, for the Serial Monitor
//   data (d)                - one CSV row every 0.5 s, for the dashboard / Excel
//   graphs (v / t / b)      - numbers only, for the IDE's Serial Plotter
#ifndef OUTPUT_DEFAULT_TEXT
#define OUTPUT_DEFAULT_TEXT 1
#endif

const uint8_t INA_ADDR = 0x40;          // I2C address of the optional INA219 current sensor
const uint8_t LCD_UNITS_PER_PASS = 6;   // LCD writes per loop pass (1 = a character or a cursor jump)

// ================================ SHARED STATE ============================
// (all defined in state.cpp)
// "extern" means: this variable exists, it is created in another file (state.cpp).
// Every module can read and change it, and there is only one copy in RAM.
extern bool     textMode;               // true = readable reports, false = CSV data rows
extern uint8_t  plotMode;
extern uint32_t tPlot;
extern LiquidCrystal_I2C* lcd;          // the LCD object, or NULL if no LCD was found
extern uint8_t  lcdAddr;
extern bool     lcdBusOk;
extern uint32_t lcdRetryMs, lcdResyncMs;
extern char     lcdCache[2][17];
extern char     lcdWant[2][17];

extern DHT      dht22;                  // two driver objects for the same pin:
extern DHT      dht11;                  // the self-test finds out which sensor is fitted
extern DHT*     dht;                    // ...and points this at the right one
extern uint8_t  dhtModel;
extern uint8_t  mpuAddr;

// Sensor health
extern bool mpuOk, tempOk;              // found at power-up
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
extern bool mpuFail, tempFail, inaFail;  // lost while running (each becomes a stop reason)
extern uint8_t mpuErrCount, mpuStuckCount, tempBadCount, inaErrCount;  // failures in a row

// Measurements
extern float   distCm;                  // filtered distance to a person, cm
extern float   distRaw[3];              // last 3 raw pings (for the median filter)
extern uint8_t distIdx;                 // where the next ping goes in distRaw
extern float   tempC, humidity, tempRate;   // deg C, % RH, deg C per minute
extern bool    tempRateValid;           // false until enough readings for a trend
extern float   tBuf[TREND_N];           // ring buffer of recent temperatures
extern uint8_t tHead, tCount;           // next slot in tBuf, and how many are filled
extern float   vibMg, tiltDeg;          // vibration RMS (milli-g) and tilt (degrees)
extern float   accOff[3], accSum[3], accSq[3];  // per-axis mean, sum, sum of squares
extern uint8_t nSamples;                // samples collected in the current 0.5 s window
extern float   tiltRef[3];              // the "level" direction of gravity when installed
extern bool    tiltRefSet, rezeroTilt;  // reference taken? / 'z' asked for a new one
extern float   currentMa, busV, powerW; // optional INA219 readings

// Learned baselines and the limits in force
extern bool     baselineReady;          // true once this speed's normal vibration is known
extern float    vibBase, curBase, learnVibSum, learnCurSum;  // baseline and learning sums
extern uint8_t  learnVibN;              // vibration windows collected while learning
extern uint16_t learnCurN;
extern float    vibWarn, vibStop, curWarn, curStop;  // limits worked out from the baseline

// Decision logic
extern uint8_t  state;                  // current state: ST_IDLE ... ST_FAULT
extern uint8_t  lvl[R_COUNT];           // grade of every check: 0 OK, 1 warning, 2 stop
extern uint8_t  worstLevel, worstReason, latchedReason;  // worst grade now / why / why we stopped
extern uint8_t  vibStopCount, curStopCount, powerBadCount, relayBadCount, stallCount;  // "in a row" counters
extern uint32_t runStartMs, relayChangeMs, lastWarnMs;   // time stamps in ms
extern bool     relayOn;                // what we last told the relay to do
extern uint8_t  pwmNow;                 // PWM currently sent to the motor (0..255)
extern volatile bool estopIrq;          // set by the E-stop interrupt, cleared by loop()
extern bool     goRequest;              // GO was pressed (button or 'g'); handled by the state machine

// Speed and per-speed baselines (10 % steps)
extern uint8_t  speedPct;               // speed set point, 10..100 %
extern uint32_t speedChangeMs;          // when it last changed (checks wait for the motor to settle)
extern float    baseVib[11], baseCur[11];   // learned baseline for 0 %, 10 %, ... 100 %
extern uint16_t baseKnown;              // bit n set = baseline for n x 10 % is learned
extern uint16_t estopCount;             // emergency stops since power-up
extern int16_t  numIn;                  // digits typed so far in the Serial Monitor (-1 = none)
extern uint32_t numInMs;                // when the last digit arrived

// Data row being sent in pieces
extern char     rowBuf[260];            // the CSV row being sent
extern uint8_t  rowLen, rowPos;         // its length, and how much is already sent

// Buttons
// One debounced push button:
//   pin          Arduino pin
//   nc           true for a normally-closed contact (a real mushroom E-stop)
//   stable       the debounced state: true = pressed
//   last         the raw reading last time, to notice a change
//   changedMs    when the raw reading last changed
//   pressedEvent set once per press, cleared by whoever handles it
struct Button {
  uint8_t pin; bool nc; bool stable; bool last; uint32_t changedMs; bool pressedEvent;
};
extern Button btnGo, btnStop;

// Task timers: the last time each task ran (ms, or us for tSampleUs). A task runs
// again when "now - its timer" reaches its period from config.h.
extern uint32_t tSampleUs, tPing, tTemp, tIna, tLogic, tLog, tLcd, tLcdPage, tMotor, beepUntil;
extern uint8_t  lcdPage;                // which reading the LCD's second line shows

// ================================== FUNCTIONS =============================
// Prototypes: the name and inputs of every function, grouped by the file it lives in.
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
