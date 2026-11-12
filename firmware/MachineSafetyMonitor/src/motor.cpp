// motor.cpp - soft-start motor ramp and the E-stop interrupt
//
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================== 15. MOTOR ==================================
void taskMotor(uint32_t now) {
  if (now - tMotor < 10) return;
  tMotor = now;
  if (!isRunning()) { motorStopNow(); return; }
  uint8_t target = 0;
  if (now - runStartMs >= RELAY_SETTLE_MS) target = (state == ST_WARN) ? warnPwm() : runPwm();
  if (pwmNow < target)      pwmNow = (target - pwmNow > RAMP_STEP) ? pwmNow + RAMP_STEP : target;
  else if (pwmNow > target) pwmNow = (pwmNow - target > RAMP_STEP) ? pwmNow - RAMP_STEP : target;
  motorDrive(pwmNow);
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
  bool pressed = (digitalRead(PIN_BTN_ESTOP) == LOW) != ESTOP_BUTTON_IS_NC;
  if (!pressed) return;                    // electrical spike, already gone - ignore
  estopIrq = true;
  motorPwm(0);                             // 1. driver off
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);   // 2. power cut
}
