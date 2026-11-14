// lcd.cpp - self-healing 16x2 I2C LCD
//
// Only changed characters are written, a few per loop pass, and the controller
// is re-synchronised every 10 s so a lost half-byte cannot leave garbage.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// The screen is updated in small pieces. Each character costs ~1.2 ms on the
// I2C bus, so rewriting both lines at once would hold up the loop for ~40 ms.
// lcdLine() only says WHAT we want; lcdFlush() runs every loop pass and writes
// at most a few changed characters (~7 ms), so no pass is ever held up for long.
// A full screen change still completes within a few tens of milliseconds.

// Say what one row (0 = top, 1 = bottom) should show. Nothing is sent to the LCD here.
void lcdLine(uint8_t row, const char* text) {
  uint8_t i = 0;
  for (; i < 16 && text[i]; i++) lcdWant[row][i] = text[i];
  for (; i < 16; i++) lcdWant[row][i] = ' ';   // pad: no lcd.clear(), no flicker
  lcdWant[row][16] = 0;
}

// --- Self-healing screen ---------------------------------------------------
// A loose wire or an unpowered module on A4/A5 can jam the I2C bus. Then every
// message waits 25 ms for its time-out, and a screen update would freeze the loop
// for ~1 s. So we first check the LCD still answers; if not, we leave it alone for
// a second. A wire that flickers can also put the LCD out of step (it receives
// half-bytes), which shows as garbage - so every 10 s, and whenever the bus comes
// back, the LCD is put back in step and the whole screen is redrawn.
// The LCD is driven 4 bits ("a nibble") at a time through the PCF8574 backpack.
// A pulse on EN (high then low) tells the LCD to take the 4 bits.
void lcdNibble(uint8_t n) {                 // backpack: P0=RS P1=RW P2=EN P3=light P4-7=data
  uint8_t v = (n << 4) | 0x08;              // data on P4-P7, backlight bit (P3) kept on
  Wire.beginTransmission(lcdAddr); Wire.write(v | 0x04); Wire.endTransmission();  // EN high
  Wire.beginTransmission(lcdAddr); Wire.write(v);        Wire.endTransmission();  // EN low
}

void lcdResync() {                          // the datasheet's "reset by instruction"
  lcdNibble(0x3); delay(5);
  lcdNibble(0x3); delay(1);
  lcdNibble(0x3); delay(1);
  lcdNibble(0x2); delay(1);                 // now 4-bit mode, in step, from ANY state
  lcd->command(0x28);                       // 4-bit, 2 lines
  lcd->command(0x0C);                       // display on, no cursor
  lcd->command(0x06);                       // write left to right
  lcd->home();                              // undo any accidental shift
  lcd->backlight();
  memset(lcdCache, 0, sizeof lcdCache);     // redraw everything
}

// Write up to "budget" changes to the LCD (a cursor jump or one character each).
// Returns true if something is still left to write
bool lcdFlush(uint8_t budget) {
  if (!lcd) return false;                   // no LCD fitted
  uint32_t now = millis();
  bool due = (now - lcdResyncMs >= 10000);  // time for the 10 s re-sync?
  // Compare "wanted" with "on screen" (memcmp = compare memory, 0 = identical)
  bool changed = (memcmp(lcdWant[0], lcdCache[0], 16) != 0) || (memcmp(lcdWant[1], lcdCache[1], 16) != 0);
  if (!changed && !due) return false;
  if (!lcdBusOk && now - lcdRetryMs < 1000) return false;   // jammed: try again in 1 s
  if (changed || due) {
    if (!i2cPresent(lcdAddr)) {             // LCD not answering: report once, back off
      if (lcdBusOk) { logEventStart(F("I2C_BUS_ERROR")); LOG.println(); }
      lcdBusOk = false;
      lcdRetryMs = now;
      return false;
    }
    if (!lcdBusOk || due) {
      if (!lcdBusOk) { logEventStart(F("I2C_BUS_OK")); LOG.println(); }
      lcdBusOk = true;
      lcdResync();
      lcdResyncMs = now;
    }
  }
  // Walk both rows; only characters that differ are written. A run of changed
  // characters needs one cursor jump, then the LCD moves right by itself.
  for (uint8_t row = 0; row < 2; row++) {
    uint8_t i = 0;
    while (i < 16) {
      if (lcdWant[row][i] == lcdCache[row][i]) { i++; continue; }   // already right
      if (budget < 2) return true;          // out of time for this pass
      lcd->setCursor(i, row);
      budget--;
      while (i < 16 && budget && lcdWant[row][i] != lcdCache[row][i]) {
        lcd->write(lcdWant[row][i]);
        lcdCache[row][i] = lcdWant[row][i];
        i++;
        budget--;
      }
    }
  }
  return false;
}

