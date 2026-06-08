# Hardware documents

This folder keeps the hardware documents that are shared with the customer.

## Files

- `Lobe_ESP32-S3_HSI.md` - main hardware/software interface.
- `reference/Lobe_ESP32-S3_custom_schematic.pdf` - custom PCB schematic.
- `reference/Waveshare_ESP32-S3-LCD-1.28_schematic.pdf` - Waveshare base board schematic.
- `reference/Waveshare_ESP32-S3-LCD-1.28_wiki.pdf` - Waveshare wiki export used as reference.

## Main open item

`SPI2_MISO` for the SD card is on H1 pin 18, but H1 pin 18 is `VSYS` on the Waveshare board. This needs a schematic fix before board bring-up.

