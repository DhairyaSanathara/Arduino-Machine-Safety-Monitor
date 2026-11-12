// state.cpp - every piece of shared state, with its starting value
//
// Declared in app.h. Kept in one place so the whole machine state can be read
// at a glance (and inspected by the host-side tests).
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// Output routing
NullOut nullOut;
Print* logOut = &Serial;
bool     textMode = OUTPUT_DEFAULT_TEXT;
uint8_t  plotMode = 0;          // 0 = off, 1 = vibration, 2 = temperature, 3 = both
uint32_t tPlot = 0;

// LCD
LiquidCrystal_I2C* lcd = NULL;            // created only if an LCD is found
uint8_t lcdAddr = 0;
bool     lcdBusOk = true;                 // false = I2C bus jammed (loose / unpowered module)
uint32_t lcdRetryMs = 0, lcdResyncMs = 0;
char lcdCache[2][17];                     // what is on the screen right now
char lcdWant[2][17];                      // what we want on the screen

// Same wire, two ways of talking - the self-test finds out which sensor is fitted
DHT dht22(PIN_DHT, DHT22);
DHT dht11(PIN_DHT, DHT11);
DHT* dht = &dht22;
uint8_t dhtModel = 0;          // 22 or 11 once found

uint8_t mpuAddr = 0x68;

// Sensor health
bool mpuOk = false, tempOk = false;                   // found at self-test
#if HAS_INA219
bool inaOk = false;                                   // INA219 found
#endif
#if HAS_12V_DIVIDER
bool senseOk = false;                                 // 12 V divider found on A3
#endif
bool mpuFail = false, tempFail = false, inaFail = false; // lost while running
uint8_t mpuErrCount = 0, mpuStuckCount = 0, tempBadCount = 0, inaErrCount = 0;

// Measurements
float distCm = DIST_MAX_CM;
float distRaw[3] = { DIST_MAX_CM, DIST_MAX_CM, DIST_MAX_CM };
uint8_t distIdx = 0;

float tempC = NAN;
float humidity = NAN;          // % relative humidity (bonus reading)
float tempRate = 0;            // deg C per minute
bool  tempRateValid = false;
float tBuf[TREND_N];
uint8_t tHead = 0, tCount = 0;

float vibMg = 0, tiltDeg = 0;
float accOff[3] = { 0, 0, 0 };  // last window's mean, subtracted before squaring
float accSum[3], accSq[3];
uint8_t nSamples = 0;
float tiltRef[3];
bool tiltRefSet = false, rezeroTilt = false;

float currentMa = 0, busV = 0, powerW = 0;

// Learned baselines and the limits currently in force
bool  baselineReady = false;
float vibBase = 0, curBase = 0;
float learnVibSum = 0, learnCurSum = 0;
uint8_t learnVibN = 0;
uint16_t learnCurN = 0;
float vibWarn = VIB_ABS_STOP_MG, vibStop = VIB_ABS_STOP_MG;
float curWarn = CUR_ABS_STOP_MA, curStop = CUR_ABS_STOP_MA;

// Decision logic
uint8_t state = ST_IDLE;
uint8_t lvl[R_COUNT];          // 0 = OK, 1 = WARNING, 2 = STOP, for every check
uint8_t worstLevel = 0, worstReason = R_NONE, latchedReason = R_NONE;
uint8_t vibStopCount = 0, curStopCount = 0, powerBadCount = 0, relayBadCount = 0, stallCount = 0;
uint32_t runStartMs = 0, relayChangeMs = 0, lastWarnMs = 0;
bool relayOn = false;
uint8_t pwmNow = 0;
volatile bool estopIrq = false;   // "volatile" = always read it from memory, because it
                                  // can change between any two instructions
bool goRequest = false;

// Speed set from the laptop, and one learned baseline per speed (10 % steps)
uint8_t  speedPct = SPEED_DEFAULT;
uint32_t speedChangeMs = 0;
float    baseVib[11], baseCur[11];
uint16_t baseKnown = 0;          // bit n set = baseline for n x 10 % is learned
uint16_t estopCount = 0;         // E-stops since power-up (shown on the LCD)
int16_t  numIn = -1;             // a number being typed in the Serial Monitor
uint32_t numInMs = 0;

// The data row is sent in pieces, so the loop never waits for the serial port
char     rowBuf[260];
uint8_t  rowLen = 0, rowPos = 0;

// Buttons
Button btnGo   = { PIN_BTN_GO,    false,              false, false, 0, false };
Button btnStop = { PIN_BTN_ESTOP, ESTOP_BUTTON_IS_NC, false, false, 0, false };

// Task timers
uint32_t tSampleUs = 0, tPing = 0, tTemp = 0, tIna = 0, tLogic = 0, tLog = 0,
         tLcd = 0, tLcdPage = 0, tMotor = 0, beepUntil = 0;
uint8_t lcdPage = 0;
