# Lobe ESP32-S3 Hardware/Software Interface

Source schematics:

- Custom PCB: `docs/hardware/reference/Lobe_ESP32-S3_custom_schematic.pdf`
- Waveshare base board: `docs/hardware/reference/Waveshare_ESP32-S3-LCD-1.28_schematic.pdf`
- Waveshare wiki export: `docs/hardware/reference/Waveshare_ESP32-S3-LCD-1.28_wiki.pdf`

Date reviewed: 2026-06-08

## Scope

This HSI describes the firmware-facing interface between the custom Lobe PCB and the Waveshare ESP32-S3-LCD-1.28 development board.

The custom PCB adds:

- ADS1294IPAG ECG analog front end
- microSD socket
- SPH0645LM4H-B I2S MEMS microphone
- Two user switch/test inputs
- Green and blue LEDs
- Power filtering and analog/digital ground domains
- Two 20-pin FTSH-110-01-F-DV-TR mating headers to the Waveshare board

## Blocking Interface Issues

These should be resolved before firmware bring-up or PCB release.

| ID | Area | Issue | Impact | Recommended fix |
| --- | --- | --- | --- | --- |
| HSI-001 | microSD SPI | Custom net `SPI2_MISO` is connected to H1 pin 18. On the Waveshare header, H1 pin 18 is `VSYS`, not a GPIO. | SD card DAT0/MISO is shorted or coupled to the system supply rail. This can damage hardware and prevents SD reads. | Move `SPI2_MISO` to an actual free GPIO exposed on the header. Prefer a currently unused non-reserved pin such as H2 pin 19 / GPIO13 if routing allows. Avoid H1 pin 10 / GPIO40 unless LCD backlight control is intentionally sacrificed. |
| HSI-002 | Power domains | `+3.3VA` and `+3.3VD` are connected through ferrite beads FB1/FB2 and shared decoupling, but the only board supply shown from the Waveshare header is `+3.3VD`. | Firmware has no control over analog power sequencing. Noise and brownout behavior must be handled electrically. | Confirm the analog rail current and filtering are acceptable for ADS1294 operation. |
| HSI-003 | ECG safety | Electrode inputs RA/LA/LL/RL connect to ADS1294 input and RLD networks. | This is a human-contact analog front end. Firmware alone cannot guarantee safety. | Confirm patient protection, isolation, ESD, leakage current, and regulatory intent before connecting to a person. |

## Waveshare Base Board Reserved Signals

These signals are already used on the ESP32-S3-LCD-1.28 board and should not be reused by the custom PCB unless the onboard function is intentionally disabled.

| ESP32-S3 GPIO | Base-board function |
| --- | --- |
| GPIO0 | BOOT button |
| RESET/RUN | Reset input |
| GPIO1 | Battery ADC |
| GPIO6 | IMU SDA |
| GPIO7 | IMU SCL |
| GPIO8 | LCD DC |
| GPIO9 | LCD CS |
| GPIO10 | LCD CLK |
| GPIO11 | LCD DIN |
| GPIO12 | LCD RST |
| GPIO40 | LCD backlight on base schematic; also appears on H1 pin 10 |
| GPIO47 | IMU INT1, not exposed on the custom H1/H2 connector symbols |
| GPIO48 | IMU INT2, not exposed on the custom H1/H2 connector symbols |

## Custom Connector Map

### H1 / J2 Header

H1 is the primary high-speed/custom IO header. The custom schematic uses these pins:

| H1 pin | Waveshare signal | Custom net | Firmware direction | Notes |
| --- | --- | --- | --- | --- |
| 1 | GPIO36 | `SPI1_MOSI` | ESP32 output | ADS1294 DIN |
| 3 | GPIO35 | `ECG_/PWDN` | ESP32 output | Active-low ADS1294 power-down control |
| 5 | GPIO34 | `ECG_RESET` | ESP32 output | ADS1294 reset |
| 7 | GPIO33 | `ECG_START` | ESP32 output | ADS1294 conversion start |
| 9 | GPIO21 | `SPI1_ECG_CS` | ESP32 output | ADS1294 chip select, active low |
| 11 | GPIO18 | `SPI1_SCLK` | ESP32 output | ADS1294 SPI clock |
| 13 | GPIO17 | `SPI1_MISO` | ESP32 input | ADS1294 DOUT |
| 15 | GPIO16 | `ECG_/DRDY` | ESP32 input | ADS1294 data-ready, active low |
| 17 | GPIO15 | `LED_BLUE` | ESP32 output | Drives LED through 100 ohm to DGND, active high |
| 19 | GPIO14 | `LED_GREEN` | ESP32 output | Drives LED through 100 ohm to DGND, active high |
| 6 | GPIO42 | `SW1` | ESP32 input | 4.7 kOhm pull-up to `+3.3VD`; external/test switch pulls to DGND |
| 8 | GPIO41 | `SW2` | ESP32 input | 4.7 kOhm pull-up to `+3.3VD`; external/test switch pulls to DGND |
| 12 | GPIO39 | `SPI2_SD_CS` | ESP32 output | microSD chip select; 10 kOhm pull-up to `+3.3VD` |
| 14 | GPIO38 | `SPI2_MOSI` | ESP32 output | microSD CMD/MOSI through 33 ohm series resistor |
| 16 | GPIO37 | `SPI2_SCLK` | ESP32 output | microSD CLK through 33 ohm series resistor |
| 18 | VSYS | `SPI2_MISO` | Invalid | Blocking issue HSI-001 |
| 20 | GND | `DGND` | Power | Digital ground |

