# MotemaSens

KCL Projects

This repo is for MotemaSens hardware, firmware and customer shared documents.

## Hardware docs

- `docs/hardware/Lobe_ESP32-S3_HSI.md` - hardware/software interface for the custom Lobe ESP32-S3 board.
- `docs/hardware/reference/` - source schematic and Waveshare reference PDFs used for the HSI.

## Current important note

The HSI found one issue that needs fixing before bring-up: `SPI2_MISO` for the microSD is connected to H1 pin 18, but that pin is `VSYS` on the Waveshare board. This needs to be moved to a proper GPIO before powering a populated board.
