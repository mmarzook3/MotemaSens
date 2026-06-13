# Display cleanup verification - 2026-06-13

Final firmware version tested: `dev-2026.06.13.15`

## Goal

The round LCD was too cluttered. Debug values and graph labels were fighting for the same screen space, and some text was too close to the edge of the circular display.

## Firmware changes

- Cleaned the display into a compact instrument layout.
- Kept ECG as the largest graph lane.
- Kept MIC and ACC as smaller lanes below ECG.
- Moved version, ECG status, BPM and WiFi dot into a compact top status area.
- Moved Core 0, Core 1, RAM and flash memory usage into two compact bottom rows.
- Removed detailed ECG diagnostic text from the display graph area. The detailed values are still available in serial and WiFi logs.
- Fixed UI startup so WiFi connection cannot block the display from starting.
- Display now continues to refresh while USB or WiFi logging is active, but at a slower refresh rate.

## Verification notes

- `dev-2026.06.13.12`: first display cleanup build. Camera showed mostly plain background because UI could still be blocked during WiFi startup/logging state.
- `dev-2026.06.13.13`: display refresh allowed during logging, but WiFi boot wait could still leave the boot background visible.
- `dev-2026.06.13.14`: WiFi boot wait reduced and logger starts later when WiFi becomes connected. Camera confirmed the UI appears before WiFi finishes.
- `dev-2026.06.13.15`: final layout adjustment. Text moved inward/upward for the round display edge.

## Final result

Final camera capture shows the device running `V13.15` with:

- version visible
- WiFi dot visible
- ECG OK/BPM visible
- ECG/MIC/ACC graph lanes visible
- Core/RAM/MEM status compactly shown at the lower edge

## Evidence files

- `docs/firmware/test_results/2026-06-13-display-clean-v13-12-serial.txt`
- `docs/firmware/test_results/2026-06-13-display-clean-v13-13-serial.txt`
- `docs/firmware/test_results/2026-06-13-display-clean-v13-14-serial.txt`
- `docs/firmware/test_results/2026-06-13-display-clean-v13-15-serial.txt`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_12_camera_0.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_12_camera_1.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_13_camera_0.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_13_camera_1.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_14_camera_0.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_14_camera_1.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_15_camera_0.jpg`
- `docs/firmware/debug_captures/2026-06-13/display_clean_v13_15_camera_1.jpg`
