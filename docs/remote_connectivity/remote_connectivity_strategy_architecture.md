# MotemaSens Remote Connectivity Strategy And Architecture

Checked on: 2026-06-14

## Purpose

This document describes the recommended connectivity architecture for MotemaSens.

The goal is to support:

- Simple local control when no Wi-Fi is available.
- Live nearby viewing from the mobile app.
- Offline recording to microSD.
- Automatic upload of stored data when internet returns.
- Remote storage and access through a Hostinger VPS backend.

## Recommended Approach

Use a hybrid connectivity model:

```text
BLE
  Simple local controls and Wi-Fi setup

ESP32 local HTTP over Wi-Fi/AP
  Live viewing, nearby control, SD browsing, SD download

MQTT
  Device status, online/offline state, lightweight telemetry, sync coordination

HTTPS API
  Reliable upload of larger stored data chunks from microSD to VPS

microSD
  Offline recording buffer and local source of truth
```

Do not make the ESP32 connect directly to the database. The ESP32 should connect to a backend API or MQTT broker on the VPS. The backend then validates and writes data into the database.

## Developer Summary

This section lists the concrete changes a firmware developer, mobile app developer, and backend developer should make.

### Firmware Changes Required

| Area | Change | Priority |
| --- | --- | --- |
| Local HTTP | Keep and complete the current `/api/status`, `/api/led`, `/api/recording`, and `/api/self-test` endpoints | Phase 1 |
| BLE | Add BLE service for simple control, status, and Wi-Fi setup | Phase 2 |
| Wi-Fi mode | Support both ESP32 AP mode and station mode using saved Wi-Fi credentials | Phase 2 |
| Recording state | Keep one shared internal device state used by HTTP, BLE, MQTT, and SD sync | Phase 2 |
| microSD | After hardware fix, write recording data into chunk files with metadata | Phase 3 |
| Local SD API | Add HTTP endpoints to list and download SD files/chunks | Phase 3 |
| MQTT | Publish heartbeat, status, recording mode, SD state, and sync progress | Phase 5 |
| HTTPS upload | Upload pending SD chunks to the VPS API and mark uploaded only after server ACK | Phase 6 |
| Retry logic | Retry failed MQTT/API connections without blocking recording | Phase 6 |
| Security | Store unique device ID and device secret/certificate | Phase 6 |

### Mobile App Changes Required

| Area | Change | Priority |
| --- | --- | --- |
| Current local HTTP | Keep current ESP32 base URL flow and endpoints | Phase 1 |
| BLE scan | Add screen/control path to find nearby MotemaSens devices over BLE | Phase 2 |
| BLE controls | Add start/stop, mode selection, status read, and self-test over BLE | Phase 2 |
| Wi-Fi setup | Add BLE-based Wi-Fi credential provisioning screen | Phase 2 |
| Connection mode UI | Show whether the app is using cloud, local HTTP, BLE, or demo mode | Phase 2 |
| SD browser | Add local HTTP screen to list SD sessions/chunks/files | Phase 3 |
| SD download | Add local HTTP file/chunk download to phone | Phase 3 |
| Cloud API | Add login/device list/latest status/uploaded sessions from VPS | Phase 7 |
| Sync status | Show unsynced chunk count and latest upload status | Phase 7 |

### Backend/VPS Changes Required

| Area | Change | Priority |
| --- | --- | --- |
| API server | Create HTTPS API for device registration, status, chunk upload, and mobile reads | Phase 4 |
| Database | Store devices, sessions, chunks, latest status, and upload state | Phase 4 |
| MQTT broker | Run MQTT broker on the VPS | Phase 4 |
| MQTT worker | Subscribe to device topics and write validated messages to DB | Phase 5 |
| Chunk validation | Validate device auth, session ID, chunk ID, byte length, checksum, and duplicates | Phase 6 |
| Mobile auth | Add user login/token flow for mobile cloud access | Phase 7 |

## Implementation Contract

Developers should treat the ESP32 as having one internal state model. Every connection method should read or update the same state.

Example internal device state:

