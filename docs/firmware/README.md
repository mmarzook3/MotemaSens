# Firmware architecture

This document is the development rule for the MotemaSens ESP32-S3 firmware. Keep this file updated when the firmware architecture changes.

## Main rule

The firmware is split into two jobs:

| Job | ESP32-S3 core | What it owns | What it must not do |
| --- | --- | --- | --- |
| Sensor acquisition | Core 0 | ECG, mic and accelerometer reads, minimal sample conditioning, queues | Beat detection, display shaping, SD writes, WiFi, USB streaming, LCD drawing, long delays |
| Output and control | Core 1 | Beat detection, display shaping, SD logging, USB, WiFi, BLE control, LCD, LEDs, OTA, commands | Direct sensor timing or blocking sensor reads |

The user-facing name is:

- Core 1 = sensor acquisition
- Core 2 = logging, USB, WiFi, BLE and UI

In ESP32/FreeRTOS code these are zero-indexed:

- `ACQUISITION_CORE = 0`
- `OUTPUT_CORE = 1`

## Data flow

```text
Sensors -> acquisition task -> queue/ring buffer -> output task -> SD / USB / WiFi / BLE / LCD
```

The acquisition task must always be able to keep reading sensors. If SD card, WiFi, USB, BLE or LCD is slow, data should be buffered or dropped in a controlled way. The acquisition task must not wait for those outputs.

## Current implementation

The current `firmware/mic_waveform_test` firmware implements the pattern with pinned FreeRTOS tasks:

| Task | Core | Priority | Current purpose |
| --- | --- | --- | --- |
| `acquisitionTask` | `ACQUISITION_CORE` / Core 0 | High | Reads ADS1294 ECG, I2S mic and QMI8658 accelerometer, then sends sensor frames to queues |
| `outputTask` | `OUTPUT_CORE` / Core 1 | Lower | Drains queues, detects heart-sound beats, updates display/logging state, draws combined LCD ECG/mic/accelerometer graph, handles USB logging, direct-IP WiFi logging/control, WiFi OTA for release builds, blinks GPIO14 green LED |

The current queues are:

| Queue | Producer | Consumer | Payload |
| --- | --- | --- | --- |
| `micFrameQueue` | `acquisitionTask` | `outputTask` | Mic level and display points from the latest I2S frame |
| `beatEventQueue` | `outputTask` | `outputTask` | Beat interval, BPM estimate, signal level and gain after Core 1 beat detection |
| `accelSampleQueue` | `acquisitionTask` | `outputTask` | QMI8658 X/Y/Z acceleration in g |
| `ecgSampleQueue` | `acquisitionTask` | `outputTask` | ADS1294 status plus raw CH1-CH4 signed 24-bit samples at the current 100 Hz dev cadence |

If a queue is full, the oldest queued item is dropped before pushing the newest item. This keeps the newest live data visible and stops the acquisition task from blocking.

The current dev firmware keeps sensor code in separate modules under `firmware/mic_waveform_test/src`:

- `mic_sensor.*`
- `accel_sensor.*`
- `ecg_ads1294.*`
- `spi_display_guard.*`
- `sensor_config.h`

Each sensor can be disabled at build time with `ENABLE_MIC_SENSOR`, `ENABLE_ACCEL_SENSOR` or `ENABLE_ECG_SENSOR`.

The current ECG dev build uses ADS1294 `RDATA` reads every 10 ms so the LCD and USB log stay close to the 100 Hz dev cadence without depending on a narrow DRDY pulse. If repeated all-zero frames are read, the driver restarts conversions so the display does not remain frozen. The final logger should move ECG to an interrupt or DMA-style path before increasing the ECG sample rate.
LCD flushes and ADS1294 SPI reads are protected with a shared guard. Keep this in place while display and ECG run on different cores, because overlapping SPI/DMA activity can corrupt the round LCD image.

ECG front-end diagnostics are also behind build switches in `sensor_config.h`:

| Switch | Default | Purpose |
| --- | --- | --- |
| `ENABLE_ECG_RLD_DRIVE` | On | Configures ADS1294 internal RLD/bias drive for the active ECG inputs |
| `ENABLE_ECG_LEAD_OFF_DETECTION` | On | Enables ADS1294 lead-off sense on the active ECG inputs |
| `ENABLE_ECG_DC_SATURATION_DIAGNOSTIC` | On | Flags raw channels close to the ADC rails |
| `ENABLE_ECG_NOISE_DIAGNOSTICS` | On | Estimates cable/shield noise from common-mode movement |
| `ENABLE_ECG_RLD_STABILITY_DIAGNOSTIC` | On | Flags possible RLD loop oscillation when noise stays high |

The USB and WiFi CSV schema includes `mic_ms`, `mic_seq8`, `acc_ms`, `acc_seq8`, `ecg_ms`, `ecg_seq8`, `ecg_lead_off_p`, `ecg_lead_off_n`, `ecg_sat_mask`, `ecg_diag_flags`, `ecg_common_step` and `ecg_diff_step`. Keep these fields in future logger revisions until ECG hardware validation is complete.

