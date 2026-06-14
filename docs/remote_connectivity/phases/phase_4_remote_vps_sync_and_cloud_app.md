# Phase 4: Remote VPS Sync And Cloud App

Checked on: 2026-06-14

## Goal

Add full remote connectivity through Hostinger VPS.

At the end of this phase:

- ESP32 publishes status to the VPS using MQTT.
- ESP32 uploads stored SD chunks to the VPS using HTTPS.
- VPS validates and stores data in the database.
- Mobile app supports Remote login.
- Mobile app shows devices available to the logged-in user.
- Mobile app opens a remote dashboard for selected devices.

## Remote User Flow

```text
App opens
  -> Connection chooser
  -> Remote
  -> Username/password login
  -> Device list
  -> Select device
  -> Remote dashboard
```

## Mobile App Scope

### Remote Screens

```text
RemoteLoginScreen
RemoteDeviceListScreen
RemoteDeviceDashboardScreen
RemoteSessionHistoryScreen
```

### Remote Login Fields

```text
Username or email
Password
Login button
```

### Remote Device List

Show:

| Field | Example |
| --- | --- |
| Device name | MotemaSens 001 |
| Device ID | ms-0001 |
| Online state | Online / Offline |
| Last seen | 2 minutes ago |
| Battery | 84% or 4.08 V |
| Recording state | Idle / ECG / Microphone |
| Unsynced chunks | 12 |

### Remote Dashboard

Show:

- Latest device status from VPS.
- Online/offline state.
- Battery/system voltage.
- Signal quality.
- Sample rate.
- Recording mode.
- SD state.
- Upload/sync progress.
- Last uploaded session.
- Available uploaded sessions.
- Remote command controls when device is online.

### Remote Command Rules

Remote commands can only execute immediately when the ESP32 is online through MQTT or the selected server command channel.

If the ESP32 is offline:

- Show last known status.
- Show uploaded history.
- Disable live command buttons or mark them as unavailable.

### Mobile Developer Tasks

1. Add Remote login screen.
2. Store auth token securely.
3. Fetch device list for logged-in user.
4. Add remote device dashboard.
5. Fetch latest device status.
6. Fetch uploaded sessions.
7. Send remote commands through VPS API.
8. Show command acknowledgement or failure.
9. Clearly show offline/last-seen state.

## Firmware Scope

### MQTT Status

Use MQTT for lightweight device status and sync coordination.

Suggested topics:

```text
motemasens/{deviceId}/status
motemasens/{deviceId}/heartbeat
motemasens/{deviceId}/recording
motemasens/{deviceId}/sync/progress
motemasens/{deviceId}/sync/chunk_ready
motemasens/{deviceId}/events
motemasens/{deviceId}/commands
motemasens/{deviceId}/commands/ack
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

### HTTPS Chunk Upload

Use HTTPS for stored chunk upload.

Suggested endpoint:

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

Successful upload response:

```json
{
  "ok": true,
  "deviceId": "ms-0001",
  "sessionId": "2026-06-14-ecg-001",
  "chunkId": 42,
  "status": "accepted"
}
```

### Firmware Developer Tasks

1. Connect to configured Wi-Fi in station mode.
2. Connect to MQTT broker after internet is available.
3. Publish heartbeat at fixed interval, for example every 30 seconds.
4. Publish status changes.
5. Publish `chunk_ready` when a new SD chunk is created.
6. Scan SD for `pending` or `failed` chunks.
7. Upload oldest chunks first using HTTPS.
8. Include metadata and checksum with each chunk.
9. Mark chunk `uploaded` only after valid server acknowledgement.
10. Retry failed uploads later.
11. Subscribe to remote command topic.
12. Publish command acknowledgement after executing command.
13. Never block recording because MQTT or HTTPS is down.

## Backend/VPS Scope

### Required VPS Components

| Component | Responsibility |
| --- | --- |
| MQTT broker | Receives status, heartbeat, sync events, command acknowledgements |
| Backend API | Handles login, device list, status reads, command requests, chunk uploads |
| Worker service | Subscribes to MQTT and writes validated messages to DB |
| Database | Stores users, devices, sessions, chunks, status, commands |

### Suggested API Endpoints

```text
POST /api/auth/login
GET  /api/devices
GET  /api/devices/{deviceId}/status
GET  /api/devices/{deviceId}/sessions
POST /api/devices/{deviceId}/commands
GET  /api/devices/{deviceId}/commands/{commandId}
POST /api/devices/{deviceId}/sessions/{sessionId}/chunks
```

### Backend Developer Tasks

1. Create database schema for users, devices, sessions, chunks, latest status, and commands.
2. Add user login API.
3. Add user-to-device authorization.
4. Add device authentication for ESP32 MQTT/API access.
5. Run MQTT broker on VPS.
6. Add MQTT worker service.
7. Validate MQTT messages before writing to DB.
8. Add chunk upload endpoint.
9. Validate chunk metadata, byte length, checksum, and duplicate chunk IDs.
10. Store uploaded chunk data.
11. Add latest device status API.
12. Add uploaded sessions API.
13. Add remote command API.
14. Forward remote commands to ESP32 through MQTT or command queue.
15. Store command acknowledgements.

## Security Requirements

Minimum requirements for this phase:

- Use HTTPS for VPS API.
- Use MQTT over TLS if available.
- Never store database credentials in ESP32 firmware.
- Give each ESP32 a unique device ID.
- Give each ESP32 a unique device secret or certificate.
- Require mobile user login for Remote mode.
- Authorize users only for their assigned devices.
- Reject unknown devices.
- Reject invalid checksums.

## Acceptance Criteria

Phase 4 is complete when:

- ESP32 connects to configured Wi-Fi.
- ESP32 connects to MQTT broker.
- ESP32 publishes heartbeat/status to VPS.
- VPS stores latest device status.
- Mobile Remote login works.
- Mobile app lists devices assigned to the logged-in user.
- Mobile app opens selected device dashboard.
- Remote dashboard shows latest status.
- ESP32 uploads pending SD chunks over HTTPS.
- VPS validates and stores uploaded chunks.
- ESP32 marks chunks uploaded only after acknowledgement.
- Remote command path works for at least start/stop recording.
- Offline device state is clearly shown in the mobile app.

## Out Of Scope

- Replacing the temporary developer password from Phase 2 with production-grade provisioning auth.
- Advanced analytics.
- Public web dashboard unless separately requested.
- Regulatory/compliance certification.

