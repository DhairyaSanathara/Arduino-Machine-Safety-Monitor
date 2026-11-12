// safety.cpp - the safety decision and the state machine
//
// evaluate() grades every check (0 OK, 1 warning, 2 stop); runStateMachine()
// moves between IDLE, RUN, WARNING, ESTOP and FAULT with a first-out reason.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================ 14. DECISIONS ================================
void evaluate(uint32_t now) {
  lvl[R_ESTOP_BTN] = btnStop.stable ? 2 : 0;
  lvl[R_SENS_TEMP] = tempFail ? 2 : 0;
  lvl[R_SENS_MPU]  = mpuFail ? 2 : 0;
  lvl[R_SENS_INA]  = (inaOk && inaFail) ? 2 : 0;

  lvl[R_PROX] = levelLow(distCm, DIST_WARN_CM, DIST_STOP_CM, DIST_HYST_CM, lvl[R_PROX]);

  if (!tempFail && !isnan(tempC)) {
    lvl[R_TEMP]  = levelHigh(tempC, TEMP_WARN_C, TEMP_STOP_C, TEMP_HYST_C, lvl[R_TEMP]);
    lvl[R_TRATE] = tempRateValid
                   ? levelHigh(tempRate, TRATE_WARN, TRATE_STOP, TRATE_HYST, lvl[R_TRATE]) : 0;
  } else {
    lvl[R_TEMP] = lvl[R_TRATE] = 0;
  }

  lvl[R_TILT] = (!mpuFail && tiltRefSet)
                ? levelHigh(tiltDeg, TILT_WARN_DEG, TILT_STOP_DEG, TILT_HYST_DEG, lvl[R_TILT]) : 0;
  // R_VIB, R_STALL, R_CURRENT, R_NOPOWER and R_RELAY are set by their own sensor tasks.

  worstLevel = 0;
  worstReason = R_NONE;
  for (uint8_t r = 1; r < R_COUNT; r++) {
    if (lvl[r] > worstLevel) { worstLevel = lvl[r]; worstReason = r; }
  }
  if (worstLevel >= 1) lastWarnMs = now;
}

void changeState(uint8_t s, uint8_t reason) {
  state = s;
  logEventStart(textMode ? F("MACHINE IS NOW ") : F("STATE,"));
  printPgm(STATE_NAME, s);
  LOG.print(textMode ? F(" - ") : F(","));
  printPgm(textMode ? REASON_TEXT : REASON_CODE, reason);
  LOG.println();
  lcdPage = 0;
  tLcdPage = millis();
}

void clearMotorChecks() {
  lvl[R_VIB] = lvl[R_CURRENT] = lvl[R_NOPOWER] = lvl[R_STALL] = 0;
  vibStopCount = curStopCount = powerBadCount = stallCount = 0;
}

void enterIdle() {
  motorStopNow();
  setRelay(false);
  clearMotorChecks();
  latchedReason = R_NONE;
  changeState(ST_IDLE, R_NONE);
}

void startLearning() {
  baselineReady = false;
  learnVibSum = learnCurSum = 0;
  learnVibN = 0;
  learnCurN = 0;
  updateLimits();
}

// One baseline per speed: each speed is learned once (the first time it runs
// healthy after power-up) and then remembered.
void selectBaseline() {
  uint8_t b = speedPct / 10;
  if (baseKnown & (1u << b)) {
    vibBase = baseVib[b];
    curBase = baseCur[b];
    baselineReady = true;
    updateLimits();
  } else {
    startLearning();
  }
}

void enterRun() {
  selectBaseline();            // known speed: keep its baseline. New speed: learn it
  clearMotorChecks();
  runStartMs = millis();
  setRelay(true);              // power first, motor after RELAY_SETTLE_MS
  changeState(ST_RUN, R_NONE);
  beep(80);
}

void enterEstop(uint8_t reason) {
  motorStopNow();              // 1. driver off
  setRelay(false);             // 2. power physically cut
  clearMotorChecks();
  latchedReason = reason;      // 3. remember the FIRST cause ("first-out")
  if (estopCount < 9999) estopCount++;
  changeState(ST_ESTOP, reason);
}

void enterFault(uint8_t reason) {
  motorStopNow();
  setRelay(false);
  clearMotorChecks();
  latchedReason = reason;
  changeState(ST_FAULT, reason);
}

void runStateMachine(uint32_t now) {
  bool go = goRequest;
  goRequest = false;

  if (lvl[R_RELAY] == 2 && state != ST_FAULT) { enterFault(R_RELAY); return; }

  switch (state) {
    case ST_IDLE:
      if (lvl[R_ESTOP_BTN] == 2) { enterEstop(R_ESTOP_BTN); break; }
      if (go) {
        if (worstLevel < 2) enterRun();
        else beep(400);                         // start refused: danger present
      }
      break;

    case ST_RUN:
    case ST_WARN:
      if (worstLevel == 2) { enterEstop(worstReason); break; }
      if (worstLevel == 1) {
        if (state != ST_WARN) changeState(ST_WARN, worstReason);
      } else if (state == ST_WARN && now - lastWarnMs >= WARN_DWELL_MS) {
        changeState(ST_RUN, R_NONE);            // clear for 3 s -> back to full speed
      }
      break;

    case ST_ESTOP:
      if (go) {
        if (worstLevel < 2) enterIdle();        // unlocked, but NOT restarted
        else beep(400);
      }
      break;

    case ST_FAULT:
      if (go) {
        selfTest();
        if (mpuOk && tempOk && lvl[R_RELAY] == 0) enterIdle();
        else beep(400);
      }
      break;
  }
}
