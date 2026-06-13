# Accelerometer axis health check - 2026-06-13

Firmware version tested: `dev-2026.06.13.11`

## Reason for test

The accelerometer graph looked like it was not working correctly. The display was not the main issue, so the board was checked over USB serial while the firmware was running on the device.

## Finding

The QMI8658 accelerometer is alive on I2C and responds at address `0x6B`.

The X axis is not healthy:

- `raw_x` is stuck at `-32768`
- `acc_x_g` was previously stuck at `-8.0000g`
- Y and Z still move and return changing raw values

This means the accelerometer device is partly working, but the X axis is saturated at the register output. It is not a display graph problem.

## Firmware change

The firmware now detects saturated QMI8658 axes and marks them with `acc_diag_flags`.

Flags:

- `0x01` = X axis saturated
- `0x02` = Y axis saturated
- `0x04` = Z axis saturated
- `0x08` = firmware recovery/reinitialise attempt was made

When an axis is saturated, that axis is masked from:

- motion gate calculation
- LCD accelerometer trace value
- exported calibrated g value

The raw value is still logged, so the hardware fault is visible in debug data.

The firmware also tries to recover the IMU by reinitialising it, but only three times, so the serial log does not get spammed forever.

## Verification

Final live serial test:

- Firmware booted as `dev-2026.06.13.11`
- QMI8658 reported ready at `0x6B`
- Recovery was attempted three times
- `raw_x` stayed at `-32768`
- `acc_x_g` stayed masked to `0.0000`
- `acc_diag_flags` stayed `01`
- Y and Z continued changing
- motion level stayed low, so the bad X axis no longer poisons the movement detector

Final parsed result from `2026-06-13-accel-v13-11-live-com14.csv`:

- CSV rows parsed: `329`
- `raw_x` min/max: `-32768 / -32768`
- `acc_x_g` min/max: `0.0000 / 0.0000`
- `raw_y` min/max: `2896 / 2938`
- `raw_z` min/max: `9148 / 9177`
- `acc_diag_flags`: `01`
- recovery lines:
  - `QMI8658 recover attempt=1 ready=1 flags=09`
  - `QMI8658 recover attempt=2 ready=1 flags=09`
  - `QMI8658 recover attempt=3 ready=1 flags=09`

## Evidence files

- `docs/firmware/test_results/2026-06-13-accel-check-before-fix-com14.csv`
- `docs/firmware/test_results/2026-06-13-accel-check-before-fix-live-com14.csv`
- `docs/firmware/test_results/2026-06-13-accel-v13-10-live-com14.csv`
- `docs/firmware/test_results/2026-06-13-accel-v13-11-live-com14.csv`
- `docs/firmware/debug_captures/2026-06-13/accel_diag_dev_2026_06_13_11_display.jpg`
- `docs/firmware/debug_captures/2026-06-13/accel_diag_dev_2026_06_13_11_display_crop.jpg`

## Conclusion

The software side is now protected from the failed X axis, and the problem is clearly reported in logs and on screen as `ACC F1`.

For real 3-axis accelerometer operation, this board/module still needs a hardware fix or another QMI8658 module, because the X axis stayed saturated after firmware reinitialisation.

## Follow-up check

After the next board check, the accelerometer reported healthy values again.

Follow-up serial log: `docs/firmware/test_results/2026-06-13-accel-check-now-com14.csv`

Parsed result:

- Firmware: `dev-2026.06.13.11`
- CSV rows parsed: `521`
- `raw_x` min/max: `689 / 1018`
- `raw_y` min/max: `-108 / 32`
- `raw_z` min/max: `-3785 / -3621`
- `acc_diag_flags`: `00`

This means X is no longer stuck at `-32768` in this latest test. The firmware diagnostic is still useful, because if the QMI8658 saturates again it will show immediately in the display and logs.
