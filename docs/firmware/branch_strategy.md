# Firmware branch strategy

## Branches

`main` is the normal dev firmware branch and the 4-electrode ECG branch. Keep the current display, WiFi logger, diagnostics, startup UI and bring-up features here. ECG work on this branch assumes RA, LA, LL and RL/RLD electrodes.

`ecg-3-electrode` is for the 3-electrode ECG build. It should use RA, LA and LL only, with no RL/RLD electrode. This branch is for testing whether software filtering and front-end setup can make the ECG good enough without the driven reference electrode.

`optimised` is for low-power and production-style logging work. Use this branch for LCD-off operation, lower CPU load, SD batch writes, reduced serial output, and power/runtime testing.

`display-smooth-core1-test` is for testing display-freeze fixes before moving them to `main`. It keeps normal sensor firmware but reduces periodic Core 1 blocking work.

## Rule

Important shared fixes must be applied to both branches. Examples:

- hardware pin fixes
- sensor driver fixes
- CSV schema fixes
- safety or diagnostic fixes
- shared display, logging, BLE, WiFi or architecture changes
- documentation that applies to the whole device

Branch-specific work should stay on its branch until tested:

- UI/display work normally starts on `main`
- 4-electrode ECG/RLD behavior stays on `main`
- 3-electrode ECG behavior stays on `ecg-3-electrode`
- low-power SD logging work normally starts on `optimised`

After each branch is tested, merge or cherry-pick the approved changes intentionally. Any new feature that is not electrode-specific should be applied to both `main` and `ecg-3-electrode`.