Unused/no-connect in the custom schematic: H1 pins 2, 4, and 10. Note that H1 pin 10 is GPIO40 on the Waveshare header and is used by the base-board LCD backlight.

### H2 / J3 Header

H2 carries power and I2S microphone signals.

| H2 pin | Waveshare signal | Custom net | Firmware direction | Notes |
| --- | --- | --- | --- | --- |
| 1 | GND | `DGND` | Power | Digital ground |
| 2 | GND | `DGND` | Power | Digital ground |
| 3 | VSYS | `VSYS` | Power | System supply from Waveshare board |
| 4 | 3V3 | `+3.3VD` | Power | Digital 3.3 V rail for custom PCB |
| 16 | GPIO3 | `I2S_DATA` | ESP32 input | SPH0645 DATA_OUT through 51 ohm series resistor |
| 18 | GPIO4 | `I2S_BCLK` | ESP32 output | SPH0645 BCLK through 51 ohm series resistor |
| 20 | GPIO5 | `I2S_WS` | ESP32 output | SPH0645 WS/LRCLK through 51 ohm series resistor |

Unused/no-connect in the custom schematic: H2 pins 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, and 19. H2 pin 19 / GPIO13 appears to be the cleanest exposed spare candidate for `SPI2_MISO`.

## Firmware Pin Definitions

Suggested firmware constants after fixing HSI-001:

```c
// ADS1294 ECG SPI/control
#define PIN_ECG_MOSI      36
#define PIN_ECG_PWDN      35
#define PIN_ECG_RESET     34
#define PIN_ECG_START     33
#define PIN_ECG_CS        21
#define PIN_ECG_SCLK      18
#define PIN_ECG_MISO      17
#define PIN_ECG_DRDY      16

// User IO
#define PIN_LED_BLUE      15
#define PIN_LED_GREEN     14
#define PIN_SW1           42
#define PIN_SW2           41

// microSD SPI
#define PIN_SD_CS         39
#define PIN_SD_MOSI       38
#define PIN_SD_SCLK       37
// TODO: assign after schematic fix. H1 pin 18 is VSYS, not GPIO.
#define PIN_SD_MISO       -1

// I2S microphone
#define PIN_I2S_DATA       3
#define PIN_I2S_BCLK       4
#define PIN_I2S_WS         5
```

## ADS1294 ECG Interface

Device: `ADS1294IPAG`

### Digital Connections

| ADS1294 pin | ADS1294 signal | Custom net | ESP32 GPIO |
| --- | --- | --- | --- |
| 34 | DIN | `SPI1_MOSI` | GPIO36 |
| 40 | SCLK | `SPI1_SCLK` | GPIO18 |
| 43 | DOUT | `SPI1_MISO` | GPIO17 |
| 39 | CS | `SPI1_ECG_CS` | GPIO21 |
| 47 | DRDY | `ECG_/DRDY` | GPIO16 |
| 38 | START | `ECG_START` | GPIO33 |
| 35 | PWDN | `ECG_/PWDN` | GPIO35 |
| 36 | RESET | `ECG_RESET` | GPIO34 |
| 52 | CLKSEL | `+3.3VD` | Internal oscillator selected |
| 37 | CLK | NC | Not used |
| 41 | DAISY_IN | `DGND` | Daisy-chain disabled |
| 42, 44, 45, 46 | GPIO1-4 | NC | Not used |

Firmware notes:

- Configure ADS1294 SPI mode according to the ADS1294 datasheet before register access.
- Treat `ECG_/DRDY` as active-low interrupt/data-ready.
- Bring-up sequence should control `ECG_/PWDN`, `ECG_RESET`, and `ECG_START` explicitly; avoid leaving them floating during boot.
- `CLKSEL` is tied high, so firmware should expect the ADS1294 internal oscillator path unless the schematic is changed.

### Analog Connections

| Electrode/test node | Front-end network | ADS1294 input |
| --- | --- | --- |
| `RA1` | 15 kOhm + 15 kOhm input filter with 68 pF shunts | `RA-INxN`, connected to IN1N and IN2N |
| `LA1` | 15 kOhm + 15 kOhm input filter with 68 pF shunts | `LA-IN1P`, connected to IN1P |
| `LL1` | 15 kOhm + 15 kOhm input filter with 68 pF shunts | `LL-IN2P`, connected to IN2P |
| `RL1` | 68 kOhm path into RLD network | `RLDOUT` / `RLDINV` network |

