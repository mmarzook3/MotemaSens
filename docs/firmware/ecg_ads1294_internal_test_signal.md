# ADS1294 internal test signal branch

Branch: `ecg-ads1294-test-signal`

Firmware: `dev-2026.06.14.2-ecgtest`

## Purpose

This branch is for ECG path bring-up without electrodes connected.

It enables the ADS1294 internal test signal so we can check:

- ADS1294 SPI register writes and reads.
- Data frame reading.
- ECG graph movement on the LCD.
- ECG values in USB and WiFi CSV logs.
- Timing and sequence counters without floating electrode noise.

## Firmware mode

`platformio.ini` defines:

```ini
-DENABLE_ECG_INTERNAL_TEST_SIGNAL=1
```

When this flag is enabled:

- `CONFIG2` enables the ADS1294 internal test generator.
- `CH1SET` to `CH4SET` use the internal test signal input mux.
- RLD sensing is disabled.
- Lead-off sensing is disabled.
- The LCD ECG trace plots CH1 directly. Normal firmware plots `CH1 - CH2`, but that would cancel the internal test waveform because every channel receives the same generated signal.

## Expected result

With no ECG leads connected, the ECG trace should show a repeatable ADS1294 test waveform instead of random floating-input noise.

Serial boot should include:

```text
ADS1294 ready, id=0x90 test=1 ...
```

The channel registers should read back as `05,05,05,05`.

## Important

This branch is not for real ECG electrode testing. For real electrodes, use `main` or another normal ECG branch where ADS1294 channels are connected to the electrode inputs.
