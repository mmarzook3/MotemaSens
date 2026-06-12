# Frequency Calibration Report

Each capture is 60 seconds over USB at the firmware 100 Hz log rate.

| Test | Hz | Rows | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean | Motion max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ambient | 0 | 999 | 100.03 | 4 | 0 | 0.2043 | 0.0 | 0.2037 | 0.2645 | 0.0065 |
| tone_120hz | 120 | 998 | 100.03 | 0 | 0 | 0.2231 | 0.0 | 0.2225 | 0.2805 | 0.0067 |

Use steady tones to confirm the detector does not report false heartbeats for continuous sound.
Use the ambient row as the room and electronics noise floor for later threshold tuning.
Rows with high `mic_level_saturation_pct` are too loud for frequency response calibration and should be repeated at lower volume or farther from the speaker.
