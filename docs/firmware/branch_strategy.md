# Firmware branch strategy

## Branches

`main` is the normal dev firmware branch. Keep the current display, WiFi logger, diagnostics, startup UI and bring-up features here.

`optimised` is for low-power and production-style logging work. Use this branch for LCD-off operation, lower CPU load, SD batch writes, reduced serial output, and power/runtime testing.

`ecg-ads1294-test-signal` is for ECG bring-up using the ADS1294 internal test signal. Use this branch to check SPI reads, ECG sample timing, ECG display scaling and CSV logging without electrodes connected. Do not use this branch for real electrode testing because the ADS1294 inputs are switched away from the electrode pins.

## Rule

Important shared fixes must be applied to both branches. Examples:

- hardware pin fixes
- sensor driver fixes
- CSV schema fixes
- safety or diagnostic fixes
- documentation that applies to the whole device

Branch-specific work should stay on its branch until tested:

- UI/display work normally starts on `main`
- low-power SD logging work normally starts on `optimised`

After each branch is tested, merge or cherry-pick the approved changes intentionally.
