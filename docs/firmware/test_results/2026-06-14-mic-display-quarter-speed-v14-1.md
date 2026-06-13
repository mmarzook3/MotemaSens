# Mic display quarter speed test

Firmware: `dev-2026.06.14.1`

## Change

- Slowed the LCD mic waveform to half of the previous build again.
- The display now averages four mic display points before scrolling the mic trace by one pixel.
- Core 0 mic acquisition, heart-sound detection input, USB logging and WiFi CSV logging are not slowed.

## Why

The previous half-speed display was still too fast to keep the full LUB-DUB heart sound visible. This build makes the mic trace quarter speed compared with the original fast display.

## Test

- Built firmware with PlatformIO.
- Flashed directly to the board on `COM14`.
- Captured serial after reset.
- Captured camera frames after the startup screen.

## Result

- Build passed.
- Upload passed.
- Serial confirmed the flashed firmware version: `dev-2026.06.14.1`.
- Device booted, WiFi started, QMI8658 accelerometer was detected, ADS1294 ECG init completed, and the acquisition/output tasks started.

## Evidence

- Serial log: `docs/firmware/test_results/2026-06-14-mic-display-quarter-speed-v14-1-serial.txt`
- Camera frames:
  - `docs/firmware/debug_captures/2026-06-14/mic_display_quarter_speed_v14_1/frame_0.jpg`
  - `docs/firmware/debug_captures/2026-06-14/mic_display_quarter_speed_v14_1/frame_1.jpg`
  - `docs/firmware/debug_captures/2026-06-14/mic_display_quarter_speed_v14_1/frame_2.jpg`
