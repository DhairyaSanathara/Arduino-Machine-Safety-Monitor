# Changelog

All notable changes to this project. Versions follow [semantic versioning](https://semver.org/).

## [2.2.0] - 2026-11-12

Open-source release.

### Changed

- Project renamed to **Arduino Machine Safety Monitor**.
- Firmware split from one 1,700-line sketch into `config.h` plus one module per job in
  `firmware/MachineSafetyMonitor/src/`. Behaviour is unchanged: the simulated machine produces byte-identical
  serial output before and after, in text and data mode, with a DHT11 and a DHT22.
- Hardware test sketches moved to `firmware/bringup/`.
- The web server keeps the exact arrival order of lines when replaying history to a new viewer (lines arriving in
  the same millisecond could be replayed out of order).

### Added

- Firmware tests that run on a PC against a simulated machine (`tests/firmware`, 17 tests).
- Web server tests (`tests/web`, 16 tests).
- Continuous integration: Uno compile check, firmware tests and web tests on every push.
- Contributor guide, code of conduct, safety notice, security policy, issue and pull request templates.
- Three contributor labs, a roadmap, a citation file and a bill of materials.
- Photos of the prototype.

## [2.1.0] - 2026-11-02

First public version, built for the ENR206 course at Ahmedabad University.

- Arduino Uno firmware: state machine with a latching emergency stop on a hardware interrupt, 2 s watchdog,
  energise-to-run relay, per-speed learned vibration limits, temperature rise rate, stall detection from
  vibration, tilt, I2C bus-jam recovery and a self-healing LCD.
- Readable text, CSV data and Serial Plotter output modes at 9600 baud.
- Python serial bridge and single-page web dashboard with live graphs, protection checks, session counters,
  event log and CSV logging; the lab PC controls, other viewers can watch and stop.
- Documentation for every component, wiring, calculations and testing.
