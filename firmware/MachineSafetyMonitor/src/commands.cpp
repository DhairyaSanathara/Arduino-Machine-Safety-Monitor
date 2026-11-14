// commands.cpp - buttons and serial commands
//
// Debounced GO / E-STOP buttons, help text, speed set point and the command parser.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================ 16. INPUTS ===================================
// Debounce: a mechanical contact "bounces" (opens and closes many times) for a few
// milliseconds when pressed. We only accept a new state once the raw reading has
// stayed the same for DEBOUNCE_MS (25 ms). Without this, one press could count twice.
void updateButton(Button &b, uint32_t now) {
  bool raw = (digitalRead(b.pin) == LOW);      // INPUT_PULLUP: pressed connects the pin to GND
  if (b.nc) raw = !raw;                       // NC contact: pressed = opens = HIGH
  if (raw != b.last) {                          // the contact moved: restart the 25 ms timer
    b.last = raw;
    b.changedMs = now;
  } else if (raw != b.stable && now - b.changedMs >= DEBOUNCE_MS) {   // steady for 25 ms
    b.stable = raw;
    if (raw) b.pressedEvent = true;            // report the press once
  }
}

// '?' : list the commands. Lines starting with # are notes for people.
void printHelp() {
  rowFinish();
  LOG.println(F("# Commands: g = GO (start/reset)  x = emergency stop  z = re-zero tilt"));
  LOG.println(F("#           10..100 + Enter = speed %   + / - = speed +-10 %"));
  LOG.println(F("#           s = status   l = re-learn baselines   ? = help"));
  LOG.println(F("# Output: n = readable text (every 2 s)  d = data for the dashboard"));
  LOG.println(F("# Graphs (Tools -> Serial Plotter): v = vibration  t = temperature  b = both"));
}

// Speed in steps of 10 %, so each speed has one learned baseline
void setSpeed(int16_t pct) {
  if (pct < SPEED_MIN_PCT || pct > 100) {
    rowFinish();
    LOG.println(F("# speed must be 10..100 (%). To stop: x, or the E-STOP button"));
    return;
  }
  uint8_t p = (pct + 5) / 10 * 10;           // round to the nearest 10: 64 -> 60, 65 -> 70
  if (p > 100) p = 100;
  if (p != speedPct) {
    speedPct = p;
    speedChangeMs = millis();                // vibration checks pause while it settles
    selectBaseline();                        // switch to that speed's learned limits
  }
  logEventStart(F("SPEED"));
  LOG.print(',');
  LOG.println(speedPct);
}

// Read every character the laptop has sent (the website's server, or a person
// typing in the Serial Monitor). Single letters are commands; digits build a number.
void taskSerialIn() {
  while (Serial.available() > 0) {             // bytes waiting in the receive buffer
    char c = Serial.read();
    if (c >= '0' && c <= '9') {                // digits = a new speed in %
      numIn = (numIn < 0 ? 0 : numIn) * 10 + (c - '0');   // "6" then "0" -> 6 -> 60
                                                         // (c - '0' turns the character into its value)
      if (numIn > 999) numIn = 999;
      numInMs = millis();
      continue;
    }
    if (numIn >= 0) { setSpeed(numIn); numIn = -1; }   // Enter ends the number
    switch (c) {
      case '+':
        setSpeed(speedPct >= 90 ? 100 : speedPct + 10);
        break;
      case '-':
        setSpeed(speedPct <= SPEED_MIN_PCT + 10 ? SPEED_MIN_PCT : speedPct - 10);
        break;
      case 'g': case 'G':
        goRequest = true;
        break;
      case 'x': case 'X':                      // stop from the laptop / website
        if (state != ST_ESTOP && state != ST_FAULT) enterEstop(R_REMOTE);
        break;
      case 'l': case 'L':
        baseKnown = 0;                         // forget every speed's baseline
        startLearning();
        logEventStart(F("RELEARN"));
        LOG.println();
        break;
      case 'z': case 'Z':
        rezeroTilt = true;
        logEventStart(F("TILT_REZERO"));
        LOG.println();
        break;
      case 's': case 'S':
        printStatus();
        break;
      case 'v': case 'V': setPlotMode(1); break;   // graphs for the Serial Plotter
      case 't': case 'T': setPlotMode(2); break;
      case 'b': case 'B': setPlotMode(3); break;
      case 'n': case 'N': setPlotMode(0); break;   // readable text
      case 'd': case 'D': setPlotMode(4); break;   // CSV data for the dashboard
      case '?':
        printHelp();
        break;
    }
  }
  // Serial Monitor set to "No line ending": take the number after a short pause
  if (numIn >= 0 && millis() - numInMs > 500) { setSpeed(numIn); numIn = -1; }
}