ADS1294 channels used:

- Channel 1: `LA-IN1P` vs `RA-INxN`
- Channel 2: `LL-IN2P` vs `RA-INxN`
- Channels 3-8: not connected

## microSD Interface

Socket: `DM3BT-DSF-PEJS`

| microSD pin | microSD signal | Custom net | ESP32 mapping |
| --- | --- | --- | --- |
| 4 | VDD | `+3.3VD` | Power |
| 6 | VSS | `DGND` | Ground |
| 3 | CMD | `SPI2_MOSI` | GPIO38 through 33 ohm |
| 5 | CLK | `SPI2_SCLK` | GPIO37 through 33 ohm |
| 7 | DAT0 | `SPI2_MISO` | Invalid until HSI-001 fixed |
| 2 | CD/DAT3 | `SPI2_SD_CS` | GPIO39 |
| 1 | DAT2 | NC | Not used in SPI mode |
| 8 | DAT1 | NC | Not used in SPI mode |
| 9, 10 | Card detect | NC | No firmware card-detect input |

Firmware notes:

- Use SPI mode, not SDMMC 4-bit mode.
- No card-detect signal is wired, so firmware should probe the card on demand.
- Do not initialize this peripheral until `SPI2_MISO` is moved off `VSYS`.

## I2S Microphone Interface

Device: `SPH0645LM4H-B`

| Mic pin | Mic signal | Custom net | ESP32 GPIO | Notes |
| --- | --- | --- | --- | --- |
| 5 | VDD | `+3.3VD` | Power | Local 1 uF + 0.1 uF decoupling |
| 3 | GND | `DGND` | Ground | Digital ground |
| 4 | BCLK | `I2S_BCLK` | GPIO4 | 51 ohm series resistor |
| 1 | WS | `I2S_WS` | GPIO5 | 51 ohm series resistor |
| 6 | DATA_OUT | `I2S_DATA` | GPIO3 | 51 ohm series resistor |
| 2 | SELECT | `DGND` via 0 ohm | Channel select strapped low |

Firmware notes:

- Configure ESP32 as I2S master.
- `SELECT` is tied low, so the microphone is strapped to the channel defined by the SPH0645 datasheet for low select.
- Use 3.3 V IO levels.

## User Inputs

| Custom net | ESP32 GPIO | Electrical behavior | Firmware behavior |
| --- | --- | --- | --- |
| `SW1` | GPIO42 | 4.7 kOhm pull-up to `+3.3VD`; switch/test point to DGND | Active-low input |
| `SW2` | GPIO41 | 4.7 kOhm pull-up to `+3.3VD`; switch/test point to DGND | Active-low input |

Internal pull-ups may be enabled in firmware but are not required.

## LEDs

| Custom net | ESP32 GPIO | Electrical behavior | Firmware behavior |
| --- | --- | --- | --- |
| `LED_GREEN` | GPIO14 | GPIO -> 100 ohm -> green LED -> DGND | Drive high to turn on |
| `LED_BLUE` | GPIO15 | GPIO -> 100 ohm -> blue LED -> DGND | Drive high to turn on |

## Power And Ground

| Net | Source/use |
| --- | --- |
| `VSYS` | System supply from Waveshare header |
| `+3.3VD` | Digital 3.3 V from Waveshare header; powers ADS1294 DVDD, SD card, mic, switches, LEDs |
| `+3.3VA` | Analog 3.3 V rail for ADS1294 AVDD/reference filtering |
| `DGND` | Digital ground |
| `AGND` | Analog ground |

Filtering/decoupling:

- `+3.3VA` and `+3.3VD` domains are coupled through ferrite beads marked 600 ohm at 100 MHz.
- ADS1294 analog and digital supplies have local 1 uF / 0.1 uF style decoupling.
- ADS1294 VCAP pins are decoupled per schematic with 22 uF, 1 uF, and 0.1 uF capacitors.
- microSD and mic have local decoupling on `+3.3VD`.

## Bring-Up Checklist

1. Fix `SPI2_MISO` routing before powering a populated board.
2. Power the board from USB with the custom PCB unpopulated or current-limited first.
3. Verify `+3.3VD`, `+3.3VA`, `DGND`, and `AGND` with a meter.
4. Hold ECG controls inactive, then check GPIO boot state does not unintentionally enable the ADS1294.
5. Blink `LED_GREEN` and `LED_BLUE`.
6. Read `SW1` and `SW2` as active-low inputs.
7. Bring up ADS1294 SPI using only register read/write first.
8. Enable `ECG_/DRDY` interrupt and validate sample timing.
9. Bring up I2S microphone and confirm clock/data activity.
10. Bring up microSD only after `SPI2_MISO` is corrected.
