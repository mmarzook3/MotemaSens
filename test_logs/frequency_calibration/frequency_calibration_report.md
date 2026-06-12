# Frequency Calibration Report

Each capture is 60 seconds over USB at the firmware 100 Hz log rate.

| Test | Hz | Rows | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean | Motion max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ambient | 0 | 5999 | 100.00 | 3 | 8 | 0.2025 | 0.0 | 0.2023 | 0.2794 | 0.0071 |
| tone_60hz | 60 | 5999 | 100.01 | 8 | 10 | 0.1838 | 0.0 | 0.1836 | 0.2625 | 0.0073 |
| tone_80hz | 80 | 5999 | 100.01 | 8 | 11 | 0.1872 | 0.0 | 0.1872 | 0.2650 | 0.0072 |
| tone_100hz | 100 | 5999 | 100.01 | 1 | 6 | 0.2114 | 0.0 | 0.2113 | 0.2880 | 0.0075 |
| tone_120hz | 120 | 5999 | 100.01 | 1 | 0 | 0.2331 | 0.0 | 0.2331 | 0.3086 | 0.0075 |
| tone_150hz | 150 | 5999 | 100.01 | 0 | 0 | 0.2403 | 0.0 | 0.2403 | 0.3158 | 0.0075 |
| tone_200hz | 200 | 5999 | 100.01 | 0 | 3 | 0.2723 | 0.0 | 0.2722 | 0.3471 | 0.0094 |
| tone_250hz | 250 | 5999 | 100.01 | 0 | 0 | 0.2927 | 0.0 | 0.2926 | 0.3671 | 0.0071 |
| tone_300hz | 300 | 5999 | 100.01 | 0 | 0 | 0.2809 | 0.0 | 0.2808 | 0.3558 | 0.0072 |

Use steady tones to confirm the detector does not report false heartbeats for continuous sound.
Use the ambient row as the room and electronics noise floor for later threshold tuning.
Rows with high `mic_level_saturation_pct` are too loud for frequency response calibration and should be repeated at lower volume or farther from the speaker.