```json
{
  "deviceId": "ms-0001",
  "greenLed": false,
  "blueLed": false,
  "batteryVolts": 4.08,
  "signalQuality": 92,
  "sampleRate": 500,
  "sdReady": true,
  "recordingMode": "idle",
  "wifiConnected": true,
  "mqttConnected": true,
  "serverReachable": true,
  "unsyncedChunks": 0
}
```

The same values should be visible through:

- Local HTTP `/api/status`
- BLE status characteristic
- MQTT status payload
- Mobile app status panels

This avoids separate behavior for BLE, local HTTP, and cloud sync.

## High-Level Architecture

```text
                    Internet available
                 +----------------------+
                 |                      |
                 v                      |
+---------+   MQTT/HTTPS        +----------------+
| ESP32   | ------------------> | Hostinger VPS  |
| Device  |                     | API + MQTT     |
+---------+                     | DB writer      |
    |                           +----------------+
    |
    | Local Wi-Fi/AP HTTP
    v
+--------------+
| Mobile App   |
| Flutter      |
+--------------+
    ^
    |
    | BLE fallback control
    |
+---------+
| ESP32   |
+---------+

+---------+
| microSD |
| Offline |
| chunks  |
+---------+
```

## Connectivity Modes

### 1. BLE Fallback Control

BLE should be used when there is no Wi-Fi available or when the user only needs simple controls.

Good BLE use cases:

| Feature | Use BLE? | Notes |
| --- | --- | --- |
| Start recording | Yes | Small command payload |
| Stop recording | Yes | Small command payload |
| Select mode: idle, ECG, microphone | Yes | Simple state change |
| Read battery level | Yes | Lightweight status |
| Read recording state | Yes | Lightweight status |
| Read SD ready/free space state | Yes | Lightweight status |
| Run self-test | Yes | Simple command |
| Configure Wi-Fi credentials | Yes | Common ESP32 provisioning workflow |
| Download SD files | No | Use local HTTP instead |
| Upload backlog to server | No | Use Wi-Fi with MQTT/HTTPS |
| High-rate ECG/audio streaming | Avoid | BLE can be too slow and fragile for this |

BLE should be treated as a control and setup channel, not the main data channel.

Suggested BLE services:

| Service | Purpose |
| --- | --- |
| Device Status Service | Battery, recording mode, SD state, signal quality |
| Device Control Service | Start/stop recording, mode changes, self-test |
| Wi-Fi Provisioning Service | Configure SSID/password and request reconnect |

Firmware developer tasks:

1. Add BLE advertising name such as `MotemaSens-ms-0001`.
2. Add a status characteristic that the mobile app can read.
3. Add a control characteristic that the mobile app can write commands to.
4. Add a Wi-Fi provisioning characteristic for SSID/password setup.
5. Return command acknowledgements so the app knows whether the command succeeded.
6. Keep BLE active when possible, even when Wi-Fi is unavailable.

Mobile developer tasks:

1. Add BLE permission handling for Android.
2. Add BLE scan and connect screen.
3. Filter devices by MotemaSens service UUID or advertising name.
4. Show a simple BLE control panel after connection.
5. Send JSON commands through the control characteristic.
6. Read status and update the existing device status UI.
7. Add Wi-Fi setup form that sends SSID/password to the ESP32 over BLE.

Suggested BLE commands:

```json
{
  "command": "set_recording_mode",
  "mode": "ecg"
}
```

```json
{
  "command": "stop_recording"
}
```

```json
{
  "command": "self_test"
}
```

### 2. Local Wi-Fi / ESP32 Access Point HTTP

The current Flutter app already expects the ESP32 to expose a local HTTP API.

Current default base URL:

```text
http://192.168.4.1
```

This is suitable when the ESP32 runs as a Wi-Fi access point.

The existing local HTTP path should remain the primary method for:

- Live nearby viewing.
- Nearby device control.
- Reading detailed status.
- Browsing available SD files.
- Downloading SD files to the mobile phone.
- Development and field debugging.

Existing endpoints from the current app:

```text
GET  /api/status
POST /api/led
POST /api/recording
POST /api/self-test
```

Recommended future local endpoints:

```text
GET  /api/files
GET  /api/files/{fileName}
GET  /api/chunks
GET  /api/chunks/{chunkId}
POST /api/sync/retry
GET  /api/network
POST /api/network/wifi
```

