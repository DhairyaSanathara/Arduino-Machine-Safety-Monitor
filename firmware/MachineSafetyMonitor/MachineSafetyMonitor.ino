/*
 * ===========================================================================
 *   ARDUINO MACHINE SAFETY MONITOR  -  firmware v2.2
 *   Board : Arduino Uno R3        Serial : 9600 baud        Power : 12 V / 5 V SMPS
 *   https://github.com/DhairyaSanathara/Arduino-Machine-Safety-Monitor   (MIT licence)
 * ===========================================================================
 *
 *  WHAT IT DOES
 *  A 12 V DC gear motor is our "factory machine". Three sensors watch it, and
 *  ten times a second the Arduino decides one of three things:
 *     SAFE     -> green LED, motor at the speed you set from the laptop
 *     WARNING  -> yellow LED, motor slows to 45 %, a beep every second
 *     E-STOP   -> red LED, relay cuts the motor's 12 V, alarm, and the machine
 *                 stays LOCKED until the danger is gone AND a person presses GO
 *
 *  SENSORS AND WHAT WE CALCULATE
 *     HC-SR04  distance to a person (median of 3 pings)          -> safety zone
 *     DHT22 or DHT11 (found automatically) air temperature at the motor
 *              + rise rate (slope of a straight line through ~30 s) -> overheat / fire
 *              + humidity (shown and logged only)
 *     MPU6050  vibration RMS in milli-g (100 samples/s, 0.5 s windows) -> imbalance,
 *              tilt from the installed position                       loose mount,
 *              vibration that COLLAPSES at a steady speed             knocked over,
 *                                                                     stall / jam
 *  Vibration limits are LEARNED: the first time the machine runs at a speed
 *  after power-up, it measures the healthy machine for ~4 s and sets its limits
 *  as multiples of that baseline. Each speed (10 % steps) has its own baseline,
 *  because a motor shakes less at 40 % than at 100 %. Baselines are KEPT, so a
 *  fault that appears later is caught. 'l' forgets them and learns again.
 *
 *  SAFETY LAYERS
 *     1. Relay is "energise to run": if the Arduino resets, loses power or a
 *        wire breaks, the relay drops out and the motor stops.
 *     2. E-STOP button on a hardware interrupt: the motor is cut within
 *        microseconds, whatever the program is doing.
 *     3. Watchdog timer: if the code ever freezes, the chip resets in 2 s (-> 1).
 *     4. Latching E-stop with "first-out" reason: never restarts by itself.
 *     5. No auto-start at power-up; GO is refused while a danger is present.
 *     6. Every sensor is checked at start-up and while running: a lost sensor
 *        stops the machine (FAULT / E-STOP), a jammed I2C bus cannot freeze it.
 *
 *  WHERE THINGS ARE
 *     config.h            pins, hardware options, safety limits, timing  <- edit this
 *     src/app.h           shared types, state and function list
 *     src/sensors.cpp     ultrasonic, temperature trend, vibration, tilt, baselines
 *     src/safety.cpp      the decision (10x per second) and the state machine
 *     src/motor.cpp       soft-start ramp and the E-stop interrupt
 *     src/commands.cpp    buttons and serial commands
 *     src/reports.cpp, src/datalog.cpp   Serial Monitor text, Plotter graphs, CSV
 *     src/lcd.cpp, src/indicators.cpp    screen, LEDs, buzzer
 *     src/mpu6050.cpp, src/dht_sensor.cpp, src/ina219.cpp, src/i2c_bus.cpp  drivers
 *     docs/firmware.md    how it all works
 *
 *  FINAL WIRING  (do all wiring with the SMPS switched OFF)
 *   Power  SMPS L / N / earth  <- mains cable (3-pin plug, earth on the metal case)
 *          SMPS +12V -> relay COM  and  Arduino VIN
 *          SMPS +5V  -> breadboard + rail          (sensors, LCD, relay)
 *          SMPS COM  -> breadboard - rail  and  L298N GND
 *          Arduino GND -> breadboard - rail        Arduino 5V pin: NOT used
 *   Relay  VCC -> + rail, GND -> - rail, IN -> D9, COM <- SMPS +12V, NO -> L298N +12V
 *   L298N  +12V <- relay NO, GND <- SMPS COM, 5V terminal: empty (12V-EN jumper ON)
 *          ENA (jumper cap OFF): ENA input pin -> 10k resistor -> D5 or D6
 *          (the speed signal is sent on BOTH D5 and D6 - either one works)
 *          IN1 -> + rail, IN2 -> - rail (fixed direction), OUT1/OUT2 -> motor
 *          (100 nF ceramic capacitor across the motor terminals)
 *   HC-SR04  VCC -> +, GND -> -, TRIG -> D11, ECHO -> D10
 *   DHT      + -> +, - -> -, DATA -> D4   (bare 4-pin sensor: 10k DATA -> +)
 *   MPU6050  VCC -> +, GND -> -, SCL -> A5, SDA -> A4   (XDA/XCL/AD0/INT empty)
 *            Fix it to the SAME base as the motor, or it cannot feel the motor.
 *   LCD      VCC -> +, GND -> -, SCL -> A5, SDA -> A4
 *   Buttons  GO: D2 <-> - rail        E-STOP: D3 <-> - rail
 *   LEDs     A0 green / A1 yellow / A2 red -> 220R -> LED long leg, short leg -> -
 *   Buzzer   + -> D8, - -> - rail
 *   Not used: D0, D1 (USB serial), D7, D12, D13, A3
 *
 *  SERIAL MONITOR, 9600 baud, any line ending
 *     g          GO: start, or reset after a stop
 *     30 / 60 / 100 + Enter   motor speed in % (10..100, steps of 10)
 *     + / -      speed +-10 %          x   emergency stop
 *     s          status in plain words z   re-zero tilt
 *     l          re-learn baselines    ?   help
 *  OUTPUT     n = readable labelled report every 2 s (default, for the Serial Monitor)
 *             d = CSV data every 0.5 s (for the website in web/ - it sends d itself)
 *  GRAPHS (Tools -> Serial Plotter, 9600; type into the plotter's message box)
 *     v = vibration graph   t = temperature graph   b = both   n = back to text
 *  Data mode (d): H,... column names  L,... limits  D,... row every 0.5 s
 *                 E,... events (state changes, speed, baseline...)  #... notes
 *
 *  LIBRARIES (Arduino IDE -> Tools -> Manage Libraries)
 *     "LiquidCrystal I2C" by Frank de Brabander
 *     "DHT sensor library" by Adafruit (say YES to "Adafruit Unified Sensor")
 *  The MPU6050 (and an optional INA219 current sensor) are read straight from
 *  their registers, so they need no library.
 *  Optional extras, found automatically if fitted: INA219 (motor current, power,
 *  overload) and a 12 V sense divider on A3 (10k / 4.7k, detects a welded relay).
 * ===========================================================================
 * ===========================================================================
 */

