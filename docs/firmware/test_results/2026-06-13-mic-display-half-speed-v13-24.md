# Mic display half speed test

Firmware: `dev-2026.06.13.24`

## Change

- Slowed the LCD mic waveform to half visual speed.
- The firmware now averages two mic display points before scrolling the mic trace by one pixel.
- Core 0 mic acquisition, heart-sound detection input, USB logging and WiFi CSV logging are not slowed.

## Why

The mic trace was moving too fast to see a full LUB-DUB heart sound. The slower display sweep keeps the heart sound shape on the round LCD longer.

## Test

- Built firmware with PlatformIO.
- Flashed directly to the board on `COM14`.
- Captured serial after reset.
- Captured camera frames after the startup screen.

## Result

- Build passed.
- Upload passed.
- Serial confirmed the flashed firmware version: `dev-2026.06.13.24`.
- Device booted, WiFi started, QMI8658 accelerometer was detected, ADS1294 ECG init completed, and the acquisition/output tasks started.
- Camera frame shows the running display with `V13.24` visible.

## Evidence

- Serial log: `docs/firmware/test_results/2026-06-13-mic-display-half-speed-v13-24-serial.txt`
- Camera frames:
  - `docs/firmware/debug_captures/2026-06-13/mic_display_half_speed_v13_24/frame_0.jpg`
  - `docs/firmware/debug_captures/2026-06-13/mic_display_half_speed_v13_24/frame_1.jpg`
  - `docs/firmware/debug_captures/2026-06-13/mic_display_half_speed_v13_24/frame_2.jpg`
