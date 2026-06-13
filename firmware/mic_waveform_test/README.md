# Mic waveform test

This is a quick test software for the custom Lobe ESP32-S3 board.

It reads the SPH0645LM4H-B I2S mic, the Waveshare onboard QMI8658 accelerometer and the custom-board ADS1294 ECG front end.
The round LCD currently shows raw ECG CH1 on the upper graph, mic on the middle graph and QMI8658 X/Y/Z accelerometer on the lower graph.

It also connects to WiFi. Dev builds are flashed directly over USB and do not run OTA while `DEVICE_VERSION` is `local-dev` or starts with `dev-`.
OTA is only for major tagged releases or manually triggered release builds.
When WiFi is connected, dev builds expose a local HTTP logger and control page at the device IP.

## Architecture

This test firmware follows the main firmware architecture in `../../docs/firmware/README.md`.

- `acquisitionTask` is pinned to Core 0. It only reads ECG, mic and accelerometer with short guarded sensor reads, then sends sensor frames to queues.
- `outputTask` is pinned to Core 1. It owns beat detection, display shaping, the combined LCD ECG/mic/accelerometer graph, USB logging, WiFi OTA for release builds and GPIO14 green heartbeat LED.
- Acquisition never waits for LCD, WiFi, OTA or future SD/USB/BLE output.

The sensor modules are split into separate files:

- `mic_sensor.*` for SPH0645 I2S mic acquisition and compact mic frames.
- `accel_sensor.*` for QMI8658 accelerometer acquisition.
- `ecg_ads1294.*` for ADS1294 ECG SPI bring-up and raw sample frames.
- `spi_display_guard.*` for serialising ADS1294 SPI reads and LCD flushes so the round display is not corrupted by cross-core SPI/DMA activity.
- `sensor_config.h` for enable flags and shared pin definitions.

Compile-time enable flags:

- `ENABLE_MIC_SENSOR`
- `ENABLE_ACCEL_SENSOR`
- `ENABLE_ECG_SENSOR`
- `ENABLE_ECG_RLD_DRIVE`
- `ENABLE_ECG_LEAD_OFF_DETECTION`
- `ENABLE_ECG_DC_SATURATION_DIAGNOSTIC`
- `ENABLE_ECG_NOISE_DIAGNOSTICS`
- `ENABLE_ECG_RLD_STABILITY_DIAGNOSTIC`

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

ADS1294 ECG from custom PCB:

- `ECG_MOSI` = GPIO36
- `ECG_MISO` = GPIO17
- `ECG_SCLK` = GPIO18
- `ECG_CS` = GPIO21
- `ECG_DRDY` = GPIO16
- `ECG_START` = GPIO33
- `ECG_RESET` = GPIO34
- `ECG_PWDN` = GPIO35

The dev firmware holds the ECG pins in safe states, wakes/resets the ADS1294, reads the ID register, starts conversions and displays raw CH1 using `RDATA` reads at the 100 Hz dev display/log cadence. It avoids depending on the narrow DRDY pulse for this screen test and restarts conversions if repeated zero frames are seen.
The USB log keeps raw ECG CH1-CH4 values, while the LCD ECG trace uses display-only CH1-CH2 differential cancellation, baseline removal, spike limiting, smoothing and adaptive scaling so common-mode noise does not dominate the round screen.

The ADS1294 dev driver also has configurable front-end diagnostics:

- RLD drive is enabled by default for CH1 and CH2 positive/negative inputs.
- Lead-off detection is enabled by default for CH1 and CH2 positive/negative inputs.
- DC saturation is flagged when any raw channel approaches the ADC rails.
- Cable/noise behaviour is estimated from CH1/CH2 common-mode step size.
- Possible RLD instability is flagged when common-mode and differential steps stay high together.

These are development diagnostics only. They confirm whether the firmware is configuring and observing the ADS1294 front end, but they do not replace final IEC/medical safety validation.

Waveshare onboard QMI8658 accelerometer:

- `IMU_SDA` = GPIO6
- `IMU_SCL` = GPIO7
- I2C address is auto-detected at `0x6A` or `0x6B`.
- Saturated QMI8658 axes are detected and removed from the motion gate and LCD trace instead of being treated as real movement.
- `acc_diag_flags`: `0x01` X saturated, `0x02` Y saturated, `0x04` Z saturated, `0x08` recovery/reinitialise attempted.

Status LED from custom PCB:

