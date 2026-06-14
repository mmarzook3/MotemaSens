# ESP32 App Firmware Integration

Checked on: 2026-06-14

## Purpose

This document explains how the MotemaSens Flutter mobile app is currently built so ESP32 firmware can be updated to communicate with it.

The app is designed to work in two modes:

- Demo mode: the app works locally without ESP32 firmware.
- Connected mode: the app sends HTTP requests to the ESP32 and reads JSON responses.

The current app source is:

```text
lib/main.dart
```

## App Communication Model

The app expects the ESP32 to expose a small HTTP API over Wi-Fi.

Default base URL in the app:

```text
http://192.168.4.1
```

This default is suitable if the ESP32 runs as a Wi-Fi access point. If the ESP32 joins an existing Wi-Fi network, enter its local network IP address in the app connection field.

## Required Firmware Endpoints

### Status

```text
GET /api/status
```

The app calls this endpoint when connecting and during status refresh.

Expected JSON response:

```json
{
  "greenLed": true,
  "blueLed": false,
  "sw1Pressed": false,
  "sw2Pressed": true,
  "batteryVolts": 4.08,
  "signalQuality": 92,
  "sampleRate": 500,
  "sdReady": false,
  "recordingMode": "idle"
}
```

Fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `greenLed` | boolean | Current state of GPIO14 green LED |
| `blueLed` | boolean | Current state of GPIO15 blue LED |
| `sw1Pressed` | boolean | Active-low SW1 status on GPIO42 |
| `sw2Pressed` | boolean | Active-low SW2 status on GPIO41 |
| `batteryVolts` | number | Battery/system voltage shown in the app |
| `signalQuality` | integer | ECG/signal quality percentage, 0-100 |
| `sampleRate` | integer | Current sample rate in Hz |
| `sdReady` | boolean | Whether microSD is available |
| `recordingMode` | string | `idle`, `ecg`, or `microphone` |

### LED Control

```text
POST /api/led
```

Request body:

```json
{
  "led": "green",
  "enabled": true
}
```

Valid `led` values:

```text
green
blue
```

Firmware behavior:

- `green` controls `LED_GREEN` on GPIO14.
- `blue` controls `LED_BLUE` on GPIO15.
- `enabled: true` drives the LED pin high.
- `enabled: false` drives the LED pin low.

Recommended response:

```json
{
  "ok": true
}
```

### Recording Control

```text
POST /api/recording
```

Request body:

```json
{
  "mode": "ecg"
}
```

Valid `mode` values:

```text
idle
ecg
microphone
```

Firmware behavior:

- `idle`: stop acquisition.
- `ecg`: start ADS1294 ECG acquisition.
- `microphone`: start I2S microphone acquisition.

Recommended response:

```json
{
  "ok": true
}
```

### Self-Test

```text
POST /api/self-test
```

Request body:

```json
{}
```

Firmware behavior should run a safe board self-test, for example:

- Blink green LED on GPIO14.
- Blink blue LED on GPIO15.
- Read SW1 on GPIO42.
- Read SW2 on GPIO41.
- Check ADS1294 SPI register read/write if ECG hardware is safe to power.
- Check I2S microphone clock/data setup.
- Skip microSD until the `SPI2_MISO` PCB issue is fixed.

Recommended response:

```json
{
  "ok": true,
  "message": "Self-test started"
}
```

## Hardware Mapping Used By The App

The app UI is based on the hardware interface file:

```text
docs/Lobe_ESP32-S3_HSI.md
```

Important firmware pins:

| Function | ESP32 GPIO |
| --- | --- |
| Green LED | GPIO14 |
| Blue LED | GPIO15 |
| SW1 | GPIO42 |
| SW2 | GPIO41 |
| ADS1294 MOSI | GPIO36 |
| ADS1294 MISO | GPIO17 |
| ADS1294 SCLK | GPIO18 |
| ADS1294 CS | GPIO21 |
| ADS1294 DRDY | GPIO16 |
| ADS1294 PWDN | GPIO35 |
| ADS1294 RESET | GPIO34 |
| ADS1294 START | GPIO33 |
| I2S DATA | GPIO3 |
| I2S BCLK | GPIO4 |
| I2S WS | GPIO5 |
| microSD CS | GPIO39 |
| microSD MOSI | GPIO38 |
| microSD SCLK | GPIO37 |

Important microSD warning:

```text
SPI2_MISO is currently invalid in the HSI because it maps to VSYS.
Do not enable microSD firmware until the PCB routing is fixed.
```

## App Source Structure

The app is currently implemented in one file:

```text
lib/main.dart
```

Important classes:

| Class | Firmware relevance |
| --- | --- |
| `DeviceApi` | Defines the HTTP calls sent to ESP32 firmware |
| `DeviceSnapshot` | Defines the JSON status model expected from firmware |
| `DeviceControllerScreen` | Owns app state, connection state, polling, and command log |
| `_LedPanel` | Sends LED commands |
| `_RecordingPanel` | Sends acquisition mode commands |
| `_HardwareView` | Displays pin mapping from the HSI |

## HTTP Client Behavior

The app uses the Flutter `http` package.

Configured dependency:

```yaml
http: ^1.2.2
```

Timeout behavior:

- `GET /api/status`: 3 second timeout.
- `POST` commands: 3 second timeout.

If the ESP32 does not respond, the app switches to demo mode and logs the failure.

## Polling

After a successful connection, the app polls:

```text
GET /api/status
```

Polling interval:

```text
4 seconds
```

Firmware should keep `/api/status` lightweight and non-blocking.

## Firmware Implementation Notes

Recommended firmware behavior:

1. Start Wi-Fi AP mode or connect to local Wi-Fi.
2. Start an HTTP server.
3. Return JSON with `Content-Type: application/json`.
4. Keep LED state in firmware so `/api/status` reflects the last command.
5. Read SW1/SW2 as active-low inputs.
6. Use safe defaults on boot:
   - LEDs off.
   - recording mode `idle`.
   - ADS1294 held in a safe inactive state until explicitly started.
7. Do not expose ECG human-contact workflows until safety validation is complete.
8. Do not enable microSD until `SPI2_MISO` is fixed.

## Suggested ESP32 API Test Commands

From a PC connected to the ESP32 network:

```powershell
curl http://192.168.4.1/api/status
```

```powershell
curl -X POST http://192.168.4.1/api/led `
  -H "Content-Type: application/json" `
  -d "{\"led\":\"green\",\"enabled\":true}"
```

```powershell
curl -X POST http://192.168.4.1/api/recording `
  -H "Content-Type: application/json" `
  -d "{\"mode\":\"ecg\"}"
```

```powershell
curl -X POST http://192.168.4.1/api/self-test `
  -H "Content-Type: application/json" `
  -d "{}"
```

## Current App Verification

The mobile app has been verified with:

```powershell
flutter analyze
flutter test
flutter build apk --debug
```

The app has also been installed and launched on the connected Android phone.

## Next Firmware Milestone

The fastest useful firmware milestone is:

1. ESP32 starts Wi-Fi AP at `192.168.4.1`.
2. Firmware implements `GET /api/status`.
3. Firmware implements `POST /api/led`.
4. Confirm the app can toggle GPIO14 and GPIO15 LEDs from the phone.

After that, add:

1. SW1/SW2 status.
2. Self-test endpoint.
3. ECG acquisition mode.
4. Microphone acquisition mode.