Firmware developer tasks:

1. Keep the current local HTTP API working exactly as the mobile app expects.
2. Add `/api/files` after SD recording is available.
3. Add file/chunk download endpoints with correct content length.
4. Make downloads resumable later if files become large.
5. Keep `/api/status` fast and non-blocking.
6. Never block recording while serving HTTP requests.

Mobile developer tasks:

1. Keep the current ESP32 base URL connection field.
2. Add a connection mode indicator: demo, BLE, local HTTP, or cloud.
3. Add an SD browser screen using `/api/files` or `/api/chunks`.
4. Add download progress and failure handling.
5. Do not require cloud login for local HTTP use.

Local HTTP is better than BLE for SD download because it is faster, easier to resume, and simpler for file transfer.

### 3. VPS Remote Connection

The Hostinger VPS should run server-side services between ESP32 devices and the database.

Recommended VPS components:

| Component | Responsibility |
| --- | --- |
| MQTT broker | Receives lightweight device messages |
| Backend API | Receives HTTPS chunk uploads and serves mobile/cloud requests |
| Worker service | Validates MQTT/API payloads and writes to DB |
| Database | Stores devices, sessions, chunks, samples, sync state |
| Web/admin dashboard | Optional remote monitoring and support |

The ESP32 should not connect directly to MySQL/PostgreSQL. Direct database access from firmware creates security and reliability problems.

Preferred remote paths:

```text
ESP32 -> MQTT broker -> backend worker -> database
ESP32 -> HTTPS API -> backend API -> database/object storage
Mobile app -> HTTPS API -> database
```

Backend developer tasks:

1. Create database tables for devices, sessions, chunks, and latest device status.
2. Create device authentication for ESP32 uploads.
3. Create mobile user authentication for cloud access.
4. Create MQTT topic permissions per device.
5. Create API endpoint for chunk upload.
6. Create API endpoints for mobile app history/status reads.
7. Add duplicate chunk protection using `deviceId`, `sessionId`, and `chunkId`.
8. Add checksum validation before marking chunks accepted.

## MQTT Role

MQTT is recommended for lightweight IoT communication.

Use MQTT for:

- Device online/offline heartbeat.
- Battery level.
- Signal quality.
- Current recording mode.
- SD card status.
- Upload progress.
- Small live telemetry previews.
- Notifications that a new SD chunk is ready.

Suggested MQTT topics:

```text
motemasens/{deviceId}/status
motemasens/{deviceId}/heartbeat
motemasens/{deviceId}/recording
motemasens/{deviceId}/sync/progress
motemasens/{deviceId}/sync/chunk_ready
motemasens/{deviceId}/events
```

Example status payload:

```json
{
  "deviceId": "ms-0001",
  "batteryVolts": 4.08,
  "signalQuality": 92,
  "sampleRate": 500,
  "sdReady": true,
  "recordingMode": "ecg",
  "wifiRssi": -61,
  "unsyncedChunks": 12,
  "timestampUtc": "2026-06-14T02:30:00Z"
}
```

Use MQTT QoS 1 for important status and sync messages so the broker acknowledges delivery.

MQTT should not be the only mechanism for large file or chunk transfer. It can carry small chunks if needed, but HTTPS upload is usually simpler and more reliable for larger ECG/audio data.

Firmware developer tasks:

1. Connect to MQTT only after Wi-Fi station mode has internet.
2. Publish heartbeat at a fixed interval, for example every 30 seconds.
3. Publish status when important values change.
4. Publish `chunk_ready` when a new SD chunk is ready for upload.
5. Publish sync progress while uploading backlog.
6. Reconnect MQTT automatically after network loss.

Backend developer tasks:

1. Subscribe to `motemasens/+/status`.
2. Subscribe to `motemasens/+/heartbeat`.
3. Subscribe to `motemasens/+/sync/+`.
4. Validate device identity before writing MQTT data to DB.
5. Store latest status separately from historical session data.

## HTTPS Upload Role

Use HTTPS API upload for stored data chunks from microSD.

Recommended upload flow:

