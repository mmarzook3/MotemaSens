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

Camera proof was attempted during the reset/startup window, but the available camera views were not pointed at the display, so those frames were not kept in git.

## Evidence

- Serial proof: `docs/firmware/test_results/2026-06-13-startup-splash-v13-21-serial.txt`
