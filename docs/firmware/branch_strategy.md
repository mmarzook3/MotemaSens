# Firmware branch strategy

## Branches

`main` is the normal dev firmware branch. Keep the current display, WiFi logger, diagnostics, startup UI and bring-up features here.

`optimised` is for low-power and production-style logging work. Use this branch for LCD-off operation, lower CPU load, SD batch writes, reduced serial output, and power/runtime testing.

`display-smooth-core1-test` is for testing display-freeze fixes before moving them to `main`. It keeps normal sensor firmware but reduces periodic Core 1 blocking work.

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
