# MotemaSens Mobile

Flutter mobile controller for the MotemaSens ESP32-S3 hardware.

## Current Features

- ESP32 base URL connection panel.
- Demo/offline mode when firmware is not reachable.
- Green LED control for GPIO14.
- Blue LED control for GPIO15.
- SW1/SW2 status display for GPIO42/GPIO41.
- ECG acquisition mode control.
- I2S microphone acquisition mode control.
- Device health panel with battery, signal, sample rate, and microSD state.
- Hardware pin reference from the repo HSI.
- Command/event log.
- Safety reminder for ECG bring-up.

## Expected Firmware HTTP Endpoints

The app is ready to talk to firmware using these endpoints:

```text
GET  /api/status
POST /api/led
POST /api/recording
POST /api/self-test
```

Example `GET /api/status` response:

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

Example `POST /api/led` body:

```json
{
  "led": "green",
  "enabled": true
}
```

Example `POST /api/recording` body:

```json
{
  "mode": "ecg"
}
```

## Run On Phone

```powershell
flutter pub get
flutter run -d RFCY508DWBP
```

## Test

```powershell
flutter test
```

## Build APK

```powershell
flutter build apk --debug
```
