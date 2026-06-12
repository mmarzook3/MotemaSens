# Firmware architecture

This document is the development rule for the MotemaSens ESP32-S3 firmware. Keep this file updated when the firmware architecture changes.

## Main rule

The firmware is split into two jobs:

| Job | ESP32-S3 core | What it owns | What it must not do |
| --- | --- | --- | --- |
| Sensor acquisition | Core 0 | ECG, mic, timestamps, sample conditioning, queues | SD writes, WiFi, USB streaming, LCD drawing, long delays |
| Output and control | Core 1 | SD logging, USB, WiFi, BLE control, LCD, LEDs, OTA, commands | Direct sensor timing or blocking sensor reads |

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

The current `firmware/mic_waveform_test` firmware implements the pattern with two pinned FreeRTOS tasks:

| Task | Core | Priority | Current purpose |
| --- | --- | --- | --- |
| `acquisitionTask` | `ACQUISITION_CORE` / Core 0 | High | Reads I2S mic, filters heart-sound band, detects beat events, sends display points to queues |
| `outputTask` | `OUTPUT_CORE` / Core 1 | Lower | Draws LCD, handles WiFi OTA, blinks GPIO14 green LED, drains acquisition queues |

The current queues are:

| Queue | Producer | Consumer | Payload |
| --- | --- | --- | --- |
| `displayPointQueue` | `acquisitionTask` | `outputTask` | Signed waveform point for the LCD trace |
| `beatEventQueue` | `acquisitionTask` | `outputTask` | Beat interval, BPM estimate, signal level and gain |

If a queue is full, the oldest queued item is dropped before pushing the newest item. This keeps the newest live data visible and stops the acquisition task from blocking.

## Future full product layout

The full firmware should keep the same split.

### Acquisition task

- ADS1294 ECG sampling using DRDY interrupt.
- I2S mic sampling using DMA.
- Sensor timestamping.
- Basic filtering or packet framing needed to preserve data.
- Push packets into ring buffers or queues.

The acquisition task should use short, deterministic code paths. It should not print frequently, write files, wait for network, draw to LCD or perform OTA.

### Output task

- SD card file creation and writes.
- USB streaming and command handling.
- WiFi streaming/upload.
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

## Development checklist

Before adding new firmware features, check:

- Does this belong to acquisition or output?
- Can this block sensor reading?
- Does this need a queue/ring buffer?
- What happens if SD/WiFi/USB/BLE is slow?
- How will overflow be detected and reported?
- Does this touch a shared bus?
- Is the behavior documented here?
