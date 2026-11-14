// motor.cpp - soft-start motor ramp and the E-stop interrupt
//
// The state machine only says WHAT speed is wanted; taskMotor() moves the PWM
// there gently, and the E-stop interrupt can cut it at any instant.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================== 15. MOTOR ==================================
// Soft start: every 10 ms move the PWM at most RAMP_STEP (5) closer to the target,
// so 0 -> 255 takes about 0.5 s. A sudden full-power start would draw a big current
// spike and shake the machine (which the vibration check could mistake for a fault).
void taskMotor(uint32_t now) {
  if (now - tMotor < 10) return;                     // not time yet
  tMotor = now;
  if (!isRunning()) { motorStopNow(); return; }      // IDLE / ESTOP / FAULT: always off
  uint8_t target = 0;
  // Wait RELAY_SETTLE_MS (150 ms) after GO so the relay contacts are fully closed
  // before current flows (switching under load would spark and wear the contacts).
  if (now - runStartMs >= RELAY_SETTLE_MS) target = (state == ST_WARN) ? warnPwm() : runPwm();
  if (pwmNow < target)      pwmNow = (target - pwmNow > RAMP_STEP) ? pwmNow + RAMP_STEP : target;   // ramp up
  else if (pwmNow > target) pwmNow = (pwmNow - target > RAMP_STEP) ? pwmNow - RAMP_STEP : target;   // ramp down
  motorDrive(pwmNow);                                // util.cpp: refuses if an E-stop just fired
}

// ===================== 15b. E-STOP INTERRUPT ===============================
// D3 is one of the Uno's two hardware-interrupt pins. The moment the E-stop
// contact closes, the chip pauses whatever it is doing (an LCD update, a sensor
// read...) and runs this function within a few microseconds. It cuts the motor
// right there; loop() then does the bookkeeping (state, LCD, log) on its next pass.
// Without it, the button is only looked at once per loop pass, so a very quick
// tap during a slow pass (~50 ms worst case) could be missed.
// Rules for an interrupt function: keep it tiny, and never use Serial, Wire,
// delay() or millis()-waits inside it - they need interrupts themselves.
void onEstopInterrupt() {
  // Read the pin again: pressed = LOW for our normally-open button (flipped for NC)
  bool pressed = (digitalRead(PIN_BTN_ESTOP) == LOW) != ESTOP_BUTTON_IS_NC;
  if (!pressed) return;                    // electrical spike, already gone - ignore
  estopIrq = true;                         // tell loop() it happened (it sets ST_ESTOP)
  motorPwm(0);                             // 1. driver off
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);   // 2. power cut
}