#include "src/app.h"   // every module, setting and shared variable comes in through this one file

// setup() runs ONCE after power-up or a reset. Order matters: outputs are made
// safe first, then communication starts, then the sensors are checked, and only
// then is the machine allowed to wait for GO.
void setup() {
  uint8_t resetCause = MCUSR;   // MCUSR = the chip's "why did I reset?" register
  MCUSR = 0;                    // clear it, so the next reset reports its own cause
  wdt_disable();                // after a watchdog reset the watchdog is still running:
                                // switch it off now, or it would reset us again mid-setup

  // 1. Every output to its SAFE state before anything else
  // digitalWrite BEFORE pinMode: the pin's output level is chosen while it is still
  // an input, so the relay can never click ON for a moment when the pin becomes an output.
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);   // relay OFF
  pinMode(PIN_RELAY, OUTPUT);
  relayOn = false;
  pinMode(PIN_MOTOR_EN, OUTPUT);
#ifdef PIN_MOTOR_EN2
  pinMode(PIN_MOTOR_EN2, OUTPUT);
#endif
  motorStopNow();                           // PWM 0 on ENA: motor driver outputs off
  pinMode(PIN_BUZZER, OUTPUT);
  setBuzzer(false);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_US_TRIG, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  if (USE_12V_SENSE) pinMode(PIN_12V_SENSE, INPUT);   // set USE_12V_SENSE false to reuse A3
  pinMode(PIN_BTN_GO, INPUT_PULLUP);        // button to GND, no resistor needed
  pinMode(PIN_BTN_ESTOP, INPUT_PULLUP);
  // Hardware interrupt: the chip itself watches D3 and jumps to onEstopInterrupt()
  // (motor.cpp) the instant the button is pressed, even in the middle of other code.
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_ESTOP), onEstopInterrupt,
                  ESTOP_BUTTON_IS_NC ? RISING : FALLING);   // the moment it is pressed
                  // FALLING = the pin goes from HIGH (pull-up) to LOW (pressed to GND)

  // 2. Communication
  Serial.begin(SERIAL_BAUD);                // USB serial to the laptop, 9600 bits per second
  Wire.begin();                             // I2C bus on A4 (SDA) / A5 (SCL): LCD + MPU6050
  initLCD();                                // (its init() restarts Wire, so set options after)
  Wire.setClock(100000);                    // 100 kHz "standard mode": robust on long jumper wires
  Wire.setWireTimeout(25000, true);         // a noisy I2C bus can never freeze the loop
                                            // (compile error here? update "Arduino AVR Boards")

  // 3. Lamp test and sensor self-test
  lampTest();                               // every LED and the buzzer on briefly (indicators.cpp)
  selfTest();                               // find the MPU6050, DHT and LCD (self_test.cpp)
  printHeader();                            // first lines on the Serial Monitor (datalog.cpp)
  if (resetCause & _BV(WDRF)) {             // WDRF bit set = the watchdog caused this reset
    logEventStart(F("WATCHDOG_RESET"));
    LOG.println();
  }

  // 4. Start in IDLE (never auto-start), or FAULT if a required sensor is missing
  uint32_t now = millis();                  // milliseconds since power-up
  relayChangeMs = now;
  tPing = tIna = tLogic = tLog = tLcd = tLcdPage = tMotor = now;   // start every task timer now
  tSampleUs = micros();                     // the vibration sampler counts in microseconds
  if (!mpuOk)     enterFault(R_SENS_MPU);
  else if (!tempOk) enterFault(R_SENS_TEMP);
  else            enterIdle();

  if (USE_WATCHDOG) wdt_enable(WDTO_2S);    // from now on loop() must call wdt_reset() at least
                                            // every 2 s, or the chip resets itself
}

