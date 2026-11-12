# Safety notice

The Arduino Machine Safety Monitor is a **teaching prototype**. It shows how machine safeguarding works: sensing,
a state machine, a latching emergency stop and fail-safe outputs. It is **not** a certified safety system and must
not be the only protection on a machine that can injure someone.

## Why it is not certified

- An Arduino Uno is not a safety-rated controller. Real machines use safety relays or safety PLCs, designed and
  validated to ISO 13849-1 performance levels or IEC 62061.
- The emergency stop button is read by the microcontroller. A real emergency stop (ISO 13850) is a positive-opening
  normally closed mushroom button wired directly into the power circuit, so it works even if all software fails.
- One HC-SR04 covers one direction in a narrow cone. If it is unplugged it reads "zone clear" (see
  [lab 1](labs/01-detect-a-dead-ultrasonic-sensor/)). Real machines use light curtains or safety laser scanners.
- The 15 cm stop distance is a tabletop demonstration. ISO 13855 safety distances for hands are far larger.
- RPM is estimated, the temperature sensor reads the air near the motor, and there is no current sensing.

The full list is in [docs/limitations-and-future-work.md](docs/limitations-and-future-work.md).

## Electrical safety when building one

- The SMPS is connected to mains voltage. Wire with it unplugged, cover its L, N and earth terminals, and earth
  its metal case.
- Never connect the 12 V output to any Arduino pin other than VIN.
- Keep fingers, hair and loose clothing away from the motor shaft while testing. Test stall detection only at low
  speed.

## Reporting a safety problem

If you find a way for the motor to start, or keep running, when it should stop, open an issue with "SAFETY" in the
title, or report it privately through [GitHub security advisories](../../security/advisories/new).
