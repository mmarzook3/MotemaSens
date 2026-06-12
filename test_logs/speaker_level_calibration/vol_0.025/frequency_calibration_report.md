# Frequency Calibration Report

Each capture is 60 seconds over USB at the firmware 100 Hz log rate.

| Test | Hz | Rows | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean | Motion max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ambient | 0 | 998 | 100.03 | 3 | 0 | 0.2120 | 0.0 | 0.2114 | 0.2704 | 0.0071 |
| tone_120hz | 120 | 998 | 100.03 | 0 | 0 | 0.2239 | 0.0 | 0.2235 | 0.2790 | 0.0064 |

Use steady tones to confirm the detector does not report false heartbeats for continuous sound.
Use the ambient row as the room and electronics noise floor for later threshold tuning.
Rows with high `mic_level_saturation_pct` are too loud for frequency response calibration and should be repeated at lower volume or farther from the speaker.