1. ESP32 records sensor data to a local chunk file on microSD.
2. ESP32 writes chunk metadata locally.
3. ESP32 publishes `chunk_ready` over MQTT when online.
4. ESP32 uploads the chunk to the VPS API over HTTPS.
5. VPS validates device ID, session ID, chunk ID, checksum, and timestamp range.
6. VPS stores the data and returns an acknowledgement.
7. ESP32 marks the chunk as uploaded only after successful acknowledgement.
8. ESP32 retries failed chunks later.

Suggested upload endpoint:

```text
POST /api/devices/{deviceId}/sessions/{sessionId}/chunks
```

Suggested metadata:

```json
{
  "deviceId": "ms-0001",
  "sessionId": "2026-06-14-ecg-001",
  "chunkId": 42,
  "recordingType": "ecg",
  "sampleRate": 500,
  "startedAtUtc": "2026-06-14T02:30:00Z",
  "endedAtUtc": "2026-06-14T02:31:00Z",
  "byteLength": 120000,
  "sha256": "..."
}
```

The chunk body can be JSON, binary, or compressed binary. For ECG/audio data, binary or compressed binary is preferred to reduce bandwidth and storage.

Firmware developer tasks:

1. Scan SD for chunks with `pending` or `failed` state.
2. Upload oldest chunks first.
3. Include metadata with every upload.
4. Include checksum with every upload.
5. Wait for server acknowledgement.
6. Mark chunk `uploaded` only after valid server acknowledgement.
7. Leave failed chunks on SD and retry later.

Backend developer tasks:

1. Reject uploads from unknown devices.
2. Reject chunks with invalid checksum.
3. Accept duplicate chunk uploads safely without duplicating data.
4. Return a clear acknowledgement payload.
5. Store upload errors for debugging.

Suggested successful upload response:

```json
{
  "ok": true,
  "deviceId": "ms-0001",
  "sessionId": "2026-06-14-ecg-001",
  "chunkId": 42,
  "status": "accepted"
}
```

## microSD Offline Buffer

The microSD card should be the offline storage buffer.

Expected behavior:

1. If internet is available, the ESP32 can record to SD and upload in parallel.
2. If internet is not available, the ESP32 records only to SD.
3. When internet returns, the ESP32 scans for unuploaded chunks.
4. The ESP32 uploads the oldest unuploaded chunks first.
5. The ESP32 marks chunks as uploaded after server acknowledgement.
6. Uploaded chunks can be retained for a configurable time or until storage is low.

Recommended SD structure:

```text
/motemasens/
  device.json
  sessions/
    2026-06-14-ecg-001/
      session.json
      chunks/
        000001.bin
        000001.json
        000002.bin
        000002.json
      sync.json
```

Recommended sync states:

| State | Meaning |
| --- | --- |
| pending | Chunk exists locally and has not been uploaded |
| uploading | Upload is in progress |
| uploaded | Server acknowledged the chunk |
| failed | Last upload failed; retry later |

Important project note:

```text
The current HSI says SPI2_MISO is incorrectly routed to VSYS.
Do not enable microSD firmware until the hardware routing is fixed.
```

Firmware developer tasks:

1. Confirm SD hardware routing is fixed before enabling SD code.
2. Mount SD on boot and expose `sdReady` through status.
3. Create a session folder when recording starts.
4. Write data in numbered chunks.
5. Write metadata next to each chunk.
6. Update `sync.json` after each upload acknowledgement.
7. Handle low storage without corrupting active recording.
8. Keep recording active even if server upload fails.

## Mobile App Role

The Flutter mobile app should support three connection paths:

| Path | Purpose |
| --- | --- |
| BLE | Simple control and Wi-Fi setup |
| Local HTTP | Live nearby viewing, SD browsing, SD download |
| Cloud API | Remote history, synced data, remote dashboard |

## Mobile App Connection Strategy

When the MotemaSens app opens, the first screen should only ask the user how they want to connect.

First screen title:

```text
Connect To MotemaSens
```

First screen options:

```text
Remote
Local
BLE
Configure Device
```

The selected connection mode decides the next screen and which features are available.

```text
App opens
  |
  v
Connection chooser
  |
  +-- Remote -> Login -> Device list -> Remote device dashboard
  |
  +-- Local  -> Local IP connect -> Local device dashboard
  |
  +-- BLE    -> BLE scan/connect -> BLE control dashboard
  |
  +-- Configure Device -> Developer password -> BLE scan/connect -> Wi-Fi setup
```

