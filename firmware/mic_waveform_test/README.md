# Mic waveform test

This is a quick test software for the custom Lobe ESP32-S3 board.

It reads the SPH0645LM4H-B I2S mic and shows a smooth live waveform on the Waveshare 1.28 inch round LCD.

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

## Build and upload

```powershell
cd firmware\mic_waveform_test
pio run -t upload
pio device monitor
```

If the screen works but the waveform is flat, change `I2S_CHANNEL` in `src/main.cpp` from `I2S_CHANNEL_FMT_ONLY_LEFT` to `I2S_CHANNEL_FMT_ONLY_RIGHT`.

