# Lab 2: Measure real RPM

**Difficulty:** intermediate · **Time:** a weekend · **Hardware:** a TCRT5000 IR reflective sensor module or an
A3144 Hall sensor with a small magnet (≈ ₹60), a strip of white tape or the magnet on the shaft

## Goal

Measure the motor shaft's real speed in revolutions per minute, send it to the PC as a new data column, and show
it on the dashboard next to the current estimate.

## Why it matters

The dashboard's RPM is an **estimate**: rated RPM × PWM duty × 10 V / 12 V. It cannot see a load slowing the
shaft, a slipping coupling or a stall. Stall detection today is indirect (vibration collapses when the shaft
stops). A measured RPM gives a direct check: if the PWM says "running" and the shaft says 0 RPM, something is
wrong. It also gives Lab 3 the shaft frequency it needs to label the vibration spectrum.

## The key idea

Put one mark on the shaft (white tape for the IR sensor, or a magnet for the Hall sensor). The sensor gives one
pulse per revolution. Count the time between pulses in an interrupt:

```
RPM = 60,000,000 / (microseconds per revolution)
```

At 300 RPM that is one pulse every 200 ms, so the interrupt costs almost nothing. Timing the period (instead of
counting pulses per second) gives a fresh reading every revolution, even at low speed.

## Pins

D2 and D3 are the Uno's external interrupts, but both buttons use them (the E-stop needs D3's interrupt). Use
**D7** with a **pin-change interrupt** instead: enable `PCINT23` in `PCMSK2` and `PCIE2` in `PCICR`, and write
`ISR(PCINT2_vect)`. It fires on both edges, so act only when the pin reads the edge you want. D12 is also free if
your relay is on D9.

## Where to start

| File | What to look at |
| --- | --- |
| `firmware/MachineSafetyMonitor/config.h` | add `#define HAS_RPM_SENSOR 0` and `PIN_RPM 7` next to `HAS_INA219` |
| `firmware/MachineSafetyMonitor/src/motor.cpp` | `onEstopInterrupt()`: an example of a short ISR |
| new `src/rpm.cpp` | your ISR, `initRPM()`, and `taskRPM()` that turns the period into RPM |
| `firmware/MachineSafetyMonitor/src/datalog.cpp` | `printDataHeader()` and `taskLog()`: the data row |
| `firmware/MachineSafetyMonitor/src/safety.cpp` | `onVibrationWindow()` holds today's stall check |
| `web/index.html` | the Shaft speed tile, `pwmToRpm()`, `DEFAULT_COLS` |
| `tests/firmware/sim.cpp` | add the shaft: generate the pulses from the duty cycle and `world.jam` |

## Steps

1. **Bring-up sketch first.** `firmware/bringup/rpm_test/rpm_test.ino`: read the sensor on D7 with the
   interrupt and print RPM once a second while `motor_speed.ino`'s settings drive the motor. Compare with a phone
   tachometer app or a slow-motion video, and note the readings at 25, 50, 75 and 100 % speed in your pull request.
2. **Add it to the firmware behind `HAS_RPM_SENSOR`.** In the ISR store only `micros()` differences in `volatile`
   variables. In `taskRPM()`, copy them with interrupts off (`noInterrupts()` ... `interrupts()`), ignore
   periods shorter than physically possible (debounce, for example 2 ms), and report 0 RPM when no pulse has come
   for 1 s.
3. **Add a data column.** Append `rpm` at the end of the `H` line and the `D` row (keeping existing columns in the
   same place). Print `nan` when the sensor is not fitted, like `current_ma`. Update `docs/serial-protocol.md` and
   the `data_mode_rows_match_the_header` test (it checks the column count).
4. **Simulate the shaft.** In `sim.cpp`, toggle `pinState[7]` once per revolution, with the speed proportional to
   the PWM duty, and stopped when `world.jam` is true. Call the pin-change ISR from the simulator the way
   `simIsr` calls the E-stop interrupt.
5. **Use it for safety.** With `HAS_RPM_SENSOR`, a running motor at steady PWM showing 0 RPM for 1 s is a stall
   (`R_STALL`). Keep the vibration-based check as a second, independent method.
6. **Show it.** In `web/index.html`, when the `rpm` column exists and is a number, show "measured" in the Shaft
   speed tile and plot it, and keep the estimate as a dashed line for comparison.

## Done when

- [ ] `make test` passes with a new test: `measured_rpm_follows_the_speed_setting` and
      `jammed_shaft_shows_zero_rpm_and_stops`.
- [ ] Built with `HAS_RPM_SENSOR 0`, the firmware is byte-for-byte the same size as before (nothing changes for
      people without the sensor).
- [ ] On the real machine, the measured RPM is within 5 % of a tachometer or video count at three speeds.
- [ ] The dashboard shows measured and estimated RPM together; with an old firmware it still works.
- [ ] Docs updated: protocol, wiring, BOM, a new `docs/components/` page for the sensor.

## Going further

The flash is almost full. If it does not fit with everything else, this is a good reason to try the
[roadmap](../../ROADMAP.md)'s port to an Arduino Uno R4 or ESP32.
