# ECG front-end diagnostics test plan

Date: 2026-06-13

This plan checks the ADS1294 firmware front-end features before using ECG data as a real measurement source.

## Firmware features under test

- ADS1294 internal RLD/bias drive is configured by `ENABLE_ECG_RLD_DRIVE`.
- ADS1294 lead-off detection is configured by `ENABLE_ECG_LEAD_OFF_DETECTION`.
- Raw channel rail checks report DC offset saturation.
- Common-mode movement reports cable shield/noise behaviour.
- Sustained high common-mode and differential movement reports possible RLD loop instability.

## CSV fields to review

- `ecg_lead_off_p`
- `ecg_lead_off_n`
- `ecg_sat_mask`
- `ecg_diag_flags`
- `ecg_common_step`
- `ecg_diff_step`
- `ecg_ch1` to `ecg_ch4`

Flag meaning:

| Bit | Meaning |
| --- | --- |
| `0x0001` | Lead-off detected |
| `0x0002` | DC saturation detected |
| `0x0004` | Cable/common-mode noise high |
| `0x0008` | Possible RLD instability |
| `0x0010` | RLD configured |
| `0x0020` | Lead-off configured |

## Bench checks

1. No electrodes connected.
   - Expected: `ecg_diag_flags` includes configured bits `0x0010` and `0x0020`.
   - Expected: lead-off masks should show an open input once ADS lead-off has settled.

2. Electrodes/cable connected to the board but not attached to a body.
   - Expected: no ADC rail saturation in `ecg_sat_mask`.
   - Expected: moving or touching the cable increases `ecg_common_step`.
   - Expected: shield/noise behaviour can be seen without freezing ECG sequence.

3. Inputs connected to a signal generator or ECG simulator through safe isolated test coupling.
   - Expected: `ecg_ch1` and `ecg_ch2` move without staying at ADC rails.
   - Expected: `ecg_sat_mask` remains `00` at normal test amplitude.
   - Expected: ECG sequence continues increasing at the dev cadence.

4. RLD stability check.
   - Expected: with stable input/cable, `ecg_diag_flags` should not keep `0x0008` set.
   - Expected: if cable placement or shield routing creates oscillation/noise, `ecg_common_step` and `ecg_diff_step` rise together and `0x0008` may appear.

## Pass condition for current dev hardware

- Firmware boots and prints ADS1294 diagnostic register setup.
- ECG sequence increments continuously.
- Lead-off masks change between open and connected cable states.
- `ecg_sat_mask` stays clear when electrodes or simulator are connected normally.
- RLD instability flag is not continuously set during a stable cable setup.

This test plan is for development hardware validation only. It does not prove human safety or regulatory ECG performance.
