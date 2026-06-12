# 2026-06-12 ECG display debug

## Purpose

Bring up ECG alongside mic and accelerometer, remove LCD corruption, and make the ECG trace readable on the round display.

## Firmware commits covered

- `3649d15` - add ECG raw reading and split sensors.
- `a6a6be1` - fix ECG screen freeze.
- `8a25699` - fix LCD corruption during ECG reads.
- `c555917` - clean up ECG display trace.

## Board and tools

- Board: custom Lobe ESP32-S3 board using Waveshare ESP32-S3-LCD-1.28 base.
- USB serial: `COM14` at `115200`.
- Camera: Logitech C922, OpenCV index `1`.
- WiFi: connected to `M3_Devices`.

## Key findings

- The full-screen coloured LCD bands were caused by display flush and ADS1294 SPI access overlapping across cores.
- A shared display/SPI guard stopped the LCD corruption.
- The ADS1294 is detected as `id=0x90`.
- Raw ECG channels are kept in USB logs.
- Raw CH1 alone was too noisy for the small round display.
- `CH1 - CH2` cancels common-mode noise much better for the LCD trace.

## Serial debug results

Before display differential filtering:

- CH1 range: about `-108750` to `-91276`.
- CH1 standard deviation: about `5304`.
- ECG sequence moved correctly.

Comparison from saved log `test_logs/ecg_display_tune_before.csv`:

| Signal | Standard deviation | 95 percent sample jump |
| --- | ---: | ---: |
| CH1 | `5229.2` | `13916` |
| CH2 | `5268.2` | `14200` |
| CH3 | `6260.4` | `15828` |
| CH4 | `1376.9` | `3633` |
| CH1-CH2 | `860.8` | `2391` |
| CH1-CH3 | `1559.5` | `4332` |

Final verification after CH1-CH2 display filtering:

- Boot reached `usb live test ready`.
- WiFi connected, IP seen as `192.168.5.29`.
- ADS1294 ready line: `ADS1294 ready, id=0x90 cfg=96,C0,E0 ch=00,00,00,00`.
- ECG sequence moved from `291` to `540` during short debug capture.
- `CH1-CH2` standard deviation was about `616.2`.
- No ECG recovery spam during final check.

## Camera evidence

Stored under `docs/firmware/debug_captures/2026-06-12/`.

- `WhatsApp Image 2026-06-12 at 4.12.19 PM*.jpeg` - LCD corruption examples with coloured bands.
- `codex-clipboard-ad186959-356b-497d-aebc-62eae3e2cbda.png` - Windows Camera view after LCD corruption fix.
- `codex_device_after_ecg_filter*.jpg` - intermediate display filtering captures.
- `codex_device_ecg_diff_filter_final.jpg` - final CH1-CH2 filtered display capture.
- `codex_device_camera_frame.jpg` - first wrong/dark camera capture, kept to document camera setup.

## Data artifacts

Live captures moved to:

- `test_logs/live_captures/2026-06-12/`

Generated analysis and calibration artifacts are under:

- `test_logs/`
- `test_logs/frequency_calibration/`
- `test_logs/frequency_calibration_volume_0035/`
- `test_logs/speaker_level_calibration/`

## Current result

Display is now usable for dev testing:

- No full-screen LCD colour corruption.
- ECG, mic and accelerometer labels and traces are visible.
- ECG display trace is smoother and based on CH1-CH2.
- Raw ECG data remains available in the USB CSV log for offline work.

## Follow-up

- Final ECG logger should move beyond the current dev `RDATA` path to a proper DRDY interrupt or DMA-style acquisition path.
- ECG electrodes/front-end configuration still need validation with real ECG input.
- Add a repeatable ECG signal source test when hardware is ready.
