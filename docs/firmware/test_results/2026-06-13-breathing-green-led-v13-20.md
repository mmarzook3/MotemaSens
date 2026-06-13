# Breathing green heartbeat LED test

Firmware: `dev-2026.06.13.20`

## Goal

Replace the hard 1 second green LED blink with a smoother breathing heartbeat indicator.

## Behaviour

- Green LED on GPIO14 is driven by LEDC PWM.
- The pulse updates every 20 ms.
- The breathing period is 1600 ms.
- PWM duty ramps between 4 and 120 on an 8-bit scale.
- The update runs on Core 1, not in the Core 0 acquisition task.

## Verification

- Built firmware with PlatformIO successfully.
- Flashed `dev-2026.06.13.20` to the device on `COM14`.
- Serial boot log confirmed `device version: dev-2026.06.13.20`.
- QMI8658 accelerometer initialized successfully at `0x6B`.
- ADS1294 initialized successfully.
- Camera captures were saved for visual proof after boot.

## Evidence

- Serial proof: `docs/firmware/test_results/2026-06-13-breathing-green-led-v13-20-serial.txt`
- Camera proof:
  - `docs/firmware/debug_captures/2026-06-13/breathing_green_led_v13_20_camera_0.jpg`
  - `docs/firmware/debug_captures/2026-06-13/breathing_green_led_v13_20_camera_1.jpg`
