// Arduino Machine Safety Monitor (ENR206, Ahmedabad University) - Tinkercad model
//
// Wiring-check sketch for the Tinkercad circuit. It uses the same pins as the real
// firmware v2.2 (firmware/MachineSafetyMonitor/config.h), but it is NOT the full
// firmware: Tinkercad has no MPU6050 or DHT22 part (both are drawn as 8-pin headers),
// so vibration, tilt, temperature and humidity are not read here. The full firmware
// runs in the Wokwi model (hardware/wokwi).
//
//   GO (D2) starts the motor, E-STOP (D3) stops it.
//   HC-SR04: warning under 40 cm, stop under 15 cm (the same limits as the firmware).
//   Click the HC-SR04 during the simulation to move the test object.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const byte PIN_GO     = 2;    // green button to GND (internal pull-up)
const byte PIN_ESTOP  = 3;    // red button to GND (internal pull-up)
const byte PIN_ENA    = 5;    // L293D Enable 1&2: PWM sets the motor speed
const byte PIN_BUZZER = 8;    // buzzer +
const byte PIN_RELAY  = 9;    // relay coil, active-low: LOW = relay on
const byte PIN_ECHO   = 10;   // HC-SR04 ECHO
const byte PIN_TRIG   = 11;   // HC-SR04 TRIG
const byte LED_GREEN  = A0;
const byte LED_YELLOW = A1;
const byte LED_RED    = A2;

const int  DIST_WARN_CM = 40;
const int  DIST_STOP_CM = 15;
const byte RUN_PWM      = 200;   // about 70 % speed

LiquidCrystal_I2C lcd(0x27, 16, 2);
bool running = false;
bool stopped = false;            // true after an emergency stop, until GO

// One HC-SR04 measurement in cm (999 = no echo, nothing in front)
int readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (us == 0) return 999;
  return us / 58;                // sound: about 58 us per cm, there and back
}

// Write one LCD row and blank the rest of it
void showLine(byte row, const char* text) {
  lcd.setCursor(0, row);
  lcd.print(text);
  for (byte i = strlen(text); i < 16; i++) lcd.print(' ');
}

void startMachine() {
  running = true;
  stopped = false;
  digitalWrite(PIN_RELAY, LOW);  // relay on: 12 V reaches the L293D
  analogWrite(PIN_ENA, RUN_PWM);
}

void stopMachine(const char* reason) {
  running = false;
  stopped = true;
  analogWrite(PIN_ENA, 0);
  digitalWrite(PIN_RELAY, HIGH); // relay off: the motor supply is cut
  tone(PIN_BUZZER, 2000, 400);
  showLine(0, "E-STOP: press GO");
  showLine(1, reason);
}

void setup() {
  pinMode(PIN_GO, INPUT_PULLUP);
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH); // start with the motor supply off
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  lcd.init();
  lcd.backlight();
  showLine(0, "Machine Safety");
  showLine(1, "Tinkercad model");
  delay(1500);
  showLine(0, "READY: press GO");
  showLine(1, "");
}

void loop() {
  int d = readDistanceCm();

  if (!running && digitalRead(PIN_GO) == LOW) startMachine();
  if (running && digitalRead(PIN_ESTOP) == LOW) stopMachine("Button pressed");
  if (running && d < DIST_STOP_CM) stopMachine("Person too close");

  bool warn = running && d < DIST_WARN_CM;
  digitalWrite(LED_GREEN, running && !warn);
  digitalWrite(LED_YELLOW, warn);
  digitalWrite(LED_RED, stopped);

  if (running) {
    char line[17];
    showLine(0, warn ? "WARNING" : "RUN SAFE");
    if (d >= 999) snprintf(line, sizeof line, "D: clear");
    else snprintf(line, sizeof line, "D:%dcm", d);
    showLine(1, line);
  }
  delay(100);
}