- `LED_GREEN` = GPIO14, PWM heartbeat LED.
- `LED_BLUE` = GPIO15, PWM heartbeat LED.

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

Dev builds mirror debug and live-test serial traffic to both USB CDC and UART0/CH343 using `DebugSerial`, so logs and `S`/`X` commands work from the native USB port or the UART bridge used for flashing.
Every firmware change must bump `DEVICE_VERSION` in `platformio.ini`, even for local USB flashing. The round LCD shows `SW <version>` plus a large short build ID such as `V13.5` near the top of the screen so the camera can confirm the flashed build.
On boot, the display shows a MotemaSens startup screen with a 3 second initialising progress bar before WiFi and sensor startup messages. The startup icon is generated from `docs/firmware/assets/motemasens_logo_source.svg` and embedded as a compact 1-bit mask in `src/motemasens_logo_mask.h`.

If the screen works but the waveform is flat, change `I2S_CHANNEL` in `src/main.cpp` from `I2S_CHANNEL_FMT_ONLY_LEFT` to `I2S_CHANNEL_FMT_ONLY_RIGHT`.

## USB live heart-sound test

The dev firmware can stream a 60 second live test over USB serial.
During the live USB test the LCD graph refresh is slowed so serial logging keeps a stable 10 ms / 100 Hz timing.
The green and blue LEDs use opposite PWM breathing. As green fades up, blue fades down, then they swap. This is the device-alive heartbeat indicator and is no longer tied to accelerometer tap input or logging state.
The mic waveform on the LCD is display-decimated to half speed so a full LUB-DUB heart sound stays visible longer. This only changes the screen sweep speed; USB and WiFi logging keep the normal sensor cadence.

1. Flash the dev build directly over USB.
2. Open the serial monitor at `115200`.
3. Wait for `usb live test ready: send S for 60 seconds, X to stop`.
4. Place the device on the chest.
5. Send `S` over serial to start.

The firmware prints:

- `LIVE_TEST_START` when capture starts.
- `LOG_HEADER` with CSV column names.
- `LOG` rows every 10 ms / 100 Hz with Core 0 timestamps/counters, mic trace, mic level, beat envelope, beat threshold, motion level, BPM, accelerometer X/Y/Z, accelerometer diagnostic flags, latest raw ECG CH1-CH4 and ECG diagnostic fields.
- `BEAT` rows when the Core 1 heart-sound detector finds a beat. The beat timestamp is the envelope peak time after the mic frame is drained by the output task, and `delay_ms` shows how long it took before the output task printed the event.
- `LIVE_TEST_END` after 60 seconds or if `X` is sent. It includes the counted beats and rejected beat candidates.

ECG diagnostic CSV fields:

- `mic_ms`, `acc_ms` and `ecg_ms` are Core 0 timestamps for the latest mic, accelerometer and ECG frames used in that row.
- `mic_seq8`, `acc_seq8` and `ecg_seq8` are from one shared 8-bit Core 0 acquisition counter. It increments when Core 0 queues each sensor frame and wraps at `255 -> 0`. Use jumps in these fields to find missing or delayed captured data.

- `ecg_lead_off_p` and `ecg_lead_off_n` are ADS1294 lead-off masks for positive and negative inputs.
- `ecg_sat_mask` marks raw channels close to ADC full-scale.
- `ecg_diag_flags` is a bitmask: `0x0001` lead-off, `0x0002` DC saturation, `0x0004` cable/common-mode noise, `0x0008` possible RLD instability, `0x0010` RLD configured, `0x0020` lead-off configured.
- `ecg_common_step` and `ecg_diff_step` are smoothed step-size diagnostics used to spot shield/cable noise and RLD oscillation.

The LCD status area shows the same ECG health in compact form:

- `NOADS` means the ADS1294 did not answer the ID register read, so ECG diagnostics cannot run yet.
- `LO` means lead-off is currently detected.
- `SAT` means one or more ADS channels are close to the ADC rails.
- `RLD` means the RLD stability heuristic sees sustained high common-mode and differential movement.
- The row `P N S F C` shows positive lead-off mask, negative lead-off mask, saturation mask, diagnostic flags and common-mode step.

The combined screen also shows compact system health:

- `C0` - Core 0 acquisition loop speed only. It is not shown as CPU percentage because Core 0 is reserved for sensor acquisition.
- `C1` - Core 1 output/logging/display loop speed and measured busy usage.
- `R` - heap RAM used percentage.
- `M` - firmware flash/sketch used percentage.

