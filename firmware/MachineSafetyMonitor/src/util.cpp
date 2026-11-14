// util.cpp - small helpers and race-free outputs
//
// Speed <-> PWM conversion, relay / buzzer / motor writes that are safe against
// the E-stop interrupt, median filter and the warning / stop level helpers.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ========================= 7. SMALL HELPERS ================================
// Copy text number i of a flash table (STATE_NAME, REASON_TEXT...) into RAM.
// Text in flash cannot be used directly like a normal string, so it is copied
// into a small buffer first. The last byte is always 0 (end of string).
void pgmText(const char* const* table, uint8_t i, char* out, uint8_t len) {
  strncpy_P(out, (PGM_P)pgm_read_word(&table[i]), len - 1);
  out[len - 1] = 0;
}

// Print text number i of a flash table.
void printPgm(const char* const* table, uint8_t i) {
  char buf[20];
  pgmText(table, i, buf, sizeof buf);
  LOG.print(buf);
}

// The motor is turning in RUN and in WARNING (at reduced speed).
bool isRunning() { return state == ST_RUN || state == ST_WARN; }

// After a start or a speed change the motor needs a moment to settle
bool motorChecksActive(uint32_t now) {
  return isRunning() && (now - runStartMs >= SPINUP_MS) && (now - speedChangeMs >= SPINUP_MS);
}

// Speed % -> PWM duty (0..255). The scale starts at PWM_MIN because a geared
// motor only hums below it. Example with PWM_MIN 70: 50 % -> 70 + 185 x 0.5 = 162.
// (uint16_t: 185 x 100 does not fit in 8 bits, so the multiplication is done in 16.)
uint8_t pctToPwm(uint8_t pct) { return PWM_MIN + (uint16_t)(255 - PWM_MIN) * pct / 100; }
// PWM -> speed %, the reverse of pctToPwm, rounded to the nearest %.
uint8_t pwmToPct(uint8_t pwm) {
  if (pwm <= PWM_MIN) return 0;
  return ((uint16_t)(pwm - PWM_MIN) * 100 + (255 - PWM_MIN) / 2) / (255 - PWM_MIN);
}
uint8_t runPwm()  { return pctToPwm(speedPct); }   // PWM for RUN: the set speed
// PWM for WARNING: 45 %, or the set speed if that is already slower
uint8_t warnPwm() { return pctToPwm(speedPct < WARN_SPEED_PCT ? speedPct : WARN_SPEED_PCT); }

// Speed signal to the L298N ENA (on both D5 and D6, see the pin map)
void motorPwm(uint8_t v) {
  analogWrite(PIN_MOTOR_EN, v);
#ifdef PIN_MOTOR_EN2
  analogWrite(PIN_MOTOR_EN2, v);
#endif
}

// --- Race-free outputs (see section 15b) ---------------------------------
// The E-stop interrupt can switch the motor off between ANY two instructions.
// If the main code checked "is the E-stop flag set?" and then switched the motor
// on, an interrupt landing between the check and the switch-on would be undone.
// So "check the flag + switch on" runs with interrupts paused: one unbreakable step.
// (It takes a few microseconds; a pending interrupt runs right after.)
void setRelay(bool on) {
  noInterrupts();
  if (on && estopIrq) on = false;           // an E-stop just happened: stay off
  relayOn = on;
  digitalWrite(PIN_RELAY, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
  interrupts();
  relayChangeMs = millis();
}

// Some buzzer modules sound on LOW: "on != BUZZER_ACTIVE_LOW" flips the level for them.
void setBuzzer(bool on) {
  digitalWrite(PIN_BUZZER, (on != BUZZER_ACTIVE_LOW) ? HIGH : LOW);
}

// Motor off immediately (no ramp).
void motorStopNow() {
  noInterrupts();          // these writes share a timer register with the interrupt's
  pwmNow = 0;
  motorPwm(0);                    // ENA low = both motor outputs off
  interrupts();
}

// Send a speed to the motor, unless an E-stop has just happened.
void motorDrive(uint8_t pwm) {
  noInterrupts();
  if (!estopIrq) {         // never switch the motor on over a fresh E-stop
    motorPwm(pwm);                     // direction is fixed by IN1/IN2 wiring
  }
  interrupts();
}

// Ask for a beep of ms milliseconds; taskIndicators() switches the buzzer off later
// (no delay(): the loop keeps running while the buzzer sounds).
void beep(uint16_t ms) { beepUntil = millis() + ms; }

// Middle value of three. One wild ultrasonic reading (an echo off something else)
// is thrown away, while a real change shows up after two pings.
float median3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }   // now a <= b
  if (b > c) b = c;                          // b = min(b, c)
  return (a > b) ? a : b;                    // max(a, min(b, c))
}

// 0 = OK, 1 = WARNING, 2 = STOP. "High" = bigger value is worse.
// Hysteresis: once in WARNING, the value must fall clearly below the limit to clear.
uint8_t levelHigh(float v, float warn, float stop, float hyst, uint8_t prev) {
  if (v >= stop) return 2;
  if (v >= warn) return 1;
  if (prev >= 1 && v > warn - hyst) return 1;
  return 0;
}

// The same for values where SMALLER is worse (distance to a person).
uint8_t levelLow(float v, float warn, float stop, float hyst, uint8_t prev) {
  if (v <= stop) return 2;
  if (v <= warn) return 1;
  if (prev >= 1 && v < warn + hyst) return 1;
  return 0;
}