### Developer Configure Device Option

The app should include a developer-only option called `Configure Device`.

This mode is used when setting up a new ESP32 device or changing the Wi-Fi network saved on the device.

Temporary developer password:

```text
1234
```

Important:

```text
This password is only for early development.
Before production, replace it with proper authentication or remove the developer shortcut.
```

User flow:

```text
Connection chooser
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

Configure Device screen responsibilities:

| Screen | Purpose |
| --- | --- |
| Developer password screen | Blocks casual users from provisioning tools |
| BLE scan screen | Finds nearby MotemaSens devices |
| Wi-Fi setup screen | Collects SSID/password and sends them to ESP32 |
| Provisioning result screen | Shows whether ESP32 connected successfully |

Wi-Fi setup fields:

```text
Wi-Fi SSID
Wi-Fi password
Save and connect button
```

Recommended BLE provisioning command:

```json
{
  "command": "configure_wifi",
  "ssid": "Clinic-WiFi",
  "password": "example-password"
}
```

Recommended BLE provisioning response:

```json
{
  "ok": true,
  "message": "Wi-Fi credentials saved. Connecting...",
  "wifiConnected": false
}
```

After the ESP32 attempts connection, the app should read status again:

```json
{
  "wifiConnected": true,
  "ipAddress": "192.168.1.42",
  "serverReachable": true,
  "mqttConnected": true
}
```

Mobile developer tasks for Configure Device mode:

1. Add `Configure Device` as a fourth option on the first screen.
2. Add developer password screen.
3. Use temporary password `1234` during development.
4. After correct password, open BLE scan flow.
5. Connect to selected MotemaSens device over BLE.
6. Show Wi-Fi SSID/password form.
7. Send credentials through the BLE Wi-Fi provisioning characteristic.
8. Show connection result, including assigned local IP address if available.
9. Offer a button to continue to Local mode using the new IP address.

Firmware developer tasks for Configure Device mode:

1. Add BLE Wi-Fi provisioning characteristic.
2. Accept SSID/password payload.
3. Save credentials securely in ESP32 non-volatile storage.
4. Attempt Wi-Fi station connection.
5. Report Wi-Fi connection status over BLE.
6. Include assigned local IP address in BLE status after connection.
7. Start MQTT/server sync if internet is reachable.

Recommended behavior after successful Wi-Fi setup:

```text
If Wi-Fi connects:
  Show local IP address.
  Allow user to open Local dashboard.
  ESP32 starts MQTT/HTTPS remote sync if server is reachable.

If Wi-Fi fails:
  Show error.
  Keep BLE connected.
  Allow user to edit SSID/password and retry.
```

### Connection Option 1: Remote

Remote mode is for connecting through the Hostinger VPS.

User flow:

```text
Connection chooser
  -> Remote
  -> Username/password login
  -> List devices available to that user
  -> Select device
  -> Remote dashboard
