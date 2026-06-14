# Stable smooth display release

Tag: `v0.10.0-stable-smooth-display`

Firmware: `dev-2026.06.14.3-smooth`

## Why this is tagged

This version is confirmed smooth on the device display after reducing periodic Core 1 blocking work.

## Included

- Smooth display fix from `display-smooth-core1-test`.
- Idle ECG serial diagnostic spam disabled by default.
- System status update slowed to reduce display stalls.
- WiFi page refresh slowed.
- Flash usage cached at boot.
- Display draw happens before WiFi/status/OTA work in Core 1.
- Previous stable features remain: mic, accelerometer, ECG pipeline, WiFi logger, startup screen and dual breathing LEDs.

## Flashable bin

The flashable app binary is tracked here:

`release_artifacts/mic_waveform_test/v0.10.0-stable-smooth-display/firmware.bin`

## Test proof

- Smoothness test report: `docs/firmware/test_results/2026-06-14-display-smooth-core1-v14-3.md`
- Serial proof: `docs/firmware/test_results/2026-06-14-display-smooth-core1-v14-3-serial.txt`
- Camera proof folder: `docs/firmware/debug_captures/2026-06-14/display_smooth_core1_v14_3/`
