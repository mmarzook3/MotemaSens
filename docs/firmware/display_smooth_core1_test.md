# Display smooth Core 1 test branch

Branch: `display-smooth-core1-test`

Firmware: `dev-2026.06.14.3-smooth`

## Purpose

This branch tests fixes for a small display freeze that appeared about once per second.

## Changes

- Disabled once-per-second ECG serial diagnostic printing by default.
- Moved ECG serial diagnostics behind `ENABLE_ECG_SERIAL_DIAGNOSTIC`.
- Slowed system CPU/RAM status update from 1 second to 5 seconds.
- Cached flash/sketch usage once at boot instead of recalculating it during display animation.
- Slowed WiFi web status refresh from 1 second to 3 seconds.
- Moved LCD drawing earlier in the Core 1 loop, before WiFi HTTP, status and OTA checks.

## Expected result

The LCD waveform should move more smoothly with less once-per-second hesitation. Sensor acquisition and CSV logging rates are not changed.