```

Remote screen responsibilities:

| Screen | Purpose |
| --- | --- |
| Remote login | User enters username/password |
| Device list | Shows devices assigned to the logged-in user |
| Remote dashboard | Shows live status, remote controls, uploaded sessions, sync state |

Remote login fields:

```text
Username or email
Password
Login button
```

Remote device list should show:

| Field | Example |
| --- | --- |
| Device name | MotemaSens 001 |
| Device ID | ms-0001 |
| Online state | Online / Offline |
| Last seen | 2 minutes ago |
| Battery | 84% or 4.08 V |
| Recording state | Idle / ECG / Microphone |
| Unsynced chunks | 12 |

Remote dashboard should show:

- Latest device status from server.
- Online/offline state.
- Battery/system voltage.
- Signal quality.
- Sample rate.
- Recording mode.
- SD state.
- Upload/sync progress.
- Last uploaded session.
- Available uploaded sessions.
- Remote commands supported by the server.

Remote control limitations:

Remote commands depend on the ESP32 being online through MQTT or a server command channel. If the ESP32 is offline, the app can show history and last known status, but live commands cannot execute immediately.

Recommended remote commands:

| Command | Remote support | Notes |
| --- | --- | --- |
| Start ECG recording | Yes, if device online | Send via MQTT command topic or server queue |
| Start microphone recording | Yes, if device online | Send via MQTT command topic or server queue |
| Stop recording | Yes, if device online | Important command |
| Run self-test | Yes, if device online | Useful for support |
| Toggle LEDs | Optional | Mainly for test/debug |
| Download SD files directly | No | Use Local mode for direct SD download |
| View uploaded sessions | Yes | From VPS database/storage |

Recommended remote command path:

```text
Mobile app -> VPS API -> MQTT command topic -> ESP32 -> MQTT ack/status -> VPS -> Mobile app
```

Suggested MQTT command topics:

```text
motemasens/{deviceId}/commands
motemasens/{deviceId}/commands/ack
```

Example remote command payload:

```json
{
  "commandId": "cmd-20260614-0001",
  "command": "set_recording_mode",
  "mode": "ecg",
  "requestedBy": "user-001",
  "timestampUtc": "2026-06-14T02:30:00Z"
}
```

Example command acknowledgement:

```json
{
  "commandId": "cmd-20260614-0001",
  "ok": true,
  "deviceId": "ms-0001",
  "recordingMode": "ecg",
  "timestampUtc": "2026-06-14T02:30:02Z"
}
```

Mobile developer tasks for Remote mode:

1. Create first-screen connection chooser.
2. Add Remote login screen.
3. Store auth token securely after login.
4. Fetch available device list from VPS API.
5. Add remote device selection screen.
6. Add remote dashboard screen.
7. Show remote command buttons only when device is online.
8. Show last known status clearly when device is offline.
9. Add uploaded sessions/history view.

Backend developer tasks for Remote mode:

1. Add user login API.
2. Add user-to-device authorization.
3. Add device list API for logged-in users.
4. Add latest device status API.
5. Add uploaded sessions API.
6. Add remote command API.
7. Forward remote commands to ESP32 through MQTT or a command queue.
8. Store command acknowledgements.

Suggested VPS API endpoints for Remote mode:

```text
POST /api/auth/login
GET  /api/devices
GET  /api/devices/{deviceId}/status
GET  /api/devices/{deviceId}/sessions
POST /api/devices/{deviceId}/commands
GET  /api/devices/{deviceId}/commands/{commandId}
```

### Connection Option 2: Local

Local mode is for connecting directly to the ESP32 by local IP address.

This is the closest match to the current Flutter app.

Critical Local mode performance rule:

```text
When the mobile app is connected through Local IP, the ESP32 device screen must only show a simple connected state such as:

"Connected to Mobile"

The ESP32 must not render plots, charts, waveforms, or other CPU-intensive live views on its own display during Local streaming.
All live plotting and visualization must be done by the mobile app.
```

Reason:

```text
Rendering plots on the ESP32 display while also acquiring, buffering, and streaming sensor data can reduce streaming performance and increase the chance of dropped samples.
The ESP32 should prioritize acquisition, SD writes, local API responses, and data streaming.
```

User flow:

```text
Connection chooser
  -> Local
  -> Local device connect screen
  -> Local dashboard
```

Local connect screen fields:

```text
ESP32 base URL
Connect button
```

Default value:

```text
http://192.168.4.1
```

Local dashboard should show:

- Live status from `/api/status`.
- Live stream visualization and plotting on the mobile app.
- Green/blue LED test controls.
- SW1/SW2 state.
- Battery/system voltage.
- Signal quality.
- Sample rate.
- SD ready state.
- Recording mode controls.
- Self-test button.
- Local event log.
- SD file/session browser after SD support is available.
- SD download after SD support is available.

Local mode should not require login because it is intended for nearby device access.

Recommended local HTTP endpoints:

```text
GET  /api/status
POST /api/led
POST /api/recording
POST /api/self-test
GET  /api/files
GET  /api/files/{fileName}
GET  /api/chunks
GET  /api/chunks/{chunkId}
```

Firmware developer tasks for Local mode:

1. Keep the current HTTP API contract stable.
2. Add SD list/download endpoints after SD hardware fix.
3. Keep local API fast enough for live viewing.
4. When a mobile app is connected locally, change the ESP32 display to a minimal `Connected to Mobile` state.
5. Disable ESP32-side live plotting/waveform rendering during Local streaming.
6. Stream raw or lightly packed sensor data to the mobile app for plotting.
7. Prioritize acquisition, buffering, SD writes, and streaming over display updates.

Mobile developer tasks for Local mode:

1. Move the current device control screen behind the Local option.
2. Keep the existing ESP32 base URL field.
3. Keep existing demo/offline fallback behavior.
4. Add SD browser/download UI after firmware supports SD endpoints.
5. Show connection mode as `Local`.
6. Do all live plotting and waveform visualization inside the mobile app.
7. Avoid asking the ESP32 to render charts while Local streaming is active.

### Connection Option 3: BLE

BLE mode is for simple nearby control when Wi-Fi is not available.

User flow:

```text
Connection chooser
  -> BLE
  -> Bluetooth permission/check screen
  -> Scan for MotemaSens devices
  -> Select nearby device
  -> BLE control dashboard
