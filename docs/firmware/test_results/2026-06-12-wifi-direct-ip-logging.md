# 2026-06-12 WiFi direct-IP logging test

## Build under test

- Firmware: `firmware/mic_waveform_test`
- Device version printed by firmware: `local-dev`
- USB port: `COM14`
- WiFi SSID used by board: `M3_Devices`
- Board IP printed by serial: `192.168.5.29`
- PC WiFi had to be switched from `M3` / `192.168.1.102` to `M3_Devices` / `192.168.5.102` before direct-IP access worked.

## What changed

- Added local HTTP server on the ESP32-S3 device IP.
- Added `/` control page.
- Added `/api/status`, `/api/start`, `/api/stop`, `/control?cmd=start`, `/control?cmd=stop`.
- Added `/stream` CSV output using the same columns as USB live logging.
- Added `tools/wifi_log_capture.py` so the PC can capture a timed CSV file and then stop logging.
- GPIO15 blue LED is on while USB or WiFi logging is active.
- LCD graph refresh now pauses during USB or WiFi logging so logging has priority.

## Commands run

```powershell
cd firmware\mic_waveform_test
python -m platformio run
python -m platformio run -t upload --upload-port COM14
```

```powershell
curl.exe --max-time 5 http://192.168.5.29/api/status
python tools\wifi_log_capture.py --host 192.168.5.29 --duration 10 --out test_logs\wifi_captures\2026-06-12\wifi_capture_direct_ip_10s_stream_priority.csv
```

## Results

Initial test while LCD continued refreshing:

- File: `test_logs/wifi_captures/2026-06-12/wifi_capture_direct_ip_10s.csv`
- Rows: 397
- Device time span: 13 ms to 9879 ms
- Effective row rate: 40.24 Hz
- Median row spacing: 25 ms

After pausing LCD during WiFi logging:

- File: `test_logs/wifi_captures/2026-06-12/wifi_capture_direct_ip_10s_display_paused.csv`
- Rows: 752
- Device time span: 12 ms to 9765 ms
- Effective row rate: 77.10 Hz
- Median row spacing: 10 ms
- One long gap was seen, max spacing 1163 ms.

After making the active stream priority over checking for another HTTP client:

- File: `test_logs/wifi_captures/2026-06-12/wifi_capture_direct_ip_10s_stream_priority.csv`
- Rows: 872
- Device time span: 31 ms to 9948 ms
- Effective row rate: 87.93 Hz
- Median row spacing: 10 ms
- Max spacing: 173 ms
- `/api/stop` returned status JSON and stopped WiFi logging cleanly.

## Status endpoint sample

```json
{"version":"local-dev","wifi_connected":true,"ip":"192.168.5.29","wifi_logging":false,"usb_logging":false,"wifi_samples":885,"wifi_beats":8,"wifi_elapsed_ms":0,"wifi_rate_hz":0.0,"ecg_seq":2016,"ecg_ready":true,"mic_level":0.0192,"acc_x":-0.2031,"acc_y":0.2410,"acc_z":-0.8796}
```

## Evidence files

- `test_logs/wifi_captures/2026-06-12/wifi_capture_direct_ip_10s.csv`
- `test_logs/wifi_captures/2026-06-12/wifi_capture_direct_ip_10s_display_paused.csv`
- `test_logs/wifi_captures/2026-06-12/wifi_capture_direct_ip_10s_stream_priority.csv`
- `docs/firmware/debug_captures/2026-06-12/wifi_direct_ip_display.jpg`
- `docs/firmware/debug_captures/2026-06-12/wifi_camera_probe_0.jpg`
- `docs/firmware/debug_captures/2026-06-12/wifi_camera_probe_1.jpg`

The camera evidence is not useful for display inspection in this run. Camera index 0 showed a colour test pattern and camera index 1 was dark. The serial and WiFi CSV evidence were usable.

## Notes

Direct-IP WiFi logging works and can be controlled from the PC when both devices are on `M3_Devices`.

The current direct HTTP stream is good enough for bench control and live development capture, but it is not yet a guaranteed 100 Hz logger. Most sample spacing is 10 ms after the fixes, but occasional gaps remain. For final product logging, use SD as the primary lossless store or add a binary buffered TCP/WebSocket protocol with a larger ring buffer before relying on WiFi as the only high-rate data path.
