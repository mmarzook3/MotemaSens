# Version display proof

Date: 2026-06-13

Firmware version flashed:

- `dev-2026.06.13.5`

Build/display rule added:

- Every firmware change must bump `DEVICE_VERSION`, even local USB flashes.
- Dev builds use the `dev-YYYY.MM.DD.N` pattern.
- The LCD shows the full `SW <version>` text and a large short build ID.
- For this build the large ID is `V13.5`.

Verification:

- Build passed.
- USB flash passed on `COM14`.
- Camera proof captured after flashing.

Evidence:

- `docs/firmware/debug_captures/2026-06-13/version_dev_2026_06_13_5_display.jpg`
- `docs/firmware/debug_captures/2026-06-13/version_dev_2026_06_13_5_display_crop.jpg`

