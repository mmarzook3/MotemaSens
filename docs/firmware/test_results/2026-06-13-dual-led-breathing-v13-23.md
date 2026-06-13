# Dual LED breathing heartbeat test

Firmware: `dev-2026.06.13.23`

## Change

- Removed the accelerometer tap LED feature.
- Changed GPIO14 `LED_GREEN` and GPIO15 `LED_BLUE` to opposite-phase PWM heartbeat LEDs.
- Blue LED is no longer used as the logging-active indicator.
- LED breathing is updated from Core 1, keeping Core 0 only for sensor acquisition.

## Test

- Built firmware with PlatformIO.
- Flashed directly to the board on `COM14`.
- Captured serial after reset.

## Result

- Build passed.
- Upload passed.
- Serial confirmed the flashed firmware version: `dev-2026.06.13.23`.
- Device booted, WiFi started, QMI8658 accelerometer was detected, ADS1294 ECG init completed, and the acquisition/output tasks started.
- Camera frame shows the device running after flash with an LED visible during the breathing cycle.

## Evidence

- Serial log: `docs/firmware/test_results/2026-06-13-dual-led-breathing-v13-23-serial.txt`
- Camera frames:
  - `docs/firmware/debug_captures/2026-06-13/dual_led_breathing_v13_23/frame_0.jpg`
  - `docs/firmware/debug_captures/2026-06-13/dual_led_breathing_v13_23/frame_1.jpg`
  - `docs/firmware/debug_captures/2026-06-13/dual_led_breathing_v13_23/frame_2.jpg`
  - `docs/firmware/debug_captures/2026-06-13/dual_led_breathing_v13_23/frame_3.jpg`