The `*_seq8` fields come from a single 8-bit Core 0 acquisition counter. Core 0 increments this counter every time it queues a sensor frame. The value wraps at `255 -> 0`. Missing or unexpected jumps show where frames were dropped or delayed. Each `*_ms` field is the Core 0 timestamp for the latest frame from that sensor.

## Future full product layout

The full firmware should keep the same split.

### Acquisition task

- ADS1294 ECG sampling using DRDY interrupt.
- I2S mic sampling using DMA.
- QMI8658 accelerometer sampling over I2C.
- Sensor timestamping.
- Basic filtering or packet framing needed to preserve data.
- Push packets into ring buffers or queues.

The acquisition task should use short, deterministic code paths. It should not run beat detection, print frequently, write files, wait for network, draw to LCD or perform OTA.

### Output task

- SD card file creation and writes.
- USB streaming and command handling.
- Direct-IP WiFi logging and local browser/API control during dev.
- WiFi streaming/upload to server later when remote sessions need it.
- BLE mobile app control.
- LCD rendering.
- LED status.
- OTA update.
- Session metadata and configuration.

This task may block for short periods, but buffers must be sized so sensor acquisition continues without data loss.

## BLE control role

BLE is planned as a mobile control channel, not the primary high-rate data stream.

Use BLE for:

- Start/stop recording.
- Device mode.
- WiFi configuration.
- Patient/session ID.
- Battery and SD status.
- Firmware version.
- LED/self-test commands.
- Time sync from phone.

Use SD, USB or WiFi for high-rate ECG/mic data.

## WiFi logging and control

Dev firmware exposes a local HTTP server on the board IP after WiFi connects. This is the first WiFi logging path while SD card hardware is not ready.

Endpoints:

- `/` opens a small control/status page.
- `/api/status` returns JSON with firmware version, logging state, sample count, sample rate, ECG sequence, mic and accelerometer values.
- `/api/start` starts WiFi logging state and turns on GPIO15 blue LED.
- `/api/stop` stops WiFi logging and turns off GPIO15 blue LED if USB logging is also stopped.
- `/stream` starts WiFi logging and streams the same CSV schema as USB live logging.
- `/control?cmd=start` and `/control?cmd=stop` are simple command aliases.

PC capture helper:

```powershell
python tools\wifi_log_capture.py --host 192.168.5.29 --duration 10 --out test_logs\wifi_captures\2026-06-12\wifi_capture.csv
```

Direct-IP logging is best for bench testing because the PC can capture raw CSV without needing a cloud service or database. Later product firmware can add a server-backed upload path for remote patient sessions.

During high-rate USB or WiFi logging the LCD graph refresh slows down. The output task gives priority to the CSV stream so the dev cadence stays close to 100 Hz.

Stop and status commands must remain available even while `/stream` is active. Do not block `/api/stop` behind the active stream, because browser downloads keep the stream connection open until the device closes it.

## Buffering rule

All sensor data that must be saved should go through a queue or ring buffer.

Recommended pattern:

```text
acquisition packet -> ring buffer -> output writer -> SD/USB/WiFi
```

For high-value data such as ECG, prefer a larger ring buffer and report overflow. For live-only display data, dropping old points is acceptable.

## Timing rule

Sensor acquisition owns timing. Output features consume data after acquisition has made it available.

Bad pattern:

```text
read ECG -> write SD -> send WiFi -> draw screen -> read ECG again
```

Good pattern:

```text
Core 0: read ECG/mic -> queue packet -> read ECG/mic -> queue packet
Core 1: drain queue -> write SD / stream / draw / command handling
```

## SPI and shared buses

If ECG and SD use the same SPI bus in a future board revision, protect bus access with a mutex. Do not let SD writes delay ECG reads. If possible, use separate buses or keep ECG SPI transactions short and scheduled around DRDY.

## Status LED

GPIO14 / `LED_GREEN` is the device heartbeat LED. It toggles every second from the output task so there is always a quick visual sign that firmware is running.

## Build tracking rule

All firmware changes must be committed and pushed, including dev builds flashed directly over USB.

- Use clear commit messages written in the project style.
- Update docs when behavior, architecture, pins, build flow or release flow changes.
- Keep debug evidence in git: serial CSV files, camera/display pictures, calibration summaries, plots, test plans and test-result notes should go into dated folders under `test_logs/` or `docs/firmware/`.
- Do not publish OTA firmware for normal dev changes.
- Publish OTA only for major release tags matching `v*.*.0-*`, or from a manual release workflow run.
- Every firmware change must bump `DEVICE_VERSION` in `firmware/mic_waveform_test/platformio.ini`, even for local USB flashes.
- Dev firmware versions should use `dev-YYYY.MM.DD.N` and must not auto-update from OTA.
- The round LCD must show `SW <version>` plus a large short build ID such as `V13.5` so camera/bench photos prove the expected firmware is running.

## Development checklist

Before adding new firmware features, check:

- Does this belong to acquisition or output?
- Can this block sensor reading?
- Does this need a queue/ring buffer?
- What happens if SD/WiFi/USB/BLE is slow?
- How will overflow be detected and reported?
- Does this touch a shared bus?
- Is the behavior documented here?
- Was the change committed and pushed?
