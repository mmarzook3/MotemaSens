# ECG, mic and accelerometer dev test plan

This is the working test plan for the ESP32-S3 Lobe board dev firmware.

## Scope

- Confirm the board boots and connects to WiFi.
- Confirm the LCD is readable and does not show SPI corruption.
- Confirm GPIO14 green heartbeat LED toggles while firmware is running.
- Confirm GPIO15 blue LED is on during USB or WiFi live logging.
- Confirm mic, accelerometer and ECG are all present in the display and USB log.
- Confirm direct-IP WiFi control endpoints work and `/stream` saves CSV at the expected dev cadence.
- Keep raw data, camera captures, plots and debug notes in git for every dev test run.

## Standard setup

- Board connected over USB serial on `COM14`.
- Serial baud rate: `115200`.
- WiFi SSID: `M3_Devices`.
- Display camera: Logitech C922, OpenCV camera index `1`.
- Dev firmware path: `firmware/mic_waveform_test`.

## Standard commands

Build:

```powershell
cd firmware\mic_waveform_test
python -m platformio run
```

Flash:

```powershell
python -m platformio run -t upload --upload-port COM14
```

USB live log:

1. Wait for `usb live test ready: send S for 60 seconds, X to stop`.
2. Send `S` to start logging.
3. Send `X` to stop early, or wait for 60 seconds.

WiFi live log:

1. Read the serial boot log for `wifi logger ready: http://<device-ip>/`.
2. Open `http://<device-ip>/api/status` and confirm JSON is returned.
3. Run `python tools\wifi_log_capture.py --host <device-ip> --duration 10 --out test_logs\wifi_captures\<date>\wifi_capture.csv`.
4. Confirm GPIO15 blue LED turns on during the capture and turns off after `/api/stop`.
5. Check row count and `ms` spacing in the CSV.
6. From the browser page, open `/stream` and confirm the Stop WiFi Log button still stops the active stream/download.

## Required evidence to keep

- Serial summary or raw serial CSV.
- Any generated plots or calibration summaries.
- Camera frame before and after display fixes.
- Firmware notes explaining what changed.
- Commit hash for the tested firmware.

## Pass criteria

- Firmware boots without watchdog reset.
- WiFi connects and the green status dot is visible.
- LCD has no full-screen coloured stripe corruption.
- ECG sequence increments in USB log.
- Raw ECG channel columns remain in USB log.
- WiFi CSV uses the same raw sensor columns as USB log.
- WiFi capture rate is close to 100 Hz for the current dev build.
- LCD ECG trace is readable and does not dominate the whole screen with common-mode noise.
- Mic and accelerometer traces remain visible after ECG changes.

## Current ECG display rule

The USB log keeps raw `ecg_ch1` to `ecg_ch4`.

The LCD uses a display-only `CH1 - CH2` differential trace with baseline removal, spike limiting, smoothing and adaptive scaling. This keeps the display readable while preserving raw data for offline analysis.
