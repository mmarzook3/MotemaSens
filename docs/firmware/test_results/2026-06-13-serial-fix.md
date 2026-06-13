# Serial fix test

Date: 2026-06-13

Firmware version:

- `dev-2026.06.13.6`

Issue:

- With USB CDC enabled, debug output was not visible on the CH343 UART bridge port used for flashing.

Fix:

- Added `DebugSerial`.
- `DebugSerial` mirrors firmware logs to both USB CDC `Serial` and UART0 `Serial0`.
- USB live-test commands are read from both ports.

Verification:

- Build passed.
- Flash passed on `COM14`.
- Serial boot capture on `COM14` now shows firmware logs again.
- Command test on `COM14` passed: sent `S`, received `LIVE_TEST_START` plus CSV rows, then sent `X` and received `LIVE_TEST_END`.

Important capture lines:

```text
device version: dev-2026.06.13.6
wifi connected, ip=192.168.5.29
QMI8658 ready at 0x6B, accel +/-8g 1000Hz
ADS1294 ready, id=0x90 cfg=96,C0,EC ch=00,00,00,00
ADS1294 diag rld=1 lead_off=1 loff=00 rldp=03 rldn=03 loffp=03 loffn=03
LIVE_TEST_START,version=dev-2026.06.13.6,duration_ms=60000,sample_ms=10
LIVE_TEST_END,reason=stopped,elapsed_ms=2016,samples=144,beats=1,rejected=0,bpm=63.6
```

Evidence files:

- `docs/firmware/test_results/2026-06-13-serial-fix-com14.txt`
- `docs/firmware/test_results/2026-06-13-serial-command-test-com14.txt`
- `docs/firmware/debug_captures/2026-06-13/version_dev_2026_06_13_6_display.jpg`
- `docs/firmware/debug_captures/2026-06-13/version_dev_2026_06_13_6_display_crop.jpg`
