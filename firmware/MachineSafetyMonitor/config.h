// config.h - everything you may need to change for your own hardware.
//
// Pins, hardware options, safety limits and timing. The rest of the firmware
// only reads these values, so a fork for different hardware usually needs
// changes here and nowhere else. See docs/wiring.md for what each one means.
#pragma once
#include <Arduino.h>

// ============================== 1. PIN MAP ================================
#define PIN_BTN_GO      2   // green push button -> GND   (START / RESET)
#define PIN_BTN_ESTOP   3   // red push button   -> GND   (EMERGENCY STOP, interrupt pin)
#define PIN_DHT         4   // DHT22 or DHT11 DATA
#define PIN_BUZZER      8   // active buzzer (+)
#define PIN_RELAY       9   // relay module IN  (9 or 12 - the pin your relay wire is on)
#define PIN_LED_GREEN  A0   // analog pins work as normal digital outputs too
#define PIN_LED_YELLOW A1
#define PIN_LED_RED    A2
#define PIN_12V_SENSE  A3   // OPTIONAL 12 V sense divider (10k from L298N +12V, 4.7k to GND)
// I2C bus: SDA = A4, SCL = A5 -> LCD (0x27 or 0x3F), MPU6050 (0x68), INA219 (0x40)

// Motor: the machine only ever turns ONE way, so the L298N's direction inputs are
// wired straight to the breadboard (IN1 -> + rail, IN2 -> - rail). The Arduino
// controls only POWER (relay, D9) and SPEED (PWM on ENA).
// ENA_PIN = the pin that drives ENA: 5, 6, 10 or 11 (pwm_finder.ino shows which
// ones work on a damaged board). The ultrasonic moves out of the way by itself:
//   ENA 5 or 6 : TRIG D11, ECHO D10   (normal)
//   ENA 10     : TRIG D11, ECHO A3
//   ENA 11     : TRIG D13, ECHO D10
#define ENA_PIN         5
#define PIN_MOTOR_EN    ENA_PIN
// The same speed signal is ALSO sent on the other one of D5 / D6, so ENA works on
// either pin (like pwm_finder) - useful on a board with damaged pins.
#if ENA_PIN == 5
  #define PIN_MOTOR_EN2 6
#elif ENA_PIN == 6
  #define PIN_MOTOR_EN2 5
#endif
#if ENA_PIN == 10
  #define PIN_US_ECHO  A3
#else
  #define PIN_US_ECHO  10   // HC-SR04 ECHO
#endif
#if ENA_PIN == 11
  #define PIN_US_TRIG  13
#else
  #define PIN_US_TRIG  11   // HC-SR04 TRIG
#endif

// ============================ 2. HARDWARE OPTIONS ==========================
const bool RELAY_ACTIVE_LOW   = true;   // blue 1-channel modules with optocoupler: ON = LOW
const bool BUZZER_ACTIVE_LOW  = false;  // true only for "low level trigger" buzzer modules
const bool ESTOP_BUTTON_IS_NC = false;  // true if you fit a real mushroom E-stop (NC contact)
const bool USE_WATCHDOG       = true;   // keep true on an Uno R3
const uint32_t SERIAL_BAUD    = 9600;   // Serial Monitor must be set to the same
// Optional extras. Set to 1 only if you fit them (they cost program memory):
#define HAS_INA219      0               // INA219 current sensor on I2C (0x40)
#define HAS_12V_DIVIDER 0               // 10k / 4.7k divider from L298N +12V to A3
const bool USE_12V_SENSE      = HAS_12V_DIVIDER && (ENA_PIN != 10);
const float SENSE_RATIO       = (10.0 + 4.7) / 4.7;   // divider: 12 V becomes 3.84 V at A3
const float SHUNT_OHMS        = 0.1;    // only for the OPTIONAL INA219 (shunt marked R100)
// 0 on the real machine. hardware/wokwi/make_sketch.py sets it to 1 for the Wokwi
// simulation, whose MPU6050 gives perfectly steady numbers (no noise at all): there
// the "frozen sensor" check would wrongly report the MPU6050 as stuck.
#define WOKWI_SIMULATION 0