```

BLE connection screen should show:

- Bluetooth enabled/disabled state.
- Required permissions.
- Scan button.
- Nearby MotemaSens devices.
- Connect button.

BLE dashboard should show only features that BLE can reliably support:

| Feature | Show In BLE Dashboard? | Notes |
| --- | --- | --- |
| Battery/status | Yes | Read characteristic |
| Recording mode | Yes | Read/write command |
| Start ECG | Yes | Control command |
| Start microphone | Yes | Control command |
| Stop recording | Yes | Control command |
| Self-test | Yes | Control command |
| Wi-Fi setup | Yes | Provisioning form |
| SD ready/free space | Yes | Status only |
| SD file download | No | Use Local mode |
| Full live ECG/audio stream | Avoid | BLE is not the main data channel |
| Uploaded cloud history | No | Use Remote mode |

Recommended BLE dashboard controls:

```text
Status card
Recording mode selector
Start/stop controls
Self-test button
Wi-Fi setup button
Connection log
```

Mobile developer tasks for BLE mode:

1. Add Android Bluetooth permissions.
2. Add BLE scan and connect flow.
3. Filter for MotemaSens devices.
4. Add BLE dashboard with limited controls.
5. Add Wi-Fi provisioning form.
6. Handle disconnect/reconnect cleanly.
7. Show connection mode as `BLE`.

Firmware developer tasks for BLE mode:

1. Advertise MotemaSens BLE service.
2. Expose status read characteristic.
3. Expose command write characteristic.
4. Expose Wi-Fi provisioning characteristic.
5. Send command acknowledgement or updated status after each command.
6. Keep BLE commands mapped to the same internal state used by HTTP and MQTT.

## Mobile App Screen Architecture

Recommended screen structure:

```text
ConnectionChooserScreen
DeveloperPasswordScreen
DeviceProvisioningBleScanScreen
WifiProvisioningScreen
ProvisioningResultScreen
RemoteLoginScreen
RemoteDeviceListScreen
RemoteDeviceDashboardScreen
LocalConnectScreen
LocalDeviceDashboardScreen
BleScanScreen
BleDeviceDashboardScreen
```

Recommended shared app services:

| Service | Purpose |
| --- | --- |
| RemoteApiClient | Talks to VPS API |
| LocalDeviceApi | Talks to ESP32 local HTTP API |
| BleDeviceClient | Talks to ESP32 BLE services |
| DeviceStateModel | Shared app-side state object |
| ConnectionModeController | Tracks selected connection mode |
| DeviceProvisioningController | Handles developer password and BLE Wi-Fi setup |

The current `DeviceApi` in `lib/main.dart` maps well to `LocalDeviceApi`. Remote and BLE should be added as separate clients instead of mixing all connection logic into one class.

Recommended app state model:

```text
ConnectionMode:
  demo
  remote
  local
  ble

DeviceState:
  deviceId
  connectionMode
  online
  batteryVolts
  signalQuality
  sampleRate
  sdReady
  recordingMode
  unsyncedChunks
  lastSeen
