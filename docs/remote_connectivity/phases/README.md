# MotemaSens Remote Connectivity Phased Development

Checked on: 2026-06-14

## Purpose

This folder breaks the MotemaSens remote connectivity work into four clear development phases.

Each phase has its own handoff document so the developer only needs the document for the current phase.

## Phase Documents

| Phase | Document | Main Goal |
| --- | --- | --- |
| Phase 1 | `phase_1_app_entry_and_local_control.md` | App opens with connection choices and Local IP control works |
| Phase 2 | `phase_2_ble_control_and_device_configuration.md` | BLE control and developer Wi-Fi configuration |
| Phase 3 | `phase_3_local_streaming_and_sd_storage.md` | Local streaming, mobile plotting, and SD offline storage |
| Phase 4 | `phase_4_remote_vps_sync_and_cloud_app.md` | VPS remote access, MQTT status, HTTPS chunk upload, and cloud app views |

## Recommended Delivery Order

```text
Phase 1 -> Phase 2 -> Phase 3 -> Phase 4
```

Do not start Phase 3 SD firmware until the documented microSD hardware routing issue is fixed.

## Source Architecture

The full strategy document is:

```text
docs/remote_connectivity_strategy_architecture.md
```

