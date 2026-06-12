# Frequency Calibration - 2026-06-12

This calibration used the `mic_waveform_test` USB live logger at 100 Hz.
The PC generated sine tones at software volume `0.05`, with the mic held close to the PC speaker.

Raw lab captures are kept locally under `test_logs/frequency_calibration` and are not committed.

## Speaker Level Check

The PC speaker was tested with short 120 Hz captures at software volumes `0.015`, `0.025`, `0.035`, and `0.05`.
All four levels avoided clipping and did not produce tone-triggered beats during the short check.

The best calibration level is `--tone-volume 0.05`.

- `0.035` was too close to the ambient floor during a full 60 second sweep and produced random false detections in some rows.
- `0.05` stayed unsaturated while giving a clearer tone response above ambient.
- Higher software volumes are not needed for this heart-sound calibration and can make the PC speaker/mic path less representative of weak chest-contact sounds.

## Result Summary

| Test | Hz | Samples | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Ambient | 0 | 5999 | 100.00 | 3 | 8 | 0.2025 | 0.0 | 0.2023 | 0.2794 |
| Tone | 60 | 5999 | 100.01 | 8 | 10 | 0.1838 | 0.0 | 0.1836 | 0.2625 |
| Tone | 80 | 5999 | 100.01 | 8 | 11 | 0.1872 | 0.0 | 0.1872 | 0.2650 |
| Tone | 100 | 5999 | 100.01 | 1 | 6 | 0.2114 | 0.0 | 0.2113 | 0.2880 |
| Tone | 120 | 5999 | 100.01 | 1 | 0 | 0.2331 | 0.0 | 0.2331 | 0.3086 |
| Tone | 150 | 5999 | 100.01 | 0 | 0 | 0.2403 | 0.0 | 0.2403 | 0.3158 |
| Tone | 200 | 5999 | 100.01 | 0 | 3 | 0.2723 | 0.0 | 0.2722 | 0.3471 |
| Tone | 250 | 5999 | 100.01 | 0 | 0 | 0.2927 | 0.0 | 0.2926 | 0.3671 |
| Tone | 300 | 5999 | 100.01 | 0 | 0 | 0.2809 | 0.0 | 0.2808 | 0.3558 |

## Findings

- Logging timing is correct. All captures were approximately 100 Hz with about 5999 samples over 60 seconds.
- The software tone volume avoided clipping. Saturation stayed at 0.0 percent for all rows.
- Continuous tones at 150 Hz and above did not create false beat detections.
- Continuous 60 Hz and 80 Hz tones created sparse false beat detections.
- Ambient sound also created a few false detections.
- False detections were mostly low-level and/or high-AGC events. Examples:
  - Ambient: level about 0.21 to 0.30, gain about 20 to 36.
  - 60 Hz tone: early false events level about 0.20 to 0.30 with gain above 11; later events reached about 0.54 to 0.73 after AGC settled lower.
  - 80 Hz tone: mostly level about 0.23 to 0.42, with one stronger event at about 0.69.

## Calibration Direction

The detector should reject continuous low-frequency tone and ambient events without losing chest-contact heart sounds.

Recommended next firmware tuning:

- Add a candidate quality gate using beat level and AGC. Low-level candidates while `autoGain` is high should be rejected.
- Keep the 720 ms refractory for resting heart-sound tests to avoid S1/S2 double counting.
- Add a steady-tone rejection check. A candidate should need a stronger local peak above the recent floor and a clear pre-beat valley.
- Keep reporting rejected candidates in `LIVE_TEST_END`; this is useful for calibration.

Do not tune only from steady speaker tones. Final values must still be checked against real chest-contact heart captures because the mechanical coupling is different.
