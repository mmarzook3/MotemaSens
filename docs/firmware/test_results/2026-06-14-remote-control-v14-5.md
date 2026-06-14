# Remote Control Test - dev-2026.06.14.5-remote

Branch: `remote-control-test`

Firmware: `dev-2026.06.14.5-remote`

Date: 2026-06-14

## What changed

- Added mobile-app compatible HTTP control endpoints to the ESP32 firmware.
- Kept the existing browser/dev logger endpoints working.
- Added Android local HTTP support in the Flutter app.
- Updated the app status model to read the real ESP32 JSON fields.
- Added a mobile command to return the LEDs to breathing heartbeat mode after manual LED testing.
- Set the bench default app URL to `http://192.168.5.29`.

## ESP32 endpoints tested

- `GET /api/status`
- `POST /api/led`
- `POST /api/led-heartbeat`
- `POST /api/recording` with `ecg`
- `POST /api/recording` with `idle`
- `POST /api/self-test`

## Build and flash

Firmware build passed.

Firmware flashed successfully on `COM14`.

Serial boot confirmed:

```text
device version: dev-2026.06.14.5-remote
connecting wifi: M3_Devices
ADS1294 ready, id=0x90 cfg=96,C0,EC ch=00,00,00,00
mic accel ecg test running, acquisition core=0 output core=1
```

Evidence:

- `docs/firmware/test_results/2026-06-14-remote-control-v14-5-serial.txt`
- `docs/firmware/test_results/2026-06-14-remote-control-v14-5-http-api.txt`

## Mobile app verification

Flutter checks:

- `flutter analyze` passed.
- `flutter test` passed.
- `flutter build apk --debug` passed.

Android install and launch:

- Real phone: `RFCY508DWBP`
- Emulator: `emulator-5554`

Both targets installed the debug APK, launched the app, connected to `http://192.168.5.29`, and displayed firmware `dev-2026.06.14.5-remote`.

Evidence screenshots:

- `docs/firmware/debug_captures/2026-06-14/remote_control_v14_5/android_phone_launch.png`
- `docs/firmware/debug_captures/2026-06-14/remote_control_v14_5/android_phone_after_connect_corrected.png`
- `docs/firmware/debug_captures/2026-06-14/remote_control_v14_5/android_emulator_launch.png`
- `docs/firmware/debug_captures/2026-06-14/remote_control_v14_5/android_emulator_after_connect_corrected.png`

## Notes

- `batteryVolts` is currently reported as `0.0` because no battery ADC path is implemented yet.
- The firmware keeps the sensors running for live display; app recording commands currently control WiFi logging state.
- microSD remains blocked until the PCB `SPI2_MISO` issue is fixed.
