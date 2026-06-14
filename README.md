# MotemaSens

KCL Projects

This repo is for MotemaSens hardware, firmware and customer shared documents.

## Hardware docs

- `docs/hardware/Lobe_ESP32-S3_HSI.md` - hardware/software interface for the custom Lobe ESP32-S3 board.
- `docs/hardware/reference/` - source schematic and Waveshare reference PDFs used for the HSI.

## Firmware architecture

- `docs/firmware/README.md` - dual-core firmware architecture for acquisition, logging, USB, WiFi, BLE and UI.

Main rule:

- Core 0 in code is the acquisition core for sensors, timestamps and queues.
- Core 1 in code is the output/control core for SD, USB, WiFi, BLE, LCD, LED and OTA.
- Sensor acquisition must not wait for SD, USB, WiFi, BLE or LCD.

## Firmware tests

- `firmware/mic_waveform_test/` - reads the I2S mic and shows a smooth live waveform on the round LCD.

## Mobile app

- `mobile/` - Flutter Android app for controlling the MotemaSens ESP32 device.
- `docs/firmware/mobile_app_integration.md` - HTTP API contract between the Flutter app and ESP32 firmware.

## Current important note

The HSI found one issue that needs fixing before bring-up: `SPI2_MISO` for the microSD is connected to H1 pin 18, but that pin is `VSYS` on the Waveshare board. This needs to be moved to a proper GPIO before powering a populated board.
