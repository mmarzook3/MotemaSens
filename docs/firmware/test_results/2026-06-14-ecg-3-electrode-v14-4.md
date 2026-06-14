# ECG 3 Electrode Dev Test - dev-2026.06.14.4-3lead

Branch: `ecg-3-electrode`

Firmware: `dev-2026.06.14.4-3lead`

Date: 2026-06-14

## What changed

- Added a 3 electrode ECG build mode using RA, LA and LL only.
- Display now shows the programmed ECG mode as `ECG3L`.
- ADS1294 channel setup for this branch:
  - CH1 = Lead I, `LA - RA`
  - CH2 = Lead II, `LL - RA`
  - CH3 = powered down and shorted internally
  - CH4 = powered down and shorted internally
- Lead III is calculated in firmware as `Lead II - Lead I`.
- RLD drive is disabled for this branch because there is no RL electrode.
- RLD stability diagnostic is disabled for this branch.
- ECG display trace uses Lead II as the default visible ECG waveform.
- USB/WiFi CSV logging now includes `ecg_lead_i`, `ecg_lead_ii` and `ecg_lead_iii`.

## Build and flash

Build result: passed.

Flash result: passed on `COM14`.

## Serial proof

Serial boot log confirmed the correct firmware and ADS1294 channel mode:

```text
device version: dev-2026.06.14.4-3lead
ADS1294 ready, id=0x90 electrodes=3 cfg=96,C0,E0 ch=00,00,81,81
ADS1294 diag rld=0 lead_off=1 loff=00 rldp=00 rldn=00 loffp=03 loffn=03
```

Evidence file:

- `docs/firmware/test_results/2026-06-14-ecg-3-electrode-v14-4-serial.txt`

## USB log proof

USB live logging start/stop was tested. The CSV header includes the derived 3 electrode leads:

```text
LOG_HEADER,...,ecg_ch1,ecg_ch2,ecg_ch3,ecg_ch4,ecg_lead_i,ecg_lead_ii,ecg_lead_iii,...
```

Rows showed CH3 and CH4 parked at zero while Lead I, Lead II and Lead III were populated.

Evidence file:

- `docs/firmware/test_results/2026-06-14-ecg-3-electrode-v14-4-usb-log.txt`

## Camera proof

Camera capture shows the device running after flash and the display mode label as `ECG3L`.

Evidence frames:

- `docs/firmware/debug_captures/2026-06-14/ecg_3_electrode_v14_4/frame_0.jpg`
- `docs/firmware/debug_captures/2026-06-14/ecg_3_electrode_v14_4/frame_1.jpg`
- `docs/firmware/debug_captures/2026-06-14/ecg_3_electrode_v14_4/frame_2.jpg`

## Notes

- The QMI8658 init still prints a brief I2C timing error during boot sometimes, but it recovers and starts. This was already seen before this ECG branch change.
- This branch is for 3 electrode ECG only. `main` stays as the 4 lead firmware path.
