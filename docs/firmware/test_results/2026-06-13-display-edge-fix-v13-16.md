# Display edge text fix - 2026-06-13

Firmware version tested: `dev-2026.06.13.16`

## Issue

The previous display layout still had text overflowing near the round LCD edge. The lower status strings were too long, and some empty curved screen areas were not being used well.

## Change

- Shortened the bottom status values.
- Replaced long `RAM` and `MEM` strings with compact `R` and `M` values.
- Removed extra CPU percentage text from the bottom rows.
- Pulled graph labels and grid away from the round display edge.
- Moved the version/WiFi/BPM area inward.
- Kept ECG, MIC and ACC graph lanes visible.

## Verification

- Built successfully with PlatformIO.
- Flashed directly to the board on `COM14`.
- Waited before camera capture so the real UI was visible, not the startup background.
- Serial confirmed `device version: dev-2026.06.13.16`.
- Camera capture confirmed the text is no longer spilling into the edge area.

## Evidence files

- `docs/firmware/test_results/2026-06-13-display-edge-fix-v13-16-serial.txt`
- `docs/firmware/debug_captures/2026-06-13/display_edge_fix_v13_16_camera_0.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_edge_fix_v13_16_camera_1.jpg`
