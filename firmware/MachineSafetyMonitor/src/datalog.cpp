// datalog.cpp - CSV data rows and the header lines for the dashboard
//
// Rows are built in memory and sent only as fast as the serial port takes them.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// One CSV row: the dashboard (part 2) and Excel both read this.
// At 9600 baud one character takes ~1 ms, so waiting for a ~100-character row
// would hold up the loop for ~40 ms (the serial port only buffers 64). Instead
// the row is built in memory and handed over only as fast as the port takes it.
char* addNum(char* p, float v, uint8_t dec) {
  dtostrf(v, 1, dec, p);
  p += strlen(p);
  *p++ = ',';
  return p;
}

void rowPump() {                               // send what fits right now, never wait
  while (rowPos < rowLen) {
    int room = Serial.availableForWrite();
    if (room <= 0) return;
    uint8_t n = rowLen - rowPos;
    if (n > room) n = room;
    Serial.write((const uint8_t*)rowBuf + rowPos, n);
    rowPos += n;
  }
}

void rowFinish() {                             // before printing anything else
  while (rowPos < rowLen) rowPump();
}

void taskLog(uint32_t now) {
  rowPump();
  if (plotMode) return;                        // a graph is shown: no CSV rows
  if (now - tLog < (textMode ? TEXT_MS : LOG_MS)) return;
  if (rowPos < rowLen) return;                 // last row still going out: skip one
  tLog = now;
  if (textMode) { textReport(); return; }
  uint8_t reason = (state == ST_ESTOP || state == ST_FAULT) ? latchedReason : worstReason;
  char* p = rowBuf;
  *p++ = 'D'; *p++ = ',';
  ultoa(now, p, 10);                  p += strlen(p); *p++ = ',';
  pgmText(STATE_NAME, state, p, 12);  p += strlen(p); *p++ = ',';
  pgmText(REASON_CODE, reason, p, 14); p += strlen(p); *p++ = ',';
  p = addNum(p, distCm, 1);
  p = addNum(p, tempC, 2);
  p = addNum(p, tempRate, 2);
  p = addNum(p, vibMg, 1);
  p = addNum(p, tiltDeg, 1);
  p = addNum(p, currentMa, 0);
  p = addNum(p, busV, 2);
  p = addNum(p, powerW, 2);
  p = addNum(p, pwmNow, 0);
  p = addNum(p, vibWarn, 0);
  p = addNum(p, vibStop, 0);
  p = addNum(p, curWarn, 0);
  p = addNum(p, curStop, 0);
  p = addNum(p, humidity, 1);
  itoa(speedPct, p, 10);              p += strlen(p); *p++ = ',';
  itoa(pwmToPct(pwmNow), p, 10);      p += strlen(p);
  *p++ = '\r'; *p++ = '\n';
  rowLen = p - rowBuf;
  rowPos = 0;
  rowPump();
}

void printDataHeader() {          // column names + limits, for the dashboard
  LOG.println(F("H,ms,state,reason,dist_cm,temp_c,temp_rate_cpm,vib_mg,tilt_deg,"
                   "current_ma,bus_v,power_w,pwm,vib_warn_mg,vib_stop_mg,cur_warn_ma,cur_stop_ma,humidity_pct,speed_pct,"
                   "speed_now_pct"));
  LOG.print(F("L,dist_warn_cm="));  LOG.print(DIST_WARN_CM, 0);
  LOG.print(F(",dist_stop_cm="));   LOG.print(DIST_STOP_CM, 0);
  LOG.print(F(",temp_warn_c="));    LOG.print(TEMP_WARN_C, 0);
  LOG.print(F(",temp_stop_c="));    LOG.print(TEMP_STOP_C, 0);
  LOG.print(F(",rate_warn_cpm="));  LOG.print(TRATE_WARN, 0);
  LOG.print(F(",rate_stop_cpm="));  LOG.print(TRATE_STOP, 0);
  LOG.print(F(",tilt_warn_deg="));  LOG.print(TILT_WARN_DEG, 0);
  LOG.print(F(",tilt_stop_deg="));  LOG.println(TILT_STOP_DEG, 0);
}

void printHeader() {
  LOG.println(F("# Arduino Machine Safety Monitor v" FIRMWARE_VERSION));
  if (!textMode) printDataHeader();
  printHelp();
}
