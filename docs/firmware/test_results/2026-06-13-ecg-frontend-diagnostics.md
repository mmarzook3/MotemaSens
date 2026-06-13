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

Serial capture did not return boot text in this Codex session after upload. The usual WiFi status IP did not respond from this PC after flashing, so live ADS1294 diagnostic register values were not captured in this result file.

Next live ECG check should capture:

- Boot line `ADS1294 diag ...`
- `/api/status` fields `ecg_lead_off_p`, `ecg_lead_off_n`, `ecg_sat_mask`, `ecg_diag_flags`, `ecg_common_step`, `ecg_diff_step`
- One USB or WiFi CSV capture with electrodes open and then connected

## Expected first checks

- With open electrodes, `ecg_diag_flags` should include `0x0010` and `0x0020`; lead-off masks should settle non-zero if the ADS1294 lead-off comparator sees the open input.
- With electrodes/cable connected normally, `ecg_sat_mask` should stay `00`.
- During quiet cable placement, `0x0008` RLD unstable should not remain continuously set.