## WiFi live logging and control

The dev firmware exposes local device-IP logging on port `80`.

Serial prints the URL after WiFi connects:

```text
wifi logger ready: http://192.168.5.29/
```

Endpoints:

- `/` - simple browser control/status page.
- `/api/status` - JSON status with firmware version, IP, logging state, ECG sequence and latest sensor values.
- `/api/start` - starts WiFi logging state and turns on the blue LED.
- `/api/stop` - stops WiFi logging and closes the active stream.
- `/stream` - starts WiFi logging and streams the same CSV rows as USB live logging.
- `/control?cmd=start` and `/control?cmd=stop` - simple control aliases.

Example PC capture:

```powershell
curl.exe http://192.168.5.29/stream --output test_logs\wifi_captures\2026-06-12\wifi_log.csv
```

Stop the stream with:

```powershell
curl.exe http://192.168.5.29/api/stop
```

Convert a downloaded `stream.csv` file into an interactive browser plot:

```powershell
python tools\csv_log_to_html.py C:\Users\mmarz\Downloads\stream.csv -o test_logs\wifi_captures\2026-06-12\stream_plot.html
```

Or use the Windows helper from the repo root. It asks for the CSV file path and opens the HTML plot when done:

```powershell
.\convert_stream_csv_to_html.bat
```

The generated viewer uses separate stacked panels for mic, ECG, accelerometer and BPM/motion. Raw ECG channels are hidden by default so the first view is readable instead of putting every signal on one cluttered graph. The viewer also has a `Browse CSV` button, so after opening one HTML plot you can load another downloaded CSV directly from the browser.

The blue LED is part of the alternating heartbeat animation and is not used as the logging-active indicator in this build.

The LCD graph refresh slows while high-rate USB or WiFi logging is active. This keeps the logger close to the 100 Hz dev cadence and avoids display work slowing down the CSV stream.

The stop and status endpoints remain available while `/stream` is active. The browser control page opens the CSV stream in a separate tab/download so the Stop WiFi Log button can still send `/api/stop`.

## Frequency calibration

For mic/filter calibration, run ambient first and then generated PC tones:

```powershell
python tools\run_frequency_calibration.py --port COM14 --tone-volume 0.18 --frequencies 60,80,100,120,150,200,250,300
```

The tool stores raw logs and writes a calibration CSV and Markdown report under `test_logs\frequency_calibration`.
Each capture resets the detector BPM, envelope, threshold and beat timing state so rows do not carry state from the previous capture.
Generated tones are sine-wave WAV files with software-controlled amplitude; lower `--tone-volume` if the report shows saturation.

## Heart-sound filtering

The dev firmware filters the mic into a body-sound band, tracks the heart envelope and uses the QMI8658 accelerometer as a motion gate.

- Very small mic levels are treated as noise.
- The beat threshold follows the local envelope and floor, with a lower weak-contact threshold so quiet heart sounds can still be detected.
- Beat detection is ignored when accelerometer motion is above the quiet-body threshold.
- The refractory time is set to `720 ms` so the detector avoids counting the same S1/S2 beat pair twice during resting heart-sound tests.
- A beat is confirmed at the local envelope peak instead of the first threshold crossing, which gives more precise timing for later heart-rate and S1/S2 work.
- Rejected noise or motion candidates are counted separately and do not reset the last accepted beat time. A very long rejected gap is used only to resync the detector after startup or placement noise.
- The USB log includes `beat_threshold` and `motion_level` so live captures can be checked after each test.

## Development and release build rule

Every dev change must be committed and pushed with a clear message, even when it is flashed directly over USB.
Docs must be updated in the same change when behavior, architecture, pins, build flow or release flow changes.

Dev builds:

- Build from `main`.
- Flash directly with PlatformIO over USB.
- Use a unique `DEVICE_VERSION` for every firmware change, normally `dev-YYYY.MM.DD.N`.
- Keep dev versions as `local-dev` or `dev-*` so OTA is skipped.
- Confirm the round LCD shows the expected `SW <version>` and large short build ID after flashing.
- Do not run OTA from the device.

Major releases:

- Use a major release tag matching `v*.*.0-*`, or run `.github/workflows/mic-waveform-release.yml` manually.
- The workflow builds the firmware and publishes:

- `device-release` branch
- `mic_waveform_test/firmware.bin`
- `mic_waveform_test/manifest.json`

Release firmware checks that manifest on boot and about once every minute after that.
