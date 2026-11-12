// sensors.cpp - sensor tasks run by the scheduler
//
// Ultrasonic distance (median of 3), temperature trend (least squares),
// learned per-speed baselines, vibration RMS and tilt, motor supply checks.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ======================== 13. SENSOR TASKS =================================
// --- Ultrasonic: distance = echo time x speed of sound / 2 (there and back)
void taskUltrasonic(uint32_t now) {
  if (now - tPing < PING_MS) return;
  tPing = now;
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  uint32_t us = pulseIn(PIN_US_ECHO, HIGH, ECHO_TIMEOUT_US);
  float cm = (us == 0) ? DIST_MAX_CM : us * 0.0343 / 2.0;   // 343 m/s = 0.0343 cm/us
  if (cm > DIST_MAX_CM) cm = DIST_MAX_CM;
  distRaw[distIdx] = cm;
  distIdx = (distIdx + 1) % 3;
  distCm = median3(distRaw[0], distRaw[1], distRaw[2]);     // one bad echo cannot trip us
}

// --- Temperature trend: least-squares slope of the last TREND_N readings
float trendSlopePerMin() {
  uint8_t n = tCount;
  uint8_t start = (tHead + TREND_N - n) % TREND_N;
  float y0 = tBuf[start];
  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (uint8_t i = 0; i < n; i++) {
    float x = i;
    float y = tBuf[(start + i) % TREND_N] - y0;
    sx += x; sy += y; sxx += x * x; sxy += x * y;
  }
  float den = n * sxx - sx * sx;
  if (den <= 0) return 0;
  float perSample = (n * sxy - sx * sy) / den;        // deg C per reading
  return perSample * (60000.0 / TEMP_MS);             // -> deg C per minute
}

void taskTemperature(uint32_t now) {
  if (!tempOk || now - tTemp < TEMP_MS) return;
  tTemp = now;
  float t = dht->readTemperature(false, true); // one fresh reading (DHT22 ~5 ms, DHT11 ~25 ms)
  float h = dht->readHumidity();               // same reading, no second wait
  if (isnan(t) || t < -40 || t > 80) {         // no answer = wire loose or sensor dead
    if (tempBadCount < 255) tempBadCount++;
    if (tempBadCount >= 3) { tempFail = true; tempC = NAN; tCount = 0; tempRateValid = false; }
    return;
  }
  tempBadCount = 0;
  tempFail = false;
  tempC = t;
  if (!isnan(h)) humidity = h;
  tBuf[tHead] = t;
  tHead = (tHead + 1) % TREND_N;
  if (tCount < TREND_N) tCount++;
  // The DHT11 only reports whole degrees, so wait for the full ~30 s of readings
  // before trusting its rise rate (one 1 C step then reads as at most ~3 C/min).
  tempRateValid = (tCount >= (dhtModel == 11 ? TREND_N : 8));
  tempRate = tempRateValid ? trendSlopePerMin() : 0;
}

// --- Baselines and limits
void updateLimits() {
  if (!baselineReady) {                       // while learning: only the absolute limits
    vibWarn = vibStop = VIB_ABS_STOP_MG;
    curWarn = curStop = CUR_ABS_STOP_MA;
    return;
  }
  vibWarn = fmax(VIB_FLOOR_MG, vibBase * VIB_WARN_FACTOR);
  vibStop = fmax(vibWarn * 1.5, vibBase * VIB_STOP_FACTOR);
  if (vibStop > VIB_ABS_STOP_MG) vibStop = VIB_ABS_STOP_MG;
  if (vibWarn > vibStop * 0.8) vibWarn = vibStop * 0.8;

  curWarn = fmax(CUR_FLOOR_MA, curBase * CUR_WARN_FACTOR);
  curStop = fmax(curWarn * 1.3, curBase * CUR_STOP_FACTOR);
  if (curStop > CUR_ABS_STOP_MA) curStop = CUR_ABS_STOP_MA;
  if (curWarn > curStop * 0.8) curWarn = curStop * 0.8;
}

void finishLearning() {
  vibBase = learnVibSum / learnVibN;
  curBase = (learnCurN > 0) ? learnCurSum / learnCurN : 0;
  baselineReady = true;
  uint8_t b = speedPct / 10;                 // remember it for this speed
  baseVib[b] = vibBase;
  baseCur[b] = curBase;
  baseKnown |= (1u << b);
  updateLimits();
  logEventStart(F("BASELINE"));
  LOG.print(F(",speed_pct=")); LOG.print(speedPct);
  LOG.print(F(" vib_base_mg=")); LOG.print(vibBase, 1);
  LOG.print(F(" vib_warn=")); LOG.print(vibWarn, 0);
  LOG.print(F(" vib_stop=")); LOG.print(vibStop, 0);
  if (inaOk) {
    LOG.print(F(" cur_base_ma=")); LOG.print(curBase, 0);
    LOG.print(F(" cur_warn=")); LOG.print(curWarn, 0);
    LOG.print(F(" cur_stop=")); LOG.print(curStop, 0);
  }
  LOG.println();
  beep(60);
}

// --- Called every 0.5 s with a fresh vibration result
void onVibrationWindow(uint32_t now) {
  if (!motorChecksActive(now)) {
    lvl[R_VIB] = lvl[R_STALL] = 0;
    vibStopCount = stallCount = 0;
    return;
  }
  if (!baselineReady && state == ST_RUN && pwmNow == runPwm()) {
    learnVibSum += vibMg;
    learnVibN++;
    if (learnVibN >= LEARN_WINDOWS) finishLearning();
  }
  uint8_t l = levelHigh(vibMg, vibWarn, vibStop, vibWarn * 0.15, lvl[R_VIB]);
  if (l == 2) {
    if (vibStopCount < 255) vibStopCount++;
    if (vibStopCount < VIB_STOP_WINDOWS) l = 1;    // a single knock only warns
  } else {
    vibStopCount = 0;
  }
  lvl[R_VIB] = l;

  // Stall / jam: at a steady speed the motor must keep vibrating like it did when healthy
  if (baselineReady && vibBase >= STALL_MIN_BASE_MG && state == ST_RUN &&
      pwmNow == runPwm() && vibMg < vibBase * STALL_FACTOR) {
    if (stallCount < 255) stallCount++;
  } else {
    stallCount = 0;
  }
  lvl[R_STALL] = (stallCount >= STALL_WINDOWS) ? 2 : 0;
}

