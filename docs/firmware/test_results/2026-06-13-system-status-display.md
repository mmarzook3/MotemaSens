# System status display test

Date: 2026-06-13

Final firmware version:

- `dev-2026.06.13.9`

Change:

- Added compact LCD system status metrics on the combined ECG/MIC/ACC screen.
- `C0` shows Core 0 acquisition loop speed and activity usage.
- `C1` shows Core 1 output/logging/display loop speed and measured busy usage.
- `RAM` shows heap usage percentage and free heap KB.
- `MEM` shows firmware/sketch flash usage percentage and used sketch KB.

Layout notes:

- `dev-2026.06.13.7` proved the first panel location was too close to the round display edge.
- `dev-2026.06.13.8` moved the panel inward.
- `dev-2026.06.13.9` draws the panel after the graph and clears a small background area behind the text so it stays readable.

Verification:

- Build passed.
- USB flash passed on `COM14`.
- Serial boot confirmed `device version: dev-2026.06.13.9`.
- WiFi connected at `192.168.5.29`.
- LCD camera proof shows `V13.9` and the system status labels.

Evidence files:

- `docs/firmware/test_results/2026-06-13-system-status-com14.txt`
- `docs/firmware/test_results/2026-06-13-system-status-v13-8-com14.txt`
- `docs/firmware/test_results/2026-06-13-system-status-v13-9-com14.txt`
- `docs/firmware/debug_captures/2026-06-13/system_status_dev_2026_06_13_7_display.jpg`
- `docs/firmware/debug_captures/2026-06-13/system_status_dev_2026_06_13_7_display_crop.jpg`
- `docs/firmware/debug_captures/2026-06-13/system_status_dev_2026_06_13_8_display.jpg`
- `docs/firmware/debug_captures/2026-06-13/system_status_dev_2026_06_13_8_display_crop.jpg`
- `docs/firmware/debug_captures/2026-06-13/system_status_dev_2026_06_13_9_display.jpg`
- `docs/firmware/debug_captures/2026-06-13/system_status_dev_2026_06_13_9_display_crop.jpg`
