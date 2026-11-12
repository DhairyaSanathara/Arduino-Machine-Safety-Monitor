# Firmware

| Folder | What it is |
| --- | --- |
| [`MachineSafetyMonitor/`](MachineSafetyMonitor) | The main program for the Arduino Uno: `config.h` for settings, `src/` for the code, one module per job |
| [`bringup/`](bringup) | One small sketch per part, for bring-up and debugging (see [docs/testing.md](../docs/testing.md)) |

## Upload the main program

1. Arduino IDE → *Tools → Manage Libraries*, install:
   - **LiquidCrystal I2C** by Frank de Brabander
   - **DHT sensor library** by Adafruit (accept **Adafruit Unified Sensor**)
2. Open `MachineSafetyMonitor/MachineSafetyMonitor.ino`. The IDE opens `config.h` and the `src/` files as tabs.
3. Board *Arduino Uno*, upload.
4. Serial Monitor at **9600 baud**, any line ending. Type `?` for the command list.

The MPU6050 is read straight from its registers, so it needs no library. If compiling fails at
`Wire.setWireTimeout`, update *Arduino AVR Boards* in the Boards Manager.

With `arduino-cli`:

```bash
arduino-cli lib install "LiquidCrystal I2C" "DHT sensor library" "Adafruit Unified Sensor"
arduino-cli compile --fqbn arduino:avr:uno firmware/MachineSafetyMonitor
```

## Settings

Pins, limits, timing and hardware options are in [`MachineSafetyMonitor/config.h`](MachineSafetyMonitor/config.h).
The ones that most often need changing are listed in
[docs/wiring.md](../docs/wiring.md#settings-that-must-match-your-hardware).

Program size: 31,798 of 32,256 bytes of flash, 1,299 of 2,048 bytes of RAM. Turning on `HAS_INA219` or
`HAS_12V_DIVIDER` adds the optional current sensor or 12 V sense; only one of them fits at a time.

## Where things are

| File | Job |
| --- | --- |
| `MachineSafetyMonitor.ino` | `setup()` and `loop()` (the scheduler), and an overview of the design |
| `src/safety.cpp` | Limits, stop reasons, the state machine |
| `src/sensors.cpp` | Distance, temperature trend, vibration, tilt, learned baselines |
| `src/motor.cpp` | Soft start, relay, the E-STOP interrupt |
| `src/app.h` | Shared types, variables and every function's prototype |
| other `src/` files | Sensor drivers, LCD, LEDs and buzzer, commands, reports, data rows |

The full table, and how to add a sensor, is in [docs/firmware.md](../docs/firmware.md#source-files). Behaviour
tests that run on a PC are in [`tests/firmware`](../tests/firmware).

## Bring-up sketches

| Sketch | Checks |
| --- | --- |
| `serial_test` | USB serial link |
| `power_check` | 5 V supply drops (loose wires) |
| `lcd_test` | LCD address, contrast, counter |
| `hello_lcd` | Continuous LCD and serial output; shows resets |
| `lcd_letters` | Moving letters with automatic LCD repair |
| `dht_test` | DHT11 / DHT22 detection and readings |
| `mpu_test` | MPU6050 acceleration, rotation, tilt, vibration |
| `pwm_finder` | Which PWM pin controls the motor speed |
| `motor_speed` | Relay plus L298N speed control from the Serial Monitor |
