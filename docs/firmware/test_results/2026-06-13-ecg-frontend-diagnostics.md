# ECG front-end diagnostics dev build

Date: 2026-06-13

## Change verified

- Added configurable ADS1294 RLD/bias setup.
- Added configurable ADS1294 lead-off setup.
- Added ECG diagnostic fields to USB and WiFi CSV logs.
- Added ECG diagnostic fields to WiFi `/api/status`.
- Added LCD status suffix for lead-off, saturation and possible RLD instability.

## Build

Command:

```powershell
python -m platformio run
```

Result: pass.

Build size:

- RAM: 50,476 bytes
- Flash: 813,381 bytes

## Flash

Command:

```powershell
python -m platformio run -t upload --upload-port COM5
```

Result: pass.

Detected device:

- ESP32-S3
- MAC: `1c:db:d4:79:e5:04`
- Port: `COM5`

## Runtime verification status

USB serial was initially not visible because the dev build had `ARDUINO_USB_CDC_ON_BOOT=0`. The dev build now enables USB CDC on boot, and serial diagnostic capture works on `COM5`.

Boot capture result:

```text
wifi connected, ip=192.168.5.89
ADS1294 not ready, id=0xFF
ECG_DIAG,ready=0,seq=0,status=000000,lop=00,lon=00,sat=00,flags=0000,common=0.0,diff=0.0,ch1=0,ch2=0,ch3=0,ch4=0
```

WiFi `/api/status` result also reports:

```json
{
  "ip": "192.168.5.89",
  "ecg_seq": 0,
  "ecg_ready": false,
  "ecg_lead_off_p": "0",
  "ecg_lead_off_n": "0",
  "ecg_sat_mask": "0",
  "ecg_diag_flags": "0"
}
```

This means the ECG diagnostic display/log plumbing works, but this connected hardware setup did not allow real ADS1294 lead-off/RLD/saturation testing yet. The ADS1294 SPI read returns `0xFF`, so the chip is not responding to the firmware.

Display camera evidence:

- `docs/firmware/debug_captures/2026-06-13/ecg_diagnostics_screen_noads.jpg`
- `docs/firmware/debug_captures/2026-06-13/ecg_diagnostics_screen_noads_crop.jpg`

The screen shows the ECG diagnostic UI and the not-ready/no-ADS state, matching the serial and WiFi results.

Next live ECG check should capture:

- Boot line `ADS1294 diag ...`
- `/api/status` fields `ecg_lead_off_p`, `ecg_lead_off_n`, `ecg_sat_mask`, `ecg_diag_flags`, `ecg_common_step`, `ecg_diff_step`
- One USB or WiFi CSV capture with electrodes open and then connected
- A non-`0xFF` ADS1294 ID before trying to validate lead-off/RLD behaviour

## Expected first checks

- With open electrodes, `ecg_diag_flags` should include `0x0010` and `0x0020`; lead-off masks should settle non-zero if the ADS1294 lead-off comparator sees the open input.
- With electrodes/cable connected normally, `ecg_sat_mask` should stay `00`.
- During quiet cable placement, `0x0008` RLD unstable should not remain continuously set.
