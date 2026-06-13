# Core 0 acquisition-only cleanup - 2026-06-13

Firmware version tested: `dev-2026.06.13.17`

## Goal

Core 0 should be dedicated to sensor acquisition only. All display, logging, WiFi, USB, heartbeat detection, status processing and other higher-level work should stay on Core 1.

## Firmware change

- Moved mic heartbeat detection out of the Core 0 acquisition path.
- Added `micFrameQueue` so Core 0 sends complete mic frames to Core 1.
- Core 1 now drains mic frames, updates the mic display waveform and runs the heart-sound detector.
- Accelerometer debug counters and latest display/log values are updated on Core 1 when `accelSampleQueue` is drained.
- Removed the old display-point queue path from Core 0.
- Increased acquisition task priority so Core 0 is more strongly reserved for ECG, mic and accelerometer reads.
- Bumped firmware to `dev-2026.06.13.17`.

## Core split after this change

Core 0:

- ADS1294 ECG read
- QMI8658 accelerometer poll
- SPH0645 I2S mic frame read
- queue newest sensor frames

Core 1:

- heart-sound detection
- ECG display filtering
- accelerometer display/motion gate state
- LCD drawing
- USB logging
- WiFi logging/control
- OTA checks
- LED status
- serial debug output

## Verification

- PlatformIO build passed.
- Firmware flashed directly to the board on `COM14`.
- Serial boot confirmed `device version: dev-2026.06.13.17`.
- Camera capture after waiting for the UI confirmed the display is running with `V13.17`.

## Evidence files

- `docs/firmware/test_results/2026-06-13-core0-acquisition-only-v13-17-serial.txt`
- `docs/firmware/debug_captures/2026-06-13/core0_acquisition_only_v13_17_camera_0.jpg`
- `docs/firmware/debug_captures/2026-06-13/core0_acquisition_only_v13_17_camera_1.jpg`