// ============================== 3. SAFETY LIMITS ===========================
// Proximity, cm (smaller = more dangerous)
const float DIST_WARN_CM    = 40.0;
const float DIST_STOP_CM    = 15.0;
const float DIST_HYST_CM    = 5.0;     // must move 5 cm further away to clear a warning
const float DIST_MAX_CM     = 150.0;   // no echo / farther = "zone clear"
// Air temperature right next to the motor (DHT22 or DHT11), deg C
const float TEMP_WARN_C     = 40.0;
const float TEMP_STOP_C     = 50.0;
const float TEMP_HYST_C     = 2.0;
// Temperature rise rate, deg C per minute
const float TRATE_WARN      = 6.0;
const float TRATE_STOP      = 15.0;
const float TRATE_HYST      = 1.5;
// Vibration RMS, milli-g. Limits = learned baseline x factor
const float VIB_WARN_FACTOR = 2.0;
const float VIB_STOP_FACTOR = 3.5;
const float VIB_FLOOR_MG    = 40.0;    // never warn below this, even if baseline is tiny
const float VIB_ABS_STOP_MG = 800.0;   // always stop above this
// Tilt from installed position, degrees
const float TILT_WARN_DEG   = 10.0;
const float TILT_STOP_DEG   = 25.0;
const float TILT_HYST_DEG   = 2.0;
// Motor current, mA. Limits = learned baseline x factor, capped
const float CUR_WARN_FACTOR = 1.6;
const float CUR_STOP_FACTOR = 2.5;
const float CUR_FLOOR_MA    = 150.0;
const float CUR_ABS_STOP_MA = 1500.0;  // protects the L298N (2 A max)
const float BUS_MIN_V       = 8.0;     // below this while running = motor has no power
// Stall / jam: at a steady speed a turning motor always vibrates. If the vibration
// collapses below 35% of that speed's healthy baseline, the shaft has stopped.
const float STALL_FACTOR      = 0.35;
const float STALL_MIN_BASE_MG = 15.0;  // only check if the healthy motor clearly vibrates
// Motor speed. You set it in % from the laptop; the code turns % into PWM 0..255:
//   speed %  ->  PWM = PWM_MIN + (255 - PWM_MIN) x % / 100     (100 % = 255)
// (a geared motor only hums below ~25 % duty, so the % scale starts where it turns)
const uint8_t PWM_MIN        = 70;     // raise to 80-90 if your motor won't turn at 10 %
const uint8_t SPEED_DEFAULT  = 100;    // speed % after power-up
const uint8_t SPEED_MIN_PCT  = 10;
const uint8_t WARN_SPEED_PCT = 45;     // WARNING slows the motor to this (or keeps it lower)
const uint8_t RAMP_STEP      = 5;      // soft start: +5 PWM every 10 ms = 0.5 s to full speed

// ================================ 4. TIMING ================================
const uint16_t SAMPLE_US        = 10000;  // MPU6050 read every 10 ms = 100 Hz
const uint8_t  VIB_WINDOW       = 50;     // 50 samples = one vibration result per 0.5 s
const uint16_t PING_MS          = 70;     // HC-SR04 needs > 60 ms between pings
const uint16_t ECHO_TIMEOUT_US  = 10000;  // ~1.6 m max, so a ping never blocks long
const uint16_t TEMP_MS          = 2100;   // DHT22 can only measure once every 2 s
const uint8_t  TREND_N          = 15;     // temperature trend over the last ~30 s
const uint16_t INA_MS           = 100;
const uint16_t LOGIC_MS         = 100;    // decide the state 10x per second
const uint16_t LOG_MS           = 500;    // dashboard data row (d mode): 2x per second
const uint16_t TEXT_MS          = 2000;   // readable report in the Serial Monitor: every 2 s
const uint16_t LCD_MS           = 250;
const uint16_t LCD_PAGE_MS      = 2000;
const uint16_t DEBOUNCE_MS      = 25;
const uint16_t RELAY_SETTLE_MS  = 150;    // let relay contacts close before driving the motor
const uint16_t SPINUP_MS        = 2000;   // ignore start-up current/vibration
const uint8_t  LEARN_WINDOWS    = 8;      // 8 x 0.5 s = 4 s of baseline learning
const uint16_t WARN_DWELL_MS    = 3000;   // stay in WARNING until clear for 3 s
const uint8_t  VIB_STOP_WINDOWS = 2;      // vibration must be too high for 1 s to trip
const uint8_t  CUR_STOP_READS   = 3;      // current must be too high for 0.3 s to trip
const uint8_t  STALL_WINDOWS    = 3;      // vibration must stay collapsed for 1.5 s
const float    ACC_LSB_PER_G    = 8192.0; // MPU6050 at +/-4 g
