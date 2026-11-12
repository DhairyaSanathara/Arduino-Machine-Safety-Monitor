# Roadmap

Where the project could go next. Items marked **lab** have a full step-by-step guide in [labs/](labs/), ready for
a contributor to pick up. Anything else is open for discussion: open a feature issue before starting.

## Now: make the prototype more trustworthy

| Item | Why | Status |
| --- | --- | --- |
| Detect a dead or unplugged ultrasonic sensor | Today an unplugged HC-SR04 reads "zone clear", a fail-to-danger mode | **lab 1** |
| Measured RPM with an IR or Hall sensor | RPM is estimated from the PWM duty; a measured value also catches a slipping or stalled shaft directly | **lab 2** |
| Vibration spectrum and ISO velocity | One RMS number cannot tell imbalance from a bad bearing | **lab 3** |
| Speed-of-sound correction from the DHT temperature | Distance is about 1.7 % off per 10 °C | open |
| Save learned baselines in EEPROM | The motor has to relearn its normal vibration after every power cut | open |

## Next: hardware changes

- A hardwired normally closed mushroom E-stop through a safety relay, so stopping does not depend on software.
- Current sensing (INA219 or ACS712) for direct overload and stall detection. The code exists (`HAS_INA219`) but
  does not fit the Uno's flash alongside everything else.
- A DS18B20 or thermistor fixed to the motor case, instead of air temperature.
- A soldered board with screw terminals to replace the breadboard.

## Later: beyond the Uno

- Port to an ESP32 or Arduino Uno R4 (more flash and RAM, Wi-Fi), with MQTT for monitoring without a PC.
- PlatformIO project for building from VS Code.
- A gas or smoke sensor (MQ-2) for fire and leak detection.
- A small UPS or supercapacitor so the controller can log and shut down cleanly in a power cut.

The reasoning behind each limitation is in [docs/limitations-and-future-work.md](docs/limitations-and-future-work.md).
