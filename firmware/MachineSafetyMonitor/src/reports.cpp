// reports.cpp - text the Serial Monitor and Serial Plotter show
//
// Readable report every 2 s (n), Serial Plotter graphs (v / t / b), status (s).
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// --- Serial Plotter graphs ----------------------------------------------------
// The Arduino IDE's Serial Plotter draws every "Name:value" pair on a line as one
// coloured line. Click a name in the legend to hide / show that line.
// m: 1/2/3 = a graph, 0 = readable text, 4 = dashboard data
void setPlotMode(uint8_t m) {
  rowFinish();
  if (m >= 1 && m <= 3) {
    logOut = &nullOut;                       // text off: the plotter only gets numbers
    plotMode = m;
    return;
  }
  plotMode = 0;
  logOut = &Serial;                          // text output back on
  textMode = (m == 0);
  if (textMode) LOG.println(F("# readable text, every 2 s  (d = data for the dashboard)"));
  else printDataHeader();
}

// --- The readable report for the Serial Monitor (every TEXT_MS) ---------------
// Built in memory and sent bit by bit, like the data row, so the loop never waits.
char* rb;
void bTxt(const __FlashStringHelper* t) { strcpy_P(rb, (PGM_P)t); rb += strlen(rb); }
void bNum(float v, uint8_t dec) {
  if (isnan(v)) { *rb++ = '-'; *rb++ = '-'; *rb = 0; return; }
  dtostrf(v, 1, dec, rb); rb += strlen(rb);
}
void bNl() { *rb++ = '\r'; *rb++ = '\n'; }

void textReport() {
  uint8_t reason = (state == ST_ESTOP || state == ST_FAULT) ? latchedReason : worstReason;
  rb = rowBuf;
  bTxt(F("--- ")); pgmText(STATE_NAME, state, rb, 12); rb += strlen(rb);
  bTxt(F(" | speed ")); bNum(pwmToPct(pwmNow), 0);
  bTxt(F("% (set ")); bNum(speedPct, 0);
  bTxt(F("%) | ")); pgmText(REASON_TEXT, reason, rb, 17); rb += strlen(rb); bNl();
  bTxt(F("Temperature : ")); bNum(tempC, 1); bTxt(F(" C (warn ")); bNum(TEMP_WARN_C, 0);
  bTxt(F(" / stop ")); bNum(TEMP_STOP_C, 0); bTxt(F("), rising "));
  if (tempRateValid) bNum(tempRate, 2); else bTxt(F("--"));
  bTxt(F(" C/min")); bNl();
  bTxt(F("Vibration   : ")); bNum(vibMg, 1); bTxt(F(" mg "));
  if (baselineReady) { bTxt(F("(warn ")); bNum(vibWarn, 0); bTxt(F(" / stop ")); bNum(vibStop, 0); bTxt(F(")")); }
  else bTxt(F("(limits learned when running)"));
  bNl();
  bTxt(F("Distance    : "));
  if (distCm >= DIST_MAX_CM) bTxt(F("clear")); else { bNum(distCm, 0); bTxt(F(" cm")); }
  bTxt(F("   Tilt: ")); bNum(tiltDeg, 1); bTxt(F(" deg   Humidity: ")); bNum(humidity, 0); bTxt(F(" %"));
  bNl();
  rowLen = rb - rowBuf;
  rowPos = 0;
  rowPump();
}

void plotPair(const __FlashStringHelper* name, float v, uint8_t dec, bool last) {
  Serial.print(name);
  Serial.print(':');
  Serial.print(isnan(v) ? 0.0 : v, dec);
  if (!last) Serial.print(',');
  else Serial.println();
}

void taskPlot(uint32_t now) {
  if (!plotMode) return;
  if (now - tPlot < (plotMode == 2 ? 1000 : 250)) return;   // temperature changes slowly
  tPlot = now;
  float vw = baselineReady ? vibWarn : 0;    // limits only once they are learned
  float vs = baselineReady ? vibStop : 0;
  if (plotMode == 1) {                       // VIBRATION graph
    plotPair(F("Vibration_mg"), vibMg, 1, false);
    plotPair(F("Warn_mg"), vw, 0, false);
    plotPair(F("Stop_mg"), vs, 0, false);
    plotPair(F("Speed_pct"), pwmToPct(pwmNow), 0, true);
  } else if (plotMode == 2) {                // TEMPERATURE graph
    plotPair(F("Temp_C"), tempC, 1, false);
    plotPair(F("Warn_C"), TEMP_WARN_C, 0, false);
    plotPair(F("Stop_C"), TEMP_STOP_C, 0, false);
    plotPair(F("Rise_C_per_min"), tempRateValid ? tempRate : 0, 2, true);
  } else {                                   // BOTH on one chart (similar ranges)
    plotPair(F("Vibration_mg"), vibMg, 1, false);
    plotPair(F("Vib_stop_mg"), vs, 0, false);
    plotPair(F("Temp_C"), tempC, 1, false);
    plotPair(F("Temp_stop_C"), TEMP_STOP_C, 0, true);
  }
}