// Build the bottom line for "page" p (the pages rotate every 2 s):
//   0  D:120cm T:30.0C        distance + temperature
//   1  V:36mg Tilt:0(deg)     vibration + tilt
//   2  RH:52% +0.3C/m         humidity + temperature rise rate
//   3  Set:70% 01:23          set speed + run time  (or Set:70% Stop:3 when stopped)
// snprintf_P = printf into a buffer, with the format text kept in flash (PSTR).
void valuePage(uint8_t p, char* out) {
  char a[8], b[8];
  if (p == 0) {                                // distance + temperature
    if (distCm >= DIST_MAX_CM) strcpy(a, "--"); else itoa((int)(distCm + 0.5), a, 10);
    if (isnan(tempC)) strcpy(b, "--.-"); else dtostrf(tempC, 1, 1, b);
    snprintf_P(out, 17, PSTR("D:%scm T:%sC"), a, b);
  } else if (p == 1) {                         // vibration + tilt
    snprintf_P(out, 17, PSTR("V:%dmg Tilt:%d%c"), (int)(vibMg + 0.5), (int)(tiltDeg + 0.5), (char)0xDF);
  } else if (p == 2) {                         // power, or humidity + temperature rise rate
    if (inaOk) {
      dtostrf(powerW, 1, 1, b);
      snprintf_P(out, 17, PSTR("I:%dmA P:%sW"), (int)(currentMa + 0.5), b);
    } else {
      if (isnan(humidity)) strcpy(a, "--"); else itoa((int)(humidity + 0.5), a, 10);
      if (!tempRateValid) strcpy(b, "--");
      else { b[0] = tempRate < 0 ? '-' : '+'; dtostrf(fabs(tempRate), 1, 1, b + 1); }
      if (senseOk) {
        char v[8];
        dtostrf(busV, 1, 1, v);
        snprintf_P(out, 17, PSTR("Vm:%sV RH:%s%%"), v, a);
      } else {
        snprintf_P(out, 17, PSTR("RH:%s%% %sC/m"), a, b);
      }
    }
  } else {                                     // speed set point + run time / stop count
    if (isRunning()) {
      uint32_t sec = (millis() - runStartMs) / 1000;
      uint16_t mins = sec / 60;
      if (mins > 99) mins = 99;
      snprintf_P(out, 17, PSTR("Set:%u%% %02u:%02u"), speedPct, mins, (uint8_t)(sec % 60));  // run time
    } else {
      snprintf_P(out, 17, PSTR("Set:%u%% Stop:%u"), speedPct, estopCount > 99 ? 99 : estopCount);
    }
  }
}

// 4 times a second: decide the two lines. Top line = the state, bottom line =
// the reason when something is wrong, alternating with the readings.
void taskLCD(uint32_t now) {
  if (!lcd || now - tLcd < LCD_MS) return;
  tLcd = now;
  if (now - tLcdPage >= LCD_PAGE_MS) { tLcdPage = now; lcdPage++; }   // next page every 2 s

  char l0[17], l1[17];
  uint8_t pct = pwmToPct(pwmNow);              // actual speed, same scale as the set point
  switch (state) {
    case ST_IDLE:
      strcpy_P(l0, worstLevel == 2 ? PSTR("NOT READY") : PSTR("READY: press GO"));
      break;
    case ST_RUN:
      if (!baselineReady) strcpy_P(l0, PSTR("RUN  learning.."));
      else snprintf_P(l0, 17, PSTR("RUN SAFE    %3u%%"), pct);
      break;
    case ST_WARN:
      snprintf_P(l0, 17, PSTR("WARNING     %3u%%"), pct);
      break;
    case ST_ESTOP:
      strcpy_P(l0, worstLevel == 2 ? PSTR("!!! E-STOP !!!") : PSTR("E-STOP: press GO"));
      break;
    default:
      strcpy_P(l0, PSTR("SYSTEM FAULT"));
      break;
  }

  // Bottom line: when something is wrong, every other page shows WHY
  uint8_t reason = R_NONE;
  if (state == ST_ESTOP || state == ST_FAULT) reason = latchedReason;
  else if (worstLevel >= 1) reason = worstReason;

  if (reason != R_NONE) {
    uint8_t p = lcdPage % 8;                  // reason, A, reason, B, reason, C, reason, D
    if (p % 2 == 0) pgmText(REASON_TEXT, reason, l1, sizeof l1);
    else valuePage(p / 2, l1);
  } else {
    valuePage(lcdPage % 4, l1);
  }
  lcdLine(0, l0);
  lcdLine(1, l1);
}

// Find the LCD at power-up. Its backpack is at 0x27 or 0x3F depending on the maker.
void initLCD() {
  uint8_t addr = 0;
  if (i2cPresent(0x27)) addr = 0x27;
  else if (i2cPresent(0x3F)) addr = 0x3F;
  if (!addr) return;                        // the machine still works without a screen
  lcdAddr = addr;
  lcd = new LiquidCrystal_I2C(addr, 16, 2); // 16 characters x 2 rows
  lcd->init();
  lcd->backlight();
  memset(lcdCache, 0, sizeof lcdCache);    // unknown -> everything counts as changed
  lcdResyncMs = millis();
  lcdLine(0, "Machine Safety");
  lcdLine(1, "Monitor v" FIRMWARE_VERSION);
  while (lcdFlush(255)) { }                 // start-up: write it all at once
}