// =============================== 19. LOOP ==================================
// Nothing here waits. Each task checks "is it my turn yet?" and returns at once
// if not, so the loop runs thousands of times a second and never misses a button.
void loop() {
  if (USE_WATCHDOG) wdt_reset();            // "I am alive" - stop doing this and the chip resets
  uint32_t now = millis();

  if (estopIrq) {                           // the interrupt already cut the motor
    estopIrq = false;                       // (one byte: reading/clearing is atomic)
    if (state != ST_ESTOP && state != ST_FAULT) enterEstop(R_ESTOP_BTN);
  }
  updateButton(btnGo, now);                 // debounce both buttons (commands.cpp)
  updateButton(btnStop, now);
  if (btnGo.pressedEvent) { btnGo.pressedEvent = false; goRequest = true; }
  if (btnStop.pressedEvent) {               // E-stop reacts immediately, not on the next tick
    btnStop.pressedEvent = false;
    if (state != ST_ESTOP && state != ST_FAULT) enterEstop(R_ESTOP_BTN);
  }
  taskSerialIn();                           // commands typed on the laptop (commands.cpp)

  // Sensors (sensors.cpp). Each one runs only when its own timer says so.
  taskMPU(micros(), now);                   // vibration + tilt sample every 10 ms
  taskUltrasonic(now);                      // distance ping every 70 ms
  taskTemperature(now);                     // DHT reading every 2.1 s
  taskPower(now);                           // optional current / 12 V checks

  // The safety decision, 10 times a second (safety.cpp).
  // "now - tLogic" works even when millis() wraps round after 49 days,
  // because unsigned subtraction wraps the same way.
  if (now - tLogic >= LOGIC_MS) {
    tLogic = now;
    evaluate(now);                          // grade every check: OK / warning / stop
    runStateMachine(now);                   // decide IDLE / RUN / WARNING / ESTOP / FAULT
  }

  // Outputs
  taskMotor(now);                           // soft-start ramp towards the target speed (motor.cpp)
  taskIndicators(now);                      // LEDs and buzzer patterns (indicators.cpp)
  taskLCD(now);                             // decide what the LCD should show (lcd.cpp)
  lcdFlush(LCD_UNITS_PER_PASS);             // ...and send a few characters of it per pass
  taskLog(now);                             // text report or CSV data row (datalog.cpp)
  taskPlot(now);                            // Serial Plotter values, if a graph is on (reports.cpp)
}
