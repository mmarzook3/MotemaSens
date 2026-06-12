# Frequency Calibration Report

Each capture is 60 seconds over USB at the firmware 100 Hz log rate.

| Test | Hz | Rows | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean | Motion max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ambient | 0 | 999 | 100.03 | 0 | 0 | 0.2047 | 0.0 | 0.2047 | 0.2650 | 0.0062 |
| tone_120hz | 120 | 999 | 100.03 | 0 | 0 | 0.2405 | 0.0 | 0.2399 | 0.2905 | 0.0068 |

Use steady tones to confirm the detector does not report false heartbeats for continuous sound.
Use the ambient row as the room and electronics noise floor for later threshold tuning.
Rows with high `mic_level_saturation_pct` are too loud for frequency response calibration and should be repeated at lower volume or farther from the speaker.