// 's': everything at a glance, in plain words (handy for the demo / viva)
void printLimitPair(float w, float st, uint8_t dec) {
  LOG.print(F("  (warn ")); LOG.print(w, dec);
  LOG.print(F(" / stop ")); LOG.print(st, dec); LOG.println(')');
}

void printStatus() {
  rowFinish();
  uint8_t reason = (state == ST_ESTOP || state == ST_FAULT) ? latchedReason : worstReason;
  char buf[20];
  LOG.println(F("# ------------------------- STATUS -------------------------"));
  LOG.print(F("# State      : ")); printPgm(STATE_NAME, state);
  pgmText(REASON_TEXT, reason, buf, sizeof buf);
  LOG.print(F(" - ")); LOG.println(buf);
  LOG.print(F("# Speed      : set ")); LOG.print(speedPct);
  LOG.print(F(" %, now ")); LOG.print(pwmToPct(pwmNow));
  LOG.print(F(" % (PWM ")); LOG.print(pwmNow); LOG.print(F(")"));
  if (isRunning()) {
    uint32_t sec = (millis() - runStartMs) / 1000;
    LOG.print(F(", running for ")); LOG.print(sec / 60);
    LOG.print(F(" min ")); LOG.print(sec % 60); LOG.print(F(" s"));
  }
  LOG.println();
  LOG.print(F("# Distance   : "));
  if (distCm >= DIST_MAX_CM) LOG.print(F("nothing near")); else { LOG.print(distCm, 0); LOG.print(F(" cm")); }
  LOG.print(F("  (warn < ")); LOG.print(DIST_WARN_CM, 0);
  LOG.print(F(" / stop < ")); LOG.print(DIST_STOP_CM, 0); LOG.println(F(" cm)"));
  LOG.print(F("# Temperature: ")); LOG.print(tempC, 1); LOG.print(F(" C"));
  printLimitPair(TEMP_WARN_C, TEMP_STOP_C, 0);
  LOG.print(F("# Rise rate  : "));
  if (tempRateValid) { LOG.print(tempRate, 2); LOG.print(F(" C/min")); }
  else LOG.print(F("measuring (needs ~30 s of readings)"));
  printLimitPair(TRATE_WARN, TRATE_STOP, 0);
  LOG.print(F("# Humidity   : ")); LOG.print(humidity, 0); LOG.println(F(" %"));
  LOG.print(F("# Vibration  : ")); LOG.print(vibMg, 1); LOG.print(F(" mg"));
  if (baselineReady) { LOG.print(F(", healthy ")); LOG.print(vibBase, 1); }
  else LOG.print(F(", baseline not learned yet"));
  printLimitPair(vibWarn, vibStop, 0);
  LOG.print(F("# Tilt       : ")); LOG.print(tiltDeg, 1); LOG.print(F(" deg"));
  printLimitPair(TILT_WARN_DEG, TILT_STOP_DEG, 0);
  if (inaOk) {
    LOG.print(F("# Motor      : ")); LOG.print(currentMa, 0); LOG.print(F(" mA, "));
    LOG.print(busV, 1); LOG.print(F(" V, ")); LOG.print(powerW, 1); LOG.println(F(" W"));
  }
  LOG.print(F("# Learned at : "));
  bool any = false;
  for (uint8_t b = 1; b <= 10; b++) {
    if (baseKnown & (1u << b)) { LOG.print(b * 10); LOG.print(F("% ")); any = true; }
  }
  LOG.println(any ? F("") : F("no speed yet"));
  LOG.print(F("# Sensors    : MPU6050 ")); LOG.print(mpuOk && !mpuFail ? F("OK") : F("ERROR"));
  LOG.print(F(", DHT")); LOG.print(dhtModel ? dhtModel : 22);
  LOG.print(tempOk && !tempFail ? F(" OK") : F(" ERROR"));
  LOG.print(F(", LCD ")); LOG.print(!lcd ? F("not found") : (lcdBusOk ? F("OK") : F("bus jammed")));
  LOG.print(F(", INA219 ")); LOG.println(inaOk ? F("OK") : F("not fitted"));
  LOG.print(F("# E-stops    : ")); LOG.print(estopCount);
  LOG.print(F(" since power-up, up ")); LOG.print(millis() / 60000UL); LOG.println(F(" min"));
  LOG.println(F("# ----------------------------------------------------------"));
}