// --- Vibration RMS + tilt from one 0.5 s window of samples
void finishWindow(uint32_t now) {
  float mean[3];
  float var = 0;
  for (uint8_t k = 0; k < 3; k++) {
    float m = accSum[k] / nSamples;
    mean[k] = accOff[k] + m;
    var += accSq[k] / nSamples - m * m;       // variance = mean of squares - square of mean
    accOff[k] = mean[k];
    accSum[k] = 0;
    accSq[k] = 0;
  }
  nSamples = 0;

  // A real MPU6050 always has a little noise. Exactly zero variance = frozen sensor.
  if (var < 0.5) {
    if (mpuStuckCount < 255) mpuStuckCount++;
    if (mpuStuckCount >= 3) mpuFail = true;
    var = 0;
  } else {
    mpuStuckCount = 0;
    mpuFail = false;
  }
  vibMg = sqrt(var) * 1000.0 / ACC_LSB_PER_G;   // RMS of all 3 axes, in milli-g

  // Tilt = angle between today's gravity direction and the installed one
  if (rezeroTilt || !tiltRefSet) {
    for (uint8_t k = 0; k < 3; k++) tiltRef[k] = mean[k];
    tiltRefSet = true;
    rezeroTilt = false;
  }
  float dot = 0, ma = 0, mb = 0;
  for (uint8_t k = 0; k < 3; k++) {
    dot += mean[k] * tiltRef[k];
    ma  += mean[k] * mean[k];
    mb  += tiltRef[k] * tiltRef[k];
  }
  if (ma > 0 && mb > 0) {
    float c = dot / (sqrt(ma) * sqrt(mb));
    if (c > 1) c = 1;
    if (c < -1) c = -1;
    tiltDeg = acos(c) * 57.29578;
  }
  onVibrationWindow(now);
}

void taskMPU(uint32_t nowUs, uint32_t now) {
  if (!mpuOk) return;
  // Sensor lost (e.g. jammed I2C bus): retry only 5x per second, so each 25 ms
  // time-out cannot slow the whole loop down
  if (nowUs - tSampleUs < (mpuFail ? 200000UL : (uint32_t)SAMPLE_US)) return;
  tSampleUs = nowUs;
  int16_t a[3];
  if (!readAccel(a)) {
    if (mpuErrCount < 255) mpuErrCount++;
    if (mpuErrCount >= 20) mpuFail = true;     // 0.2 s of failed reads
    return;
  }
  mpuErrCount = 0;
  for (uint8_t k = 0; k < 3; k++) {
    float d = a[k] - accOff[k];                // small numbers keep float maths accurate
    accSum[k] += d;
    accSq[k]  += d * d;
  }
  if (++nSamples >= VIB_WINDOW) finishWindow(now);
}

// --- Motor supply: voltage from the A3 divider (or the optional INA219),
//     two electrical cross-checks, and current checks if an INA219 is fitted
void taskPower(uint32_t now) {
  if (now - tIna < INA_MS) return;
  tIna = now;
  if (inaOk) {
    if (!readINA()) {
      if (inaErrCount < 255) inaErrCount++;
      if (inaErrCount >= 5) inaFail = true;
      return;
    }
    inaErrCount = 0;
    inaFail = false;
  } else if (senseOk) {
    // Voltage divider: Vpin = V12 x 4.7 / (10 + 4.7), so V12 = Vpin x 3.13
    busV = analogRead(PIN_12V_SENSE) * (5.0 / 1023.0) * SENSE_RATIO;
  } else {
    return;                                    // nothing measures the motor supply
  }

  // Check 1: relay is ON, so the motor must have its 12 V
  if (isRunning() && now - relayChangeMs > 500) {
    if (busV < BUS_MIN_V) { if (powerBadCount < 255) powerBadCount++; }
    else powerBadCount = 0;
  } else {
    powerBadCount = 0;
  }
  lvl[R_NOPOWER] = (powerBadCount >= 3) ? 2 : 0;

  // Check 2: relay is OFF, so there must be NO voltage after it (welded contacts?)
  if (!relayOn && now - relayChangeMs > 800) {
    if (busV > BUS_MIN_V) { if (relayBadCount < 255) relayBadCount++; }
    else relayBadCount = 0;
  } else {
    relayBadCount = 0;
  }
  lvl[R_RELAY] = (relayBadCount >= 5) ? 2 : 0;

  // Check 3: overload / jam (only possible with an INA219)
  if (!inaOk) return;
  if (!motorChecksActive(now)) { lvl[R_CURRENT] = 0; curStopCount = 0; return; }
  if (!baselineReady && state == ST_RUN && pwmNow == runPwm()) {
    learnCurSum += currentMa;
    learnCurN++;
  }
  uint8_t l = levelHigh(currentMa, curWarn, curStop, curWarn * 0.1, lvl[R_CURRENT]);
  if (l == 2) {
    if (curStopCount < 255) curStopCount++;
    if (curStopCount < CUR_STOP_READS) l = 1;
  } else {
    curStopCount = 0;
  }
  lvl[R_CURRENT] = l;
}
