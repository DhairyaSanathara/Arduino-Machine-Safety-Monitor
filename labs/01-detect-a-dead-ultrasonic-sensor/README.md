# Lab 1: Detect a dead ultrasonic sensor

**Difficulty:** good first issue · **Time:** an afternoon · **Hardware:** the existing HC-SR04 and one 10 kΩ resistor

## Goal

When the HC-SR04 is unplugged, broken or its ECHO wire falls out, the machine must stop (while running) or refuse
to start (at power-up), instead of believing the zone is clear.

## Why it matters

Today, "no echo came back" and "nothing is in front of the sensor" look the same to the firmware: both make
`pulseIn()` return 0, and `taskUltrasonic()` turns 0 into `DIST_MAX_CM` (150 cm, zone clear). So a sensor that
falls off the breadboard silently disables proximity protection. Safety engineers call this a
**fail-to-danger** fault, and it is the most serious known limitation of the project
([SAFETY.md](../../SAFETY.md), limitation 6 in [docs/limitations-and-future-work.md](../../docs/limitations-and-future-work.md)).

Every other sensor already fails safe: a missing MPU6050 or DHT is a FAULT at power-up and an E-stop while running.
This lab gives the ultrasonic sensor the same treatment.

## The key idea

After a trigger, a working HC-SR04 always raises its ECHO pin, about 0.5 ms later, even when nothing is in range.
With nothing in range most modules hold ECHO high for about 38 ms, which is longer than our 10 ms
`ECHO_TIMEOUT_US`. So when `pulseIn()` gives up:

| What happened | ECHO pin right after `pulseIn()` returns 0 |
| --- | --- |
| Sensor alive, nothing in range | still **HIGH** (the long pulse is still going) |
| Sensor dead or ECHO wire loose | **LOW** (no pulse ever started) |

A floating input pin can read anything, so a 10 kΩ pull-down resistor from D10 (ECHO) to GND makes "wire loose"
read a clean LOW.

Do not trust this table blindly: clones differ. Step 1 checks what your module really does.

## Where to start

| File | What to look at |
| --- | --- |
| `firmware/MachineSafetyMonitor/src/sensors.cpp` | `taskUltrasonic()`: where the reading is taken |
| `firmware/MachineSafetyMonitor/src/app.h` | the `R_...` reason list and the sensor-failure flags (`tempFail`, `mpuFail`) |
| `firmware/MachineSafetyMonitor/src/strings.cpp` | LCD text and PC codes for each reason |
| `firmware/MachineSafetyMonitor/src/safety.cpp` | `evaluate()`: how `tempFail` becomes a stop |
| `firmware/MachineSafetyMonitor/src/self_test.cpp` | `selfTest()`: power-up checks |
| `tests/firmware/sim.cpp` | the simulated `pulseIn()`; `world.distCm = 0` means unplugged |
| `tests/firmware/test_safety.cpp` | `temperature_sensor_lost_while_running_stops_the_machine` is the test to copy |

## Steps

1. **Measure your module.** Write `firmware/bringup/ultrasonic_test/ultrasonic_test.ino`: trigger every 100 ms,
   print the `pulseIn()` result and `digitalRead(ECHO)` right after it. Try three cases: hand at 20 cm, nothing in
   front (point it at the ceiling), and ECHO wire pulled out. Put the output in your pull request. This step alone
   is a useful contribution.
2. **Write the failing tests first.** In `test_safety.cpp`, add:
   - `unplugged_ultrasonic_is_a_fault_at_power_up`: set `world.distCm = 0` before `boot()`, expect `ST_FAULT`.
   - `ultrasonic_lost_while_running_stops_the_machine`: `startMotor()`, then `world.distCm = 0`, expect a latched
     `ST_ESTOP` within 1 s and the relay open.
   - `nothing_in_range_keeps_running`: `startMotor()`, then `world.distCm = 400`, expect `ST_RUN` after 10 s.

   Run `make test`. The first two must fail; that proves they test the problem.
3. **Teach the simulator about the ECHO pin.** In `sim.cpp`'s `pulseIn()`, leave `pinState[PIN_US_ECHO]` HIGH when
   the distance is beyond the timeout (the pulse is still going) and LOW otherwise, including when unplugged.
4. **Detect it in the firmware.** In `taskUltrasonic()`, when `us == 0` and ECHO reads LOW, count a missed ping;
   after several in a row (for example 5, which is 0.35 s at `PING_MS = 70`), set a new `distFail` flag. A good
   ping clears the count. Do not change the distance itself, so the median filter keeps working.
5. **Make it a stop reason.** Add `R_SENS_DIST` next to `R_SENS_TEMP` (the list is in priority order), its LCD
   text (16 characters at most, for example `DIST SENSOR ERR`) and PC code `SENS_DIST`. In `evaluate()`, set
   `lvl[R_SENS_DIST] = distFail ? 2 : 0;`.
6. **Check it at power-up.** In `selfTest()`, ping a few times; if the sensor never answers, set `distFail` so the
   machine starts in FAULT, and print `HC-SR04=OK` or `HC-SR04=MISSING` with the other self-test results.
7. **Show it on the dashboard.** In `web/index.html`, add `SENS_DIST` to the reason texts and to the "Sensors
   answering" check.
8. **Update the docs:** the reason table in `docs/serial-protocol.md`, `docs/components/hc-sr04-ultrasonic.md`,
   limitation 6 in `docs/limitations-and-future-work.md`, and the wiring note for the pull-down in `docs/wiring.md`.

## Done when

- [ ] All three new tests pass, and every old test still passes (`make test`).
- [ ] On the real machine: pulling the ECHO wire while running stops the motor and the LCD shows the reason;
      plugging it back and pressing GO restarts it.
- [ ] Pointing the sensor at an empty room does not stop the machine.
- [ ] The firmware still fits the Uno; the pull request says how many bytes it added.
- [ ] Docs updated.

## Going further

A dead sensor is not the only way to be blind: a sensor that has been knocked to point at the ceiling still
answers. A fixed reference target (a small plate at a known distance at the edge of the beam) would let the
firmware check it sees the plate every few seconds. Open an issue if you want to try it.
