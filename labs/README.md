# Contributor labs

Labs are open challenges: real improvements this project needs, each written up so you can start without asking
anyone. Every lab says why it matters, what to buy, which files to open, the steps, and how you know you are done.

| # | Lab | You will learn | Hardware | Difficulty |
| --- | --- | --- | --- | --- |
| 1 | [Detect a dead ultrasonic sensor](01-detect-a-dead-ultrasonic-sensor/) | Fail-safe design, fault states, writing a test | One 10 kΩ resistor | Good first issue |
| 2 | [Measure real RPM](02-measure-real-rpm/) | Interrupts, pulse timing, extending a serial protocol and a dashboard | IR or Hall sensor (≈ ₹60) | Intermediate |
| 3 | [Vibration spectrum (FFT)](03-vibration-spectrum-fft/) | Sampling, the MPU6050 FIFO, FFT, ISO vibration units | None extra | Advanced |

## How to take a lab

1. Find the lab's issue in the [issue list](../../../issues?q=is%3Aissue+label%3Alab) and comment that you are
   working on it, so two people do not do the same work. If there is no issue, open one.
2. Fork the repository and create a branch, for example `lab-1-dead-sensor`.
3. Run `make test` before you change anything, so you know the starting point is green.
4. Follow the lab. Commit small steps. Ask questions on the issue whenever you are stuck.
5. Open a pull request that links the issue. Include serial output, and photos or a short video if you used
   hardware.

You do not need to finish a whole lab alone. A pull request that does step 1 well, or only adds the test, is a
real contribution.

## Ground rules for every lab

- **Never make stopping weaker.** A lab adds checks; it must not remove or delay existing ones.
- **Flash is tight.** The firmware uses 31,798 of 32,256 bytes (≈ 450 bytes free). Measure your change with
  `Sketch → Verify` and say how many bytes it adds. If it does not fit, put it behind a `#define HAS_...` switch
  in `config.h`, the way `HAS_INA219` is done, and default it to 0.
- **Add a test.** If the firmware's behaviour changes, add a test to `tests/firmware/test_safety.cpp` that fails
  before your change and passes after it.

## Ideas for more labs

Have an idea that fits this format? Write it up in a new folder (`labs/04-...`) following the same headings and
open a pull request. Good candidates are on the [roadmap](../ROADMAP.md).
