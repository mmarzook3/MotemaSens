# Phase 3: Local Streaming And SD Offline Storage

Checked on: 2026-06-14

## Goal

Add high-performance Local IP streaming, mobile-side plotting, and microSD offline storage.

At the end of this phase:

- Local mode streams sensor data to the mobile app.
- The ESP32 does not render live plots on its own display during Local streaming.
- The ESP32 display only shows a simple connected state such as `Connected to Mobile`.
- The mobile app performs live plotting and visualization.
- The ESP32 records data to microSD chunks when SD hardware is fixed.
- The mobile app can browse and download SD files over Local HTTP.

## Critical Hardware Note

Do not enable microSD firmware until the documented SD routing issue is fixed.

Current documented issue:

```text
SPI2_MISO is incorrectly routed to VSYS.
Do not enable microSD firmware until this hardware routing is corrected.
```

## Critical Local Mode Performance Rule

When the mobile app is connected through Local IP:

```text
The ESP32 screen must only show:

"Connected to Mobile"

The ESP32 must not render plots, charts, waveforms, or other CPU-intensive live views on its own display.
All live plotting and visualization must be done by the mobile app.
```

Reason:

```text
Rendering plots on the ESP32 display while acquiring, buffering, writing to SD, and streaming can reduce performance and increase dropped samples.
The ESP32 must prioritize acquisition, buffering, SD writes, local API responses, and streaming.
```

## Mobile App Scope

### Local Dashboard Additions

Add:

- Live stream view.
- Mobile-side waveform plotting.
- Stream start/stop controls if needed.
- Stream health indicators.
- Dropped-sample or reconnect warning if firmware exposes it.
- SD session browser.
- SD file/chunk browser.
- SD file/chunk download.
- Download progress.

### Mobile Developer Tasks

1. Add live plotting in the mobile app.
2. Consume streamed data from ESP32 over Local IP.
3. Keep plotting efficient on mobile.
4. Do not request ESP32-side waveform rendering.
5. Add SD browser screen.
6. Add SD file/chunk download.
7. Add download progress and retry UI.
8. Show connection mode as `Local`.
9. Show stream state and basic stream health.

## Firmware Scope

### Local Display Behavior

When Local streaming is active:

```text
ESP32 display = Connected to Mobile
No live waveform drawing
No chart rendering
No CPU-heavy display updates
```

### Streaming Behavior

Firmware should:

1. Acquire sensor data.
2. Buffer data safely.
3. Write to SD if recording is active and SD is available.
4. Stream raw or lightly packed data to the mobile app.
5. Keep local status API responsive.
6. Avoid blocking acquisition during HTTP requests or file downloads.

### SD Storage Structure

Recommended SD layout:

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

### SD Sync States

| State | Meaning |
| --- | --- |
| pending | Chunk exists locally and has not been uploaded |
| uploading | Upload is in progress |
| uploaded | Server acknowledged the chunk |
| failed | Last upload failed; retry later |

### Local HTTP Endpoints To Add

```text
GET  /api/files
GET  /api/files/{fileName}
GET  /api/chunks
GET  /api/chunks/{chunkId}
GET  /api/stream/status
```

Streaming endpoint can be implemented using the most suitable firmware approach:

```text
WebSocket
Server-sent events
Chunked HTTP
Polling batches
```

Choose the approach that is stable on the ESP32 and easy for Flutter to consume.

### Firmware Developer Tasks

1. Confirm SD hardware routing is fixed before SD enablement.
2. Mount SD on boot and report `sdReady`.
3. Create session folder when recording starts.
4. Write data in numbered chunks.
5. Write metadata next to each chunk.
6. Add local streaming endpoint.
7. Add local SD list endpoint.
8. Add local SD download endpoint.
9. Keep `/api/status` fast and non-blocking.
10. Disable ESP32-side live plotting during Local streaming.
11. Show only `Connected to Mobile` on the ESP32 display during Local streaming.
12. Prioritize acquisition, buffering, SD writes, and streaming over display updates.

## Backend Scope

No VPS backend is required in Phase 3.

However, SD metadata should already include fields needed by Phase 4 upload:

```json
{
  "deviceId": "ms-0001",
  "sessionId": "2026-06-14-ecg-001",
  "chunkId": 1,
  "recordingType": "ecg",
  "sampleRate": 500,
  "startedAtUtc": "2026-06-14T02:30:00Z",
  "endedAtUtc": "2026-06-14T02:31:00Z",
  "byteLength": 120000,
  "sha256": "..."
}
```

## Acceptance Criteria

Phase 3 is complete when:

- Local mode can stream data to the mobile app.
- The mobile app plots live data.
- ESP32 display shows only `Connected to Mobile` during Local streaming.
- ESP32 does not render live plots while streaming to mobile.
- Recording can continue while Local streaming is active.
- SD recording works after hardware fix.
- SD chunks and metadata are created.
- Mobile app can list SD files/chunks over Local HTTP.
- Mobile app can download SD files/chunks over Local HTTP.
- `/api/status` remains responsive during streaming.

## Out Of Scope

- MQTT.
- VPS database.
- Remote login.
- Cloud dashboard.
- Automatic cloud upload.

