# Lab 3: Vibration spectrum (FFT)

**Difficulty:** advanced · **Time:** one to two weeks · **Hardware:** the existing MPU6050 (nothing to buy)

## Goal

Capture the motor's vibration fast enough to see its frequencies, compute a spectrum with an FFT on the PC, and
report vibration the way industry does: velocity in mm/s RMS (ISO 10816 / 20816).

## Why it matters

Today the firmware turns vibration into one number: the RMS acceleration over 0.5 s, sampled at 100 Hz. That
number says "more shaking than normal", but not why. Different faults shake at different frequencies:

| Fault | Where it shows in the spectrum |
| --- | --- |
| Imbalance (a heavy spot on the rotor) | a peak at 1 × shaft speed (RPM / 60 Hz) |
| Misalignment of the shaft coupling | peaks at 2 × and 3 × shaft speed |
| Looseness (a mounting screw backing out) | many harmonics of shaft speed |
| Bearing wear | peaks at higher, non-integer multiples, often hundreds of Hz |

At 100 Hz sampling we can only see up to 50 Hz (the Nyquist limit), and gear motors also have gear-mesh frequencies
above that. So this lab needs about 1 kHz sampling.

## The key idea

The Uno cannot read the MPU6050 over I2C 1,000 times a second and still run everything else. The MPU6050 can do the
work itself: set its sample rate to 1 kHz and let it store samples in its own 1,024-byte **FIFO**, then read them in
bursts. The heavy maths (the FFT) runs on the PC, where memory is free.

Velocity is acceleration integrated over time. In the frequency domain that is a division:
`v(f) = a(f) / (2π f)`. ISO 10816 uses the RMS velocity between 10 Hz and 1,000 Hz.

Be honest about the limits: the MPU6050's accelerometer outputs at most 1 kHz, so the spectrum ends at 500 Hz, and
its low-pass filter cuts in at about 184 Hz. Report the band you really measured (about 10 to 180 Hz) and call it
"ISO-style". A full ISO measurement needs a sensor with more bandwidth, for example an ADXL345 (3.2 kHz) or an
industrial piezo accelerometer.

## Where to start

| File | What to look at |
| --- | --- |
| `firmware/MachineSafetyMonitor/src/mpu6050.cpp` | `initMPU()` and `readAccel()`: register setup and reads |
| `firmware/MachineSafetyMonitor/src/sensors.cpp` | `taskMPU()` and `finishWindow()`: today's RMS and tilt |
| `firmware/bringup/mpu_test/mpu_test.ino` | a small standalone sketch to start from |
| `docs/components/mpu6050.md` | registers, ranges and units already used |
| `docs/calculations.md` | how today's RMS is computed |

MPU6050 registers you will need: `SMPLRT_DIV` (0x19), `CONFIG` (0x1A, low-pass filter), `FIFO_EN` (0x23),
`USER_CTRL` (0x6A), `FIFO_COUNT` (0x72), `FIFO_R_W` (0x74). See the InvenSense *MPU-6000/6050 Register Map*.

## Steps

1. **Capture sketch.** Write `firmware/bringup/vibration_capture/vibration_capture.ino`. Set `CONFIG` to
   `DLPF_CFG = 1` (184 Hz accelerometer bandwidth) and `SMPLRT_DIV = 0`, which gives 1 kHz, and put only the
   accelerometer into the FIFO. On the `c` command, stream 2,048 samples of one axis to the PC at 115,200 baud as
   they arrive: they do not fit in the Uno's 2 KB of RAM, and the FIFO holds only 170 samples (6 bytes each), so
   read it in bursts every few milliseconds. Watch `FIFO_COUNT`: if it reaches 1,024 the FIFO has overflowed and
   the capture is not valid; reset it and try again.
2. **Analysis script.** Write `tools/vibration_spectrum.py` (Python with `numpy` and `matplotlib`): read a capture
   from the serial port or a file, remove the mean, apply a Hann window, FFT, and plot the acceleration spectrum in
   mg and the velocity spectrum in mm/s. Print the RMS velocity over the measured band and the three biggest peaks.
3. **Test the maths without hardware.** Add `tests/tools/test_vibration_spectrum.py`: generate a 25 Hz sine of
   known amplitude, check the script finds a peak at 25 Hz and the right velocity
   (`v_rms = a_rms / (2π × 25)`).
4. **Measure your motor.** Take captures at 25, 50, 75 and 100 % speed. Is there a peak at RPM / 60? Add a small
   weight (a piece of tape with a nut) to one side of the shaft and capture again: the 1 × peak should grow. Put
   the plots in `docs/images/` and describe what you see.
5. **Classify it.** Compare the RMS velocity with the ISO 10816-1 zones for small machines (class I: below
   0.71 mm/s good, up to 1.8 satisfactory, up to 4.5 unsatisfactory, above that unacceptable) and write up the
   result in a new `docs/vibration-analysis.md`.
6. **Optional, hard:** show a live spectrum on the dashboard by adding a capture button and a `/spectrum` endpoint
   to `web/server.py`. Remote viewers must still be limited to watching and stopping.

## Done when

- [ ] Captures of 2,048 samples at 1 kHz arrive on the PC without FIFO overflow.
- [ ] The script's test passes, and it runs in CI (add it to the `web-tests` job or a new job).
- [ ] Spectra at four speeds are in the docs, with the shaft-speed peak marked; the added-weight experiment shows
      the 1 × peak growing.
- [ ] The main firmware is untouched (this lab adds a bring-up sketch and a tool), so `make test` still passes.

## Going further

- Combine with [Lab 2](../02-measure-real-rpm/): label the peaks as 1 ×, 2 ×, 3 × the measured shaft speed.
- On an ESP32 the FFT fits on the board itself: the firmware could trip on the 1 × peak instead of total RMS.
