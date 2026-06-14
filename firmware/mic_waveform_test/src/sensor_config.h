#pragma once

#include <Arduino.h>

#ifndef ENABLE_MIC_SENSOR
#define ENABLE_MIC_SENSOR 1
#endif

#ifndef ENABLE_ACCEL_SENSOR
#define ENABLE_ACCEL_SENSOR 1
#endif

#ifndef ENABLE_ECG_SENSOR
#define ENABLE_ECG_SENSOR 1
#endif

#ifndef ENABLE_ECG_RLD_DRIVE
#define ENABLE_ECG_RLD_DRIVE 1
#endif

#ifndef ENABLE_ECG_LEAD_OFF_DETECTION
#define ENABLE_ECG_LEAD_OFF_DETECTION 1
#endif

#ifndef ENABLE_ECG_DC_SATURATION_DIAGNOSTIC
#define ENABLE_ECG_DC_SATURATION_DIAGNOSTIC 1
#endif

#ifndef ENABLE_ECG_NOISE_DIAGNOSTICS
#define ENABLE_ECG_NOISE_DIAGNOSTICS 1
#endif

#ifndef ENABLE_ECG_RLD_STABILITY_DIAGNOSTIC
#define ENABLE_ECG_RLD_STABILITY_DIAGNOSTIC 1
#endif

#ifndef ENABLE_ECG_SERIAL_DIAGNOSTIC
#define ENABLE_ECG_SERIAL_DIAGNOSTIC 0
#endif

// Waveshare ESP32-S3-LCD-1.28 display pins.
static constexpr int LCD_DC = 8;
static constexpr int LCD_CS = 9;
static constexpr int LCD_SCLK = 10;
static constexpr int LCD_MOSI = 11;
static constexpr int LCD_RST = 12;
static constexpr int LCD_BL = 40;

// Custom Lobe PCB SPH0645LM4H-B mic pins.
static constexpr int I2S_DATA = 3;
static constexpr int I2S_BCLK = 4;
static constexpr int I2S_WS = 5;

// Custom Lobe PCB LEDs.
static constexpr int LED_GREEN = 14;
static constexpr int LED_BLUE = 15;

// Waveshare base-board QMI8658 IMU pins.
static constexpr int IMU_SDA = 6;
static constexpr int IMU_SCL = 7;

// Custom Lobe PCB ADS1294 ECG pins.
static constexpr int ECG_MOSI = 36;
static constexpr int ECG_PWDN = 35;
static constexpr int ECG_RESET = 34;
static constexpr int ECG_START = 33;
static constexpr int ECG_CS = 21;
static constexpr int ECG_SCLK = 18;
static constexpr int ECG_MISO = 17;
static constexpr int ECG_DRDY = 16;
