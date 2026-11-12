# Contributing

Thanks for wanting to improve the Arduino Machine Safety Monitor. It started as a student project, and it is meant
to stay something a student can understand, build and extend. Every contribution is welcome: a fixed typo, a
better explanation, a new test, a new sensor.

## Where to start

- **New here?** Pick one of the [contributor labs](labs/). Each one is a self-contained project with the files to
  touch, the steps and a clear definition of done.
- **Found a bug?** [Open an issue](../../issues/new/choose) with the Serial Monitor output (type `s` for a status
  report). If the bug could let the motor run when it should stop, say so in the title.
- **Have an idea?** Open a feature issue first, so we can agree on the approach before you write code.

## Set up

You need git, the Arduino IDE (or `arduino-cli`), and for the tests a C++ compiler, `make` and Python 3.

```bash
git clone https://github.com/DhairyaSanathara/Arduino-Machine-Safety-Monitor.git
cd Arduino-Machine-Safety-Monitor
python3 -m pip install -r web/requirements.txt pytest
make test
```

`make test` compiles the real firmware for your PC against small stand-ins for the Arduino libraries, runs it
against a simulated machine (a person walking up, a jammed shaft, an unplugged sensor...), and runs the web server
tests. No hardware is needed. On Windows, run it inside WSL.

To try the dashboard without hardware: `make demo`, then open http://localhost:8000.

## How the code is organised

| Path | What lives there |
| --- | --- |
| `firmware/MachineSafetyMonitor/config.h` | Pins, limits and timing: the only file most hardware changes need |
| `firmware/MachineSafetyMonitor/src/` | The firmware, one module per job (see [docs/firmware.md](docs/firmware.md)) |
| `firmware/bringup/` | One small sketch per part, for testing hardware |
| `tests/firmware/` | Simulated machine and safety tests |
| `web/` | Python serial bridge and the single-page dashboard |
| `tests/web/` | Web server tests |
| `docs/`, `hardware/` | How everything works, wiring, bill of materials, photos |

## Rules for changes

1. **Safety first.** A change must never make the motor start, or keep running, when it should stop. If you
   change a limit, a state transition or an output, add or update a test in `tests/firmware/test_safety.cpp`.
2. **Keep it non-blocking.** No `delay()` in the running loop, and no waits longer than a few milliseconds. The
   E-stop interrupt must stay short.
3. **Mind the memory.** The Uno has 32,256 bytes of flash (99 % used) and 2,048 bytes of RAM. Put fixed text in
   flash with `F()` or `PROGMEM`, say how many bytes your change adds, and make big optional features
   compile-time switches like `HAS_INA219`.
4. **Settings go in `config.h`.** Pins and numbers people may want to change belong there, with a comment.
5. **Keep the serial protocol compatible.** Add new columns at the end of the data row and update
   [docs/serial-protocol.md](docs/serial-protocol.md); the dashboard reads the `H` line, so old pages keep working.
6. **Write for learners.** Comments explain why, not what. Docs use plain words, short sentences and real numbers.

## Pull requests

- One topic per pull request. Small is easier to review.
- Describe how you tested it, on real hardware if it touches wiring or timing. Photos and serial output help.
- Make sure `make test` passes; the CI runs it, plus a compile check for the Arduino Uno, on every push.
- By contributing you agree that your work is released under the [MIT License](LICENSE).

## Code style

- C++: 2-space indent, `camelCase` for functions and variables, `UPPER_CASE` for constants, the same style as
  the code around your change.
- Python: PEP 8, standard library only for `web/server.py` (plus `pyserial`).
- JavaScript in `web/index.html`: no frameworks and no build step; the page must work offline.

Please follow the [code of conduct](CODE_OF_CONDUCT.md) in issues, pull requests and discussions.
