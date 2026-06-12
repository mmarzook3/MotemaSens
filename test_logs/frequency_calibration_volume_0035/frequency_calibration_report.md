# Frequency Calibration Report

Each capture is 60 seconds over USB at the firmware 100 Hz log rate.

| Test | Hz | Rows | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean | Motion max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ambient | 0 | 5999 | 100.01 | 8 | 9 | 0.2033 | 0.0 | 0.2032 | 0.2800 | 0.0073 |
| tone_60hz | 60 | 5999 | 100.00 | 3 | 13 | 0.2037 | 0.0 | 0.2035 | 0.2799 | 0.0069 |
| tone_80hz | 80 | 5999 | 100.01 | 2 | 7 | 0.2050 | 0.0 | 0.2050 | 0.2815 | 0.0070 |
| tone_100hz | 100 | 5999 | 100.01 | 0 | 5 | 0.2210 | 0.0 | 0.2208 | 0.2973 | 0.0070 |
| tone_120hz | 120 | 5999 | 100.01 | 0 | 0 | 0.2212 | 0.0 | 0.2211 | 0.2965 | 0.0079 |
| tone_150hz | 150 | 5999 | 100.01 | 0 | 0 | 0.2380 | 0.0 | 0.2377 | 0.3128 | 0.0071 |
| tone_200hz | 200 | 5999 | 100.01 | 5 | 8 | 0.2134 | 0.0 | 0.2132 | 0.2878 | 0.0087 |
| tone_250hz | 250 | 5999 | 100.01 | 0 | 0 | 0.2676 | 0.0 | 0.2675 | 0.3426 | 0.0111 |
| tone_300hz | 300 | 5999 | 100.00 | 7 | 7 | 0.1740 | 0.0 | 0.1739 | 0.2509 | 0.0127 |

Use steady tones to confirm the detector does not report false heartbeats for continuous sound.
Use the ambient row as the room and electronics noise floor for later threshold tuning.
Rows with high `mic_level_saturation_pct` are too loud for frequency response calibration and should be repeated at lower volume or farther from the speaker.
