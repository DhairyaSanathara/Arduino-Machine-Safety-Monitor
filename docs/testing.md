# Testing and troubleshooting

Bring the hardware up one part at a time with the sketches in `firmware/bringup`, then run the main program and
check every protection. All sketches use the Serial Monitor at 9600 baud. The firmware's logic is also tested
automatically on a PC, without hardware: see [Automated tests](#automated-tests).

## Bring-up order

| Step | Sketch | What it proves |
| --- | --- | --- |
| 1 | `serial_test` | The USB link is clean: numbered lines, none missing or garbled. Run it with nothing else connected |
| 2 | `power_check` | The 5 V supply is steady. Wiggle wires one at a time; a "5V DROPPED" line points at the loose one |
| 3 | `lcd_test` | The LCD answers, its address is found (0x27 or 0x3F) and the contrast is right |
| 4 | `hello_lcd` | Continuous printing; the count restarting at 1 means the board is resetting |
| 5 | `lcd_letters` | Moving letters; the screen repairs itself after a wire glitch and reports each dropout |
| 6 | `dht_test` | Detects DHT11 or DHT22 and prints temperature and humidity every 2 s |
| 7 | `mpu_test` | Acceleration, rotation, tilt, vibration and chip temperature twice a second |
| 8 | `pwm_finder` | Which PWM pin really controls the motor speed (stop, slow, fast every 8 s) |
| 9 | `motor_speed` | Relay plus L298N: type 0 to 100 to set the speed, `+`, `-`, `x` |
| 10 | `MachineSafetyMonitor` | The full system |

## Functional checks on the full system

| Check | How | Expected |
| --- | --- | --- |
| Power-up | Switch on | Lamp test (three LEDs, beep), self-test line, IDLE with blinking green; the motor does not start |
| Start | Press GO | RUN; fast green blink for about 6 s, then a `BASELINE` event |
| Speed | Send 30, then 100 | Speed graph steps; a new baseline is learned at each new speed |
| Person nearby | Hand at about 30 cm | WARNING, yellow LED, beeps, motor at 45 %; back to RUN 3 s after the hand leaves |
| Too close | Hand under 15 cm | ESTOP, red, alarm; GO refused while the hand is there |
| Reset | Remove hand, GO, GO | IDLE, then RUN |
| Distance accuracy | Ruler at 15, 40 and 100 cm | Readings within about 1 cm |
| Vibration | Tap the base firmly, or tape a small weight to the shaft | Vibration crosses its warning line |
| Tilt | Tilt the base past 10°, then 25° | WARNING, then ESTOP; `z` re-zeroes |
| Temperature | Warm the DHT with a hand | Rise rate climbs; 40 °C needs a heat source |
| Stall | At 30 % only, briefly hold the shaft | ESTOP "MOTOR STALLED" after about 1.5 s |
| E-STOP button | Press while running | Instant stop, latched until GO |
| Remote stop | `x` from the Serial Monitor or a phone on the website | ESTOP, reason REMOTE |
| Sensor loss | Unplug the DHT data wire while running | ESTOP "TEMP SENSOR ERR" after three failed readings (about 6 s); GO is refused until it is back |
| Sensor missing at power-up | Unplug the MPU6050, then power up | FAULT "MPU6050 ERROR"; the motor cannot be started |
| Watchdog | (Debug only) add an endless loop | The board restarts within 2 s and logs `WATCHDOG_RESET` |

## Problems we hit while building, and the fixes

| Symptom | Cause | Fix |
| --- | --- | --- |
| Board kept resetting; garbage characters in the Serial Monitor | Unstable supply from loose breadboard contacts; Serial Monitor speed not matching the sketch | `serial_test` and `power_check` to isolate it; everything at 9600 baud |
| LCD text appeared, then vanished or turned into symbols | A lost half-byte in 4-bit mode after an I2C glitch | Self-healing re-sync (0x3, 0x3, 0x3, 0x2) every 10 s, first tried in `lcd_letters` |
| Temperature readings made no sense | The sensor fitted was a DHT11, read as a DHT22 | Automatic model detection by plausibility |
| MPU6050 values froze, then the whole program hung | Loose SDA / SCL wires jammed the I2C bus | `Wire.setWireTimeout()`, slower retries, frozen-data detection that stops the machine |
| All LEDs and the buzzer on together, motor dead | Arduino unpowered while the 5 V rail fed the modules, or the system sitting in FAULT | Power the Arduino from VIN; read the FAULT reason on the LCD |
| Relay did not switch | Relay wire on D9 while the code used D12 | One `PIN_RELAY` setting, now D9 |
| Motor did not turn | ENA jumper cap still on, then the ENA wire on the module's 5V pin | Cap off, ENA wire through 10 kΩ to D5 |
| Speed did not change on some pins | Damaged pins on the board | `pwm_finder`; the speed signal is sent on both D5 and D6 |
| Motor barely turned; electronics reset when it started | Motor fed from the SMPS 5 V output | Motor supply moved to 12 V through the relay |
| Serial Monitor scrolled too fast to read | A data row every 0.5 s | Readable labelled report every 2 s by default; CSV only in data mode |

## Problems on the day

| You see | Do this |
| --- | --- |
| `COMxx is busy` from the server | Close the Arduino IDE's Serial Monitor and Plotter |
| FAULT | A sensor did not answer at power-up: reseat its wires and press GO |
| LCD garbage | Wait up to 10 s for the automatic re-sync |
| Motor won't start | A stop is latched: press GO once to reset, then again to start |
| Phone cannot open the page | Same Wi-Fi as the lab PC; allow Python through the Windows firewall |

## Automated tests

Run `make test` from the repository root (Linux, macOS or WSL on Windows; needs `g++`, `make`, Python 3 and
`pytest`). GitHub Actions runs the same tests, plus a compile check for the Arduino Uno, on every push and pull
request.

**Firmware tests** (`tests/firmware`). The real firmware files are compiled for the PC together with small
stand-ins for the Arduino core and the libraries (`tests/firmware/mocks`). A simulator (`sim.cpp`) plays the world
around the board: the distance to a person, the air temperature, the motor's vibration as a function of its PWM
duty, tilt and a jammed shaft, plus the MPU6050, the LCD and the buttons. Each test powers up a fresh machine in its
own process, changes the world and checks the firmware's state, stop reason, relay and PWM.

| Area | Tests |
| --- | --- |
| Power-up | Waits in IDLE with the motor off; a missing MPU6050 is a FAULT; a DHT11 is detected automatically |
| Running | GO soft-starts the motor; the vibration baseline is learned, once per speed |
| Proximity | Warning zone slows the motor and recovers; stop zone latches an emergency stop |
| Stops | E-STOP button cuts the motor inside the interrupt; remote stop from the PC |
| Machine health | Abnormal vibration, jammed shaft (stall), tilt, fast heating before the temperature limit, temperature sensor lost while running |
| Interface | Speed commands are checked; data-mode rows match the header |

Run one test with `make -C tests/firmware test T=<name>`. A deliberately broken E-STOP interrupt makes the suite
fail, which shows the tests really check the behaviour.

**Web server tests** (`tests/web`, pytest). They start `server.py` on a free port with a stand-in for the Arduino and
check the command whitelist, that remote viewers can only stop the machine, the 503 reply when no Arduino is
connected, history replay order for a new viewer, peaks and counters, the demo machine and file serving.
