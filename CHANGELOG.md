# Change log

## 2026-06-08

- Added the first hardware/software interface for the Lobe ESP32-S3 custom PCB.
- Added the custom schematic PDF and the Waveshare ESP32-S3-LCD-1.28 reference PDFs.
- Added the connector pin map from the Waveshare board to the custom PCB.
- Added firmware pin names for ECG, SD card, I2S mic, switches and LEDs.
- Added bring-up notes and called out the SD card `SPI2_MISO` routing issue that needs fixing.

## 2026-06-09

- Added mic waveform test software for the Waveshare round LCD.
- Reads the SPH0645 I2S mic from GPIO3/GPIO4/GPIO5.
- Shows a smooth live waveform with DC removal, smoothing and auto gain.
- Added WiFi connection and GitHub auto-update support for the mic test firmware.
- Changed the waveform view to use the round LCD safe area and real signed audio samples.
- Cleaned up the mic waveform screen so the trace stays inside the round display and looks smoother.
- Tagged the first properly working mic firmware and added the flashable bin file.
- Added the first heartbeat capture tuning with slower display, low-frequency filtering and BPM estimate.
- Changed OTA to use GitHub raw API download so the board does not get stuck on cached firmware manifests.
- Tightened heartbeat detection so it counts rising beats instead of repeating while the signal is still high.
