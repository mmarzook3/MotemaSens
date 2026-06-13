# ADS1294 internal test signal test

Branch: `ecg-ads1294-test-signal`

Firmware: `dev-2026.06.14.2-ecgtest`

## Change

- Added `ENABLE_ECG_INTERNAL_TEST_SIGNAL`.
- Enabled the ADS1294 internal test generator on this branch.
- Switched CH1 to CH4 input muxes to internal test signal mode.
- Disabled RLD and lead-off sensing while in internal test mode.
- LCD ECG plot uses CH1 directly in this mode so the shared test signal is not cancelled by `CH1 - CH2`.
- Display status shows `ECG TEST`.

## Test

- Built firmware with PlatformIO.
- Flashed directly to the board on `COM14`.
- Captured serial after reset.
- Captured camera frames after startup.

## Result

- Build passed.
- Upload passed.
- Serial confirmed the flashed firmware version: `dev-2026.06.14.2-ecgtest`.
- ADS1294 register readback confirmed internal test mode:

```text
ADS1294 ready, id=0x90 test=1 cfg=96,D0,E0 ch=05,05,05,05
ADS1294 diag rld=1 lead_off=1 loff=00 rldp=00 rldn=00 loffp=00 loffn=00
```

- Camera frame shows `ECG TEST` on the display and a clean generated ECG test waveform.

## Evidence

- Serial log: `docs/firmware/test_results/2026-06-14-ads1294-internal-test-signal-serial.txt`
- Camera frames:
  - `docs/firmware/debug_captures/2026-06-14/ads1294_internal_test_signal/frame_0.jpg`
  - `docs/firmware/debug_captures/2026-06-14/ads1294_internal_test_signal/frame_1.jpg`
  - `docs/firmware/debug_captures/2026-06-14/ads1294_internal_test_signal/frame_2.jpg`
