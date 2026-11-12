// util.cpp - small helpers and race-free outputs
//
// Speed <-> PWM conversion, relay / buzzer / motor writes that are safe against
// the E-stop interrupt, median filter and the warning / stop level helpers.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ========================= 7. SMALL HELPERS ================================
void pgmText(const char* const* table, uint8_t i, char* out, uint8_t len) {
  strncpy_P(out, (PGM_P)pgm_read_word(&table[i]), len - 1);
  out[len - 1] = 0;
}

void printPgm(const char* const* table, uint8_t i) {
  char buf[20];
  pgmText(table, i, buf, sizeof buf);
  LOG.print(buf);
}

bool isRunning() { return state == ST_RUN || state == ST_WARN; }

// After a start or a speed change the motor needs a moment to settle
bool motorChecksActive(uint32_t now) {
  return isRunning() && (now - runStartMs >= SPINUP_MS) && (now - speedChangeMs >= SPINUP_MS);
}

uint8_t pctToPwm(uint8_t pct) { return PWM_MIN + (uint16_t)(255 - PWM_MIN) * pct / 100; }
uint8_t pwmToPct(uint8_t pwm) {
  if (pwm <= PWM_MIN) return 0;
  return ((uint16_t)(pwm - PWM_MIN) * 100 + (255 - PWM_MIN) / 2) / (255 - PWM_MIN);
}
uint8_t runPwm()  { return pctToPwm(speedPct); }
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

void setBuzzer(bool on) {
  digitalWrite(PIN_BUZZER, (on != BUZZER_ACTIVE_LOW) ? HIGH : LOW);
}

void motorStopNow() {
  noInterrupts();          // these writes share a timer register with the interrupt's
  pwmNow = 0;
  motorPwm(0);                    // ENA low = both motor outputs off
  interrupts();
}

void motorDrive(uint8_t pwm) {
  noInterrupts();
  if (!estopIrq) {         // never switch the motor on over a fresh E-stop
    motorPwm(pwm);                     // direction is fixed by IN1/IN2 wiring
  }
  interrupts();
}

void beep(uint16_t ms) { beepUntil = millis() + ms; }

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

uint8_t levelLow(float v, float warn, float stop, float hyst, uint8_t prev) {
  if (v <= stop) return 2;
  if (v <= warn) return 1;
  if (prev >= 1 && v < warn + hyst) return 1;
  return 0;
}