```

## Feature Availability By Connection Mode

| Feature | Remote | Local | BLE | Configure Device |
| --- | --- | --- | --- | --- |
| Login required | Yes | No | No | Developer password |
| Device list | Yes | No | Nearby scan only | Nearby BLE scan |
| Live status | Yes, if device online | Yes | Basic only | Provisioning status only |
| Start/stop recording | Yes, if device online | Yes | Yes | No |
| Change recording mode | Yes, if device online | Yes | Yes | No |
| Self-test | Yes, if device online | Yes | Yes | Optional |
| LED test | Optional | Yes | Optional | Optional |
| SD file browser | Uploaded files only | Yes | No | No |
| SD direct download | No | Yes | No | No |
| Wi-Fi setup | Optional | Optional | Yes | Yes |
| Uploaded session history | Yes | No | No | No |
| Works with no Wi-Fi | No | Only if ESP32 AP is available | Yes | Yes |

Recommended app behavior:

1. Try cloud login/API for remote history if internet is available.
2. Allow direct ESP32 local connection by IP address or default AP address.
3. Allow BLE scan for nearby MotemaSens devices.
4. If connected over BLE, show simple controls only.
5. If connected over local HTTP, show full live controls and SD tools.
6. If connected to cloud, show uploaded sessions and remote device status.

## Device Connection Priority

Recommended device-side priority:

1. Continue recording safely if recording is active.
2. Keep writing to SD when enabled.
3. Maintain BLE control availability.
4. Connect to configured Wi-Fi if available.
5. If internet is available, connect MQTT.
6. Upload pending SD chunks over HTTPS.
7. If configured Wi-Fi is unavailable, optionally start ESP32 AP mode for local app access.

Recommended mobile-side priority:

1. Cloud API for remote history.
2. Local HTTP for nearby live view and SD download.
3. BLE for simple control/setup when Wi-Fi is unavailable.

## Security Requirements

Minimum security recommendations:

- Use HTTPS for all VPS API calls.
- Use MQTT over TLS if possible.
- Give each ESP32 a unique device ID.
- Give each ESP32 a unique device secret or certificate.
- Never embed database credentials in firmware.
- Rotate device credentials if a device is lost.
- Require mobile user authentication for cloud access.
- Keep local HTTP limited to nearby/local network use.
- Add a simple pairing step for BLE control.

## Implementation Phases

### Phase 1: Keep Current Local HTTP Working

Firmware should first implement the current app contract:

```text
GET  /api/status
POST /api/led
POST /api/recording
POST /api/self-test
```

This gives a working baseline for mobile control and testing.

### Phase 2: Add BLE Simple Control

Add BLE for:

- Device discovery.
- Start/stop recording.
- Mode selection.
- Battery/status read.
- Wi-Fi setup.

The mobile app can show a limited BLE control screen when local HTTP is not available.

### Phase 3: Add microSD Recording

After the SD hardware routing issue is fixed:

- Write recordings into chunk files.
- Store chunk metadata.
- Track pending/uploaded state.
- Add local HTTP endpoints for SD file listing and download.

### Phase 4: Add VPS Backend

Deploy to Hostinger VPS:

- Backend API.
- Database.
- MQTT broker.
- Worker service for MQTT ingestion.

Start with status and heartbeat before adding chunk upload.

### Phase 5: Add MQTT Status

Firmware publishes:

- Heartbeat.
- Battery.
- Recording mode.
- Signal quality.
- SD state.
- Unsynced chunk count.

Server stores the latest status for the mobile app and dashboard.

### Phase 6: Add HTTPS Chunk Upload

Firmware uploads pending SD chunks to the VPS.

Server validates and acknowledges each chunk. Firmware only marks a chunk as uploaded after acknowledgement.

### Phase 7: Add Cloud View In Mobile App

Mobile app reads from the VPS API:

- Device list.
- Latest device status.
- Uploaded sessions.
- Downloadable/exportable data.

## Final Recommendation

The best MotemaSens architecture is:

```text
BLE for simple fallback control
Local HTTP for nearby live viewing and SD download
microSD for offline recording
MQTT for status and sync coordination
HTTPS API for reliable data chunk upload
VPS backend for database storage and remote access
```

This gives the device useful behavior in all conditions:

- No Wi-Fi: BLE control and SD recording.
- Nearby phone: local HTTP live view and SD download.
- Internet available: automatic sync to VPS.
- Remote user: cloud data and device status from server.
