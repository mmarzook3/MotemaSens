# Display smooth Core 1 test

Branch: `display-smooth-core1-test`

Firmware: `dev-2026.06.14.3-smooth`

## Change

- Disabled periodic ECG serial diagnostics by default.
- Slowed ECG diagnostic period to 5 seconds if it is explicitly re-enabled.
- Slowed system CPU/RAM status update from 1 second to 5 seconds.
- Cached flash/sketch usage at boot instead of recalculating during the display loop.
- Slowed WiFi page status refresh from 1 second to 3 seconds.
- Moved LCD drawing earlier in the Core 1 loop before HTTP/status/OTA checks.

## Test

- Built firmware with PlatformIO.
- Flashed directly to the board on `COM14`.
- Captured serial after reset.
- Captured camera frames after startup.

## Result

- Build passed.
- Upload passed.
- Serial confirmed the flashed firmware version: `dev-2026.06.14.3-smooth`.
- Serial no longer prints periodic `ECG_DIAG` lines during idle display mode.
- Device booted, WiFi started, QMI8658 accelerometer was detected, ADS1294 ECG init completed, and the acquisition/output tasks started.
- Camera frame shows normal display running on the board.

## Evidence

- Serial log: `docs/firmware/test_results/2026-06-14-display-smooth-core1-v14-3-serial.txt`
- Camera frames:
  - `docs/firmware/debug_captures/2026-06-14/display_smooth_core1_v14_3/frame_0.jpg`
  - `docs/firmware/debug_captures/2026-06-14/display_smooth_core1_v14_3/frame_1.jpg`
  - `docs/firmware/debug_captures/2026-06-14/display_smooth_core1_v14_3/frame_2.jpg`
