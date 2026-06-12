# Mic waveform test

This is a quick test software for the custom Lobe ESP32-S3 board.

It reads the SPH0645LM4H-B I2S mic and the Waveshare onboard QMI8658 accelerometer.
The round LCD currently shows both live sensors: QMI8658 X/Y/Z accelerometer on the upper graph and the mic waveform on the lower graph.

It also connects to WiFi. Dev builds are flashed directly over USB and do not run OTA while `DEVICE_VERSION` is `local-dev`.
OTA is only for major tagged releases or manually triggered release builds.

## Architecture

This test firmware follows the main firmware architecture in `../../docs/firmware/README.md`.

- `acquisitionTask` is pinned to Core 0. It reads/processes the mic and accelerometer and sends queue events.
- `outputTask` is pinned to Core 1. It owns the combined LCD mic/accelerometer graph, WiFi OTA for release builds and GPIO14 green heartbeat LED.
- Acquisition never waits for LCD, WiFi, OTA or future SD/USB/BLE output.

## Hardware pins

LCD from Waveshare ESP32-S3-LCD-1.28:

- `LCD_DC` = GPIO8
- `LCD_CS` = GPIO9
- `LCD_CLK` = GPIO10
- `LCD_DIN` = GPIO11
- `LCD_RST` = GPIO12
- `LCD_BL` = GPIO40

Mic from custom PCB:

- `I2S_DATA` = GPIO3
- `I2S_BCLK` = GPIO4
- `I2S_WS` = GPIO5

The mic `SELECT` pin is tied low, so the firmware reads the left/low-WS slot first.

Waveshare onboard QMI8658 accelerometer:

- `IMU_SDA` = GPIO6
- `IMU_SCL` = GPIO7
- I2C address is auto-detected at `0x6A` or `0x6B`.

Status LED from custom PCB:

- `LED_GREEN` = GPIO14, active high, toggles every second when firmware is running.

## Build and upload

Create `include/local_secrets.h` for local flashing. This file is ignored by git.

```cpp
#pragma once
#define LOCAL_WIFI_SSID "your wifi"
#define LOCAL_WIFI_PASSWORD "your password"
```

```powershell
cd firmware\mic_waveform_test
pio run -t upload
pio device monitor
```

If the screen works but the waveform is flat, change `I2S_CHANNEL` in `src/main.cpp` from `I2S_CHANNEL_FMT_ONLY_LEFT` to `I2S_CHANNEL_FMT_ONLY_RIGHT`.

## USB live heart-sound test

The dev firmware can stream a 60 second live test over USB serial.
During the live USB test the LCD graph is paused so serial logging keeps a stable 10 ms / 100 Hz timing.

1. Flash the dev build directly over USB.
2. Open the serial monitor at `115200`.
3. Wait for `usb live test ready: send S for 60 seconds, X to stop`.
4. Place the device on the chest.
5. Send `S` over serial to start.

The firmware prints:

- `LIVE_TEST_START` when capture starts.
- `LOG_HEADER` with CSV column names.
- `LOG` rows every 10 ms / 100 Hz with mic trace, mic level, beat envelope, beat threshold, motion level, BPM and accelerometer X/Y/Z.
- `BEAT` rows when the heart-sound detector finds a beat. The beat timestamp is the acquisition-side envelope peak time, and `delay_ms` shows how long it took before the output task printed the event.
- `LIVE_TEST_END` after 60 seconds or if `X` is sent.

## Heart-sound filtering

The dev firmware filters the mic into a body-sound band, tracks the heart envelope and uses the QMI8658 accelerometer as a motion gate.

- Very small mic levels are treated as noise.
- The beat threshold follows the local envelope and floor so room noise is less likely to trigger a beat.
- Beat detection is ignored when accelerometer motion is above the quiet-body threshold.
- The refractory time is set long enough to avoid counting the same S1/S2 beat pair twice.
- A beat is confirmed at the local envelope peak instead of the first threshold crossing, which gives more precise timing for later heart-rate and S1/S2 work.
- The USB log includes `beat_threshold` and `motion_level` so live captures can be checked after each test.

## Development and release build rule

Every dev change must be committed and pushed with a clear message, even when it is flashed directly over USB.
Docs must be updated in the same change when behavior, architecture, pins, build flow or release flow changes.

Dev builds:

- Build from `main`.
- Flash directly with PlatformIO over USB.
- Keep `DEVICE_VERSION` as `local-dev`.
- Do not run OTA from the device.

Major releases:

- Use a major release tag matching `v*.*.0-*`, or run `.github/workflows/mic-waveform-release.yml` manually.
- The workflow builds the firmware and publishes:

- `device-release` branch
- `mic_waveform_test/firmware.bin`
- `mic_waveform_test/manifest.json`

Release firmware checks that manifest on boot and about once every minute after that.
