# Phase 1: App Entry And Local IP Control

Checked on: 2026-06-14

## Goal

Create the first app navigation structure and keep Local IP control working with the current ESP32 HTTP API.

At the end of this phase, the app should open to a connection chooser with:

```text
Remote
Local
BLE
Configure Device
```

Only `Local` must be fully functional in this phase. The other options can show placeholder screens that say they are planned for later phases.

## Why This Phase Comes First

The current MotemaSens Flutter app already supports local ESP32 HTTP control. This phase keeps that working while preparing the app for Remote, BLE, and Configure Device flows.

## Mobile App Scope

### Screens To Create

```text
ConnectionChooserScreen
LocalConnectScreen
LocalDeviceDashboardScreen
RemotePlaceholderScreen
BlePlaceholderScreen
ConfigureDevicePlaceholderScreen
```

### Opening Screen

The app must open to:

```text
Connect To MotemaSens
```

With four options:

```text
Remote
Local
BLE
Configure Device
```

### Local Flow

```text
App opens
  -> Connection chooser
  -> Local
  -> Local connect screen
  -> Local device dashboard
```

### Local Connect Screen

Fields:

```text
ESP32 base URL
Connect button
```

Default value:

```text
http://192.168.4.1
```

### Local Device Dashboard

Move the current device control experience behind the Local option.

The dashboard should continue to support:

- `GET /api/status`
- `POST /api/led`
- `POST /api/recording`
- `POST /api/self-test`
- Demo/offline fallback when the ESP32 is unreachable
- Device health panel
- Recording mode controls
- LED controls
- Command/event log

### Mobile Developer Tasks

1. Add `ConnectionChooserScreen` as the app start screen.
2. Add four connection options: `Remote`, `Local`, `BLE`, and `Configure Device`.
3. Move existing ESP32 base URL connection flow under `Local`.
4. Keep the current local HTTP behavior working.
5. Keep the existing demo/offline fallback behavior.
6. Add placeholder screens for Remote, BLE, and Configure Device.
7. Add connection mode state with at least:

```text
demo
local
remote
ble
configureDevice
```

## Firmware Scope

The firmware must support the existing local HTTP API contract.

Required endpoints:

```text
GET  /api/status
POST /api/led
POST /api/recording
POST /api/self-test
```

Required `/api/status` response:

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

### Firmware Developer Tasks

1. Start ESP32 Wi-Fi AP mode at `192.168.4.1`, or support joining a local Wi-Fi network.
2. Start HTTP server.
3. Implement `GET /api/status`.
4. Implement `POST /api/led`.
5. Implement `POST /api/recording`.
6. Implement `POST /api/self-test`.
7. Keep `/api/status` lightweight and non-blocking.
8. Use safe defaults on boot:

```text
LEDs off
recordingMode = idle
ADS1294 inactive until explicitly started
```

## Backend Scope

No backend work is required in Phase 1.

Remote mode can show a placeholder.

## Acceptance Criteria

Phase 1 is complete when:

- The app opens to the connection chooser.
- Tapping `Local` opens the local connection flow.
- The default local URL is `http://192.168.4.1`.
- The app can connect to the ESP32 over HTTP.
- The app can read status.
- The app can toggle green and blue LEDs.
- The app can change recording mode.
- The app can run self-test.
- If ESP32 is unreachable, demo/offline mode still works.
- Remote, BLE, and Configure Device options exist, even if they are placeholders.

## Out Of Scope

- BLE control.
- Wi-Fi provisioning.
- SD file browser.
- SD download.
- MQTT.
- VPS login.
- Remote device list.
- Cloud dashboard.

