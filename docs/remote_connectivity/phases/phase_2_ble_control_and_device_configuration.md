# Phase 2: BLE Control And Device Configuration

Checked on: 2026-06-14

## Goal

Add BLE support for simple nearby control and developer device configuration.

At the end of this phase:

- `BLE` option should connect to nearby MotemaSens devices and show simple controls.
- `Configure Device` option should allow a developer to connect over BLE and send Wi-Fi SSID/password to the ESP32.
- Developer password is temporarily `1234`.

## Why This Phase Comes Second

BLE gives a fallback control path when Wi-Fi is not available. It also provides the easiest way to configure Wi-Fi credentials on a new ESP32 device.

## Mobile App Scope

### Screens To Create

```text
BleScanScreen
BleDeviceDashboardScreen
DeveloperPasswordScreen
DeviceProvisioningBleScanScreen
WifiProvisioningScreen
ProvisioningResultScreen
```

### BLE User Control Flow

```text
App opens
  -> Connection chooser
  -> BLE
  -> Bluetooth permission/check screen
  -> Scan for MotemaSens devices
  -> Select device
  -> BLE control dashboard
```

### Configure Device Flow

```text
App opens
  -> Connection chooser
  -> Configure Device
  -> Enter developer password
  -> BLE scan for MotemaSens devices
  -> Select device
  -> Enter Wi-Fi SSID and password
  -> Send credentials to ESP32 over BLE
  -> ESP32 saves credentials
  -> ESP32 tries to connect to Wi-Fi
  -> App shows success/failure status
```

Temporary developer password:

```text
1234
```

Important:

```text
This password is only for development.
Replace it with proper authentication before production.
```

### BLE Dashboard Features

The BLE dashboard should support only lightweight controls:

- Read battery/status.
- Read recording mode.
- Start ECG recording.
- Start microphone recording.
- Stop recording.
- Run self-test.
- Show SD ready/free-space status if firmware exposes it.
- Show connection log.

Do not use BLE for:

- SD file download.
- Full live ECG/audio streaming.
- Uploaded cloud history.

### Wi-Fi Provisioning Fields

```text
Wi-Fi SSID
Wi-Fi password
Save and connect button
```

### Mobile Developer Tasks

1. Add Android Bluetooth permission handling.
2. Add BLE scan and connect flow.
3. Filter for MotemaSens devices by service UUID or advertising name.
4. Add BLE control dashboard.
5. Add developer password screen.
6. Accept temporary developer password `1234`.
7. Add BLE Wi-Fi provisioning form.
8. Send SSID/password to ESP32 over BLE.
9. Show provisioning result.
10. If ESP32 reports an IP address, offer a button to open Local mode using that IP.

## Firmware Scope

### BLE Services

Firmware should expose BLE for:

| Service | Purpose |
| --- | --- |
| Device Status Service | Battery, recording mode, SD state, Wi-Fi state |
| Device Control Service | Start/stop recording, mode changes, self-test |
| Wi-Fi Provisioning Service | Receive SSID/password and request reconnect |

### Suggested BLE Commands

Start ECG:

```json
{
  "command": "set_recording_mode",
  "mode": "ecg"
}
```

Stop recording:

```json
{
  "command": "stop_recording"
}
```

Configure Wi-Fi:

```json
{
  "command": "configure_wifi",
  "ssid": "Clinic-WiFi",
  "password": "example-password"
}
```

### Suggested Provisioning Response

```json
{
  "ok": true,
  "message": "Wi-Fi credentials saved. Connecting...",
  "wifiConnected": false
}
```

After connection attempt:

```json
{
  "wifiConnected": true,
  "ipAddress": "192.168.1.42",
  "serverReachable": false,
  "mqttConnected": false
}
```

### Firmware Developer Tasks

1. Advertise BLE name such as `MotemaSens-ms-0001`.
2. Add MotemaSens BLE service UUID.
3. Add status read characteristic.
4. Add command write characteristic.
5. Add Wi-Fi provisioning characteristic.
6. Save Wi-Fi credentials in ESP32 non-volatile storage.
7. Attempt Wi-Fi station connection after receiving credentials.
8. Report Wi-Fi connection result over BLE.
9. Include assigned local IP address in BLE status if connected.
10. Keep BLE commands mapped to the same internal state used by local HTTP.

## Backend Scope

No backend work is required in Phase 2.

## Acceptance Criteria

Phase 2 is complete when:

- Tapping `BLE` opens BLE scan.
- The app can discover MotemaSens ESP32 devices.
- The app can connect to a selected device over BLE.
- The app can read basic status over BLE.
- The app can start/stop recording over BLE.
- The app can run self-test over BLE.
- Tapping `Configure Device` asks for developer password.
- Password `1234` allows access to provisioning.
- Wrong password blocks provisioning.
- The app can send SSID/password to ESP32 over BLE.
- ESP32 saves Wi-Fi credentials.
- ESP32 attempts Wi-Fi connection.
- App shows success/failure and local IP address when available.

## Out Of Scope

- SD file download.
- Live waveform plotting over BLE.
- MQTT.
- VPS remote login.
- HTTPS chunk upload.

