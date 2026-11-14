// indicators.cpp - status LEDs and buzzer
//
// Every state has its own light and sound pattern, so a person can tell what the
// machine is doing without reading the LCD.
//
// Part of Arduino Machine Safety Monitor - see ../MachineSafetyMonitor.ino

#include "app.h"

// ============================ 17. OUTPUTS ==================================
// Called on every loop pass. Blinking is worked out from the clock, no timers needed:
// (now / 150) % 2 flips between 0 and 1 every 150 ms -> a fast blink.
void taskIndicators(uint32_t now) {
  bool fast = (now / 150) % 2;
  bool slow = (now / 500) % 2;                 // flips every 500 ms -> a slow blink
  bool g = false, y = false, r = false, buzz = false;

  switch (state) {
    case ST_IDLE:
      g = slow;                                  // blinking green = ready
      y = (worstLevel >= 1);
      break;
    case ST_RUN:
      g = baselineReady ? true : fast;           // fast blink while learning
      break;
    case ST_WARN:
      y = true;
      buzz = (now % 1000) < 80;                  // short beep every second
      break;
    case ST_ESTOP:
      if (worstLevel == 2) { r = fast; buzz = fast; }        // danger still there
      else                 { r = true; buzz = (now % 2000) < 60; } // safe: press GO
      break;
    case ST_FAULT:
      r = slow; y = !slow;
      buzz = (now % 3000) < 60;
      break;
  }
  // A short beep asked for by beep() (signed difference: works across millis() wrap)
  if ((int32_t)(beepUntil - now) > 0) buzz = true;

  digitalWrite(PIN_LED_GREEN,  g ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, y ? HIGH : LOW);
  digitalWrite(PIN_LED_RED,    r ? HIGH : LOW);
  setBuzzer(buzz);
}

void lampTest() {                           // proves every LED and the buzzer work
  digitalWrite(PIN_LED_GREEN, HIGH);  delay(250);
  digitalWrite(PIN_LED_YELLOW, HIGH); delay(250);
  digitalWrite(PIN_LED_RED, HIGH);    delay(250);
  setBuzzer(true); delay(120); setBuzzer(false);
  delay(300);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED, LOW);
}
