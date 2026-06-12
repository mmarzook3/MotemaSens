# Mic waveform test

This is a quick test software for the custom Lobe ESP32-S3 board.

It reads the SPH0645LM4H-B I2S mic and the Waveshare onboard QMI8658 accelerometer.
The round LCD currently shows a smooth live X/Y/Z accelerometer graph.

It also connects to WiFi and checks GitHub for a newer firmware build. When a new firmware build is published by the GitHub Action, the board downloads it, flashes it and reboots.

## Architecture

This test firmware follows the main firmware architecture in `../../docs/firmware/README.md`.

- `acquisitionTask` is pinned to Core 0. It reads/processes the mic and accelerometer and sends queue events.
- `outputTask` is pinned to Core 1. It owns the LCD accelerometer graph, WiFi OTA and GPIO14 green heartbeat LED.
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

## Auto update

Every push that changes this firmware runs `.github/workflows/mic-waveform-release.yml`.

That workflow builds the firmware and publishes:

- `device-release` branch
- `mic_waveform_test/firmware.bin`
- `mic_waveform_test/manifest.json`

The board checks that manifest on boot and about once every minute after that.
