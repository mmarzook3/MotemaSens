# MotemaSens startup splash test

Firmware: `dev-2026.06.13.21`

## Goal

Show a MotemaSens startup screen with a 3 second initialising progress bar before the normal live display starts.

## Behaviour

- LCD backlight turns on.
- Splash screen is drawn immediately after LCD init.
- Screen uses grey background and yellow MotemaSens branding.
- Progress bar runs for 3 seconds.
- After the splash, WiFi, accelerometer, ECG and mic init continue as before.

## Verification

- Built firmware with PlatformIO successfully.
- Flashed `dev-2026.06.13.21` to the device on `COM14`.
- Serial boot log confirmed `device version: dev-2026.06.13.21`.
- Serial timing shows the later WiFi/sensor init still runs after the splash.
- QMI8658 accelerometer initialized successfully at `0x6B`.
- ADS1294 initialized successfully.

Camera proof was captured by opening the serial port to reset the ESP32 while the Windows Media Foundation camera feed recorded the display.

## Evidence

- Serial proof: `docs/firmware/test_results/2026-06-13-startup-splash-v13-21-serial.txt`
- Camera proof:
  - `docs/firmware/debug_captures/2026-06-13/startup_splash_v13_21_msmf/frame_018.jpg`
  - `docs/firmware/debug_captures/2026-06-13/startup_splash_v13_21_msmf/frame_045.jpg`
  - `docs/firmware/debug_captures/2026-06-13/startup_splash_v13_21_msmf/frame_060.jpg`
  - `docs/firmware/debug_captures/2026-06-13/startup_splash_v13_21_msmf/frame_084.jpg`
