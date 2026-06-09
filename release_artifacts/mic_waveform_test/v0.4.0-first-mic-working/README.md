# v0.4.0-first-mic-working

This is the first mic test firmware confirmed working properly on the ESP32-S3 round LCD board.

- Firmware file: `firmware.bin`
- Commit: `2da12a6c3e9ba23d25108a4c8df81c558d2fb3c5`
- SHA256: `DA0D4C74622865E3D6FA678A4E49DEEF5DA608CB13318288A3740F3436540673`

Flash with PlatformIO:

```powershell
pio run -t upload --upload-port COM6
```

The binary was built from `firmware/mic_waveform_test`.
