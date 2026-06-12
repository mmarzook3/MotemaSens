# 2026-06-12 WiFi stop fix

## Issue

Browser CSV download from `/stream` could not be stopped from the control page. The firmware was giving the active stream full priority and ignored new HTTP clients while the stream was connected, so `/api/stop` could not always reach the device.

## Fix

- Keep accepting `/api/stop`, `/control?cmd=stop`, `/api/status` and `/status` while `/stream` is active.
- Return a small busy JSON response for other requests while a stream is already active.
- Open the CSV stream from the control page in a separate browser tab/download.
- Keep the Stop WiFi Log button using `/api/stop` with no cache.

## Test plan

1. Build and flash the firmware.
2. Open `http://192.168.5.29/`.
3. Start `/stream` from a browser or PC script.
4. Send `/api/stop` while the stream is still active.
5. Confirm stream closes and `/api/status` returns `"wifi_logging":false`.

## Result

Passed on the live board at `192.168.5.29`.

The test opened `/stream`, waited about 2 seconds, then sent `/api/stop` from a second HTTP connection while the stream was still active.

Result JSON:

```json
{
  "lines": 193,
  "stream_closed": true,
  "stop_response": "{\"version\":\"local-dev\",\"wifi_connected\":true,\"ip\":\"192.168.5.29\",\"wifi_logging\":false,\"usb_logging\":false,\"wifi_samples\":192,\"wifi_beats\":1,\"wifi_elapsed_ms\":0,\"wifi_rate_hz\":0.0,\"ecg_seq\":2481,\"ecg_ready\":true,\"mic_level\":0.2823,\"acc_x\":-0.2029,\"acc_y\":0.2385,\"acc_z\":-0.8801}",
  "status_after": "{\"version\":\"local-dev\",\"wifi_connected\":true,\"ip\":\"192.168.5.29\",\"wifi_logging\":false,\"usb_logging\":false,\"wifi_samples\":192,\"wifi_beats\":1,\"wifi_elapsed_ms\":0,\"wifi_rate_hz\":0.0,\"ecg_seq\":2483,\"ecg_ready\":true,\"mic_level\":0.3956,\"acc_x\":-0.2031,\"acc_y\":0.2390,\"acc_z\":-0.8762}"
}
```

Evidence CSV:

- `test_logs/wifi_captures/2026-06-12/wifi_stop_fix_stream.csv`
