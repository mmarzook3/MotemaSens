#include "ecg_ads1294.h"

#include "debug_serial.h"
#include "sensor_config.h"
#include "spi_display_guard.h"
#include <SPI.h>

static constexpr uint32_t ECG_SPI_HZ = 1000000;
static constexpr uint8_t CMD_WAKEUP = 0x02;
static constexpr uint8_t CMD_RESET = 0x06;
static constexpr uint8_t CMD_START = 0x08;
static constexpr uint8_t CMD_SDATAC = 0x11;
static constexpr uint8_t CMD_RDATA = 0x12;
static constexpr uint8_t CMD_RDATAC = 0x10;
static constexpr uint8_t CMD_RREG = 0x20;
static constexpr uint8_t CMD_WREG = 0x40;

static constexpr uint8_t REG_ID = 0x00;
static constexpr uint8_t REG_CONFIG1 = 0x01;
static constexpr uint8_t REG_CONFIG2 = 0x02;
static constexpr uint8_t REG_CONFIG3 = 0x03;
static constexpr uint8_t REG_LOFF = 0x04;
static constexpr uint8_t REG_CH1SET = 0x05;
static constexpr uint8_t REG_CH2SET = 0x06;
static constexpr uint8_t REG_CH3SET = 0x07;
static constexpr uint8_t REG_CH4SET = 0x08;
static constexpr uint8_t REG_RLD_SENSP = 0x0D;
static constexpr uint8_t REG_RLD_SENSN = 0x0E;
static constexpr uint8_t REG_LOFF_SENSP = 0x0F;
static constexpr uint8_t REG_LOFF_SENSN = 0x10;
static constexpr uint8_t REG_LOFF_STATP = 0x12;
static constexpr uint8_t REG_LOFF_STATN = 0x13;
static constexpr uint32_t ECG_FRAME_PERIOD_US = 10000;
static constexpr uint32_t ECG_RECOVERY_PERIOD_MS = 1000;
static constexpr uint8_t ECG_ACTIVE_ELECTRODE_MASK = 0x03;
static constexpr size_t ECG_DIAG_CHANNEL_COUNT = (ECG_ELECTRODE_COUNT == 3) ? 2 : ECG_CHANNELS;
static constexpr uint8_t ECG_CH_NORMAL_ELECTRODE = 0x00;
static constexpr uint8_t ECG_CH_POWERDOWN_SHORTED = 0x81;
static constexpr int32_t ECG_ADC_SATURATION_LIMIT = 7600000;
static constexpr float ECG_CABLE_NOISE_STEP_LIMIT = 60000.0f;
static constexpr float ECG_RLD_UNSTABLE_STEP_LIMIT = 90000.0f;

static SPIClass ecgSpi(HSPI);
static SPISettings ecgSettings(ECG_SPI_HZ, MSBFIRST, SPI_MODE1);

void EcgAds1294::command(uint8_t value)
{
  lockSpiDisplayGuard();
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(value);
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
  unlockSpiDisplayGuard();
  delayMicroseconds(4);
}

uint8_t EcgAds1294::readRegister(uint8_t address)
{
  lockSpiDisplayGuard();
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(CMD_RREG | (address & 0x1F));
  ecgSpi.transfer(0x00);
  const uint8_t value = ecgSpi.transfer(0x00);
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
  unlockSpiDisplayGuard();
  delayMicroseconds(4);
  return value;
}

void EcgAds1294::writeRegister(uint8_t address, uint8_t value)
{
  lockSpiDisplayGuard();
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(CMD_WREG | (address & 0x1F));
  ecgSpi.transfer(0x00);
  ecgSpi.transfer(value);
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
  unlockSpiDisplayGuard();
  delayMicroseconds(4);
}

void EcgAds1294::startContinuousRead()
{
  command(CMD_SDATAC);
  delay(2);
  digitalWrite(ECG_START, LOW);
  delay(2);
  digitalWrite(ECG_START, HIGH);
  delay(2);
  command(CMD_START);
  delay(2);
  lastFrameUs_ = micros();
  zeroFrames_ = 0;
}

void EcgAds1294::begin()
{
#if !ENABLE_ECG_SENSOR
  ready_ = false;
  return;
#else
  pinMode(ECG_CS, OUTPUT);
  pinMode(ECG_PWDN, OUTPUT);
  pinMode(ECG_RESET, OUTPUT);
  pinMode(ECG_START, OUTPUT);
  pinMode(ECG_DRDY, INPUT_PULLUP);

  digitalWrite(ECG_CS, HIGH);
  digitalWrite(ECG_START, LOW);
  digitalWrite(ECG_RESET, LOW);
  digitalWrite(ECG_PWDN, LOW);
  delay(20);

  ecgSpi.begin(ECG_SCLK, ECG_MISO, ECG_MOSI, ECG_CS);

  digitalWrite(ECG_PWDN, HIGH);
  delay(20);
  digitalWrite(ECG_RESET, HIGH);
  delay(20);
  command(CMD_RESET);
  delay(20);
  command(CMD_WAKEUP);
  delay(5);
  command(CMD_SDATAC);
  delay(2);

  deviceId_ = readRegister(REG_ID);
  if (deviceId_ == 0x00 || deviceId_ == 0xFF) {
    DebugSerial.printf("ADS1294 not ready, id=0x%02X\n", deviceId_);
    ready_ = false;
    return;
  }

  // Conservative bring-up setup: internal reference on, 250 SPS class data rate.
  writeRegister(REG_CONFIG1, 0x96);
  writeRegister(REG_CONFIG2, 0xC0);
  writeRegister(REG_CONFIG3, 0xE0);
#if ECG_ELECTRODE_COUNT == 3
  writeRegister(REG_CH1SET, ECG_CH_NORMAL_ELECTRODE);   // Lead I: LA - RA
  writeRegister(REG_CH2SET, ECG_CH_NORMAL_ELECTRODE);   // Lead II: LL - RA
  writeRegister(REG_CH3SET, ECG_CH_POWERDOWN_SHORTED);
  writeRegister(REG_CH4SET, ECG_CH_POWERDOWN_SHORTED);
#else
  writeRegister(REG_CH1SET, ECG_CH_NORMAL_ELECTRODE);
  writeRegister(REG_CH2SET, ECG_CH_NORMAL_ELECTRODE);
  writeRegister(REG_CH3SET, ECG_CH_NORMAL_ELECTRODE);
  writeRegister(REG_CH4SET, ECG_CH_NORMAL_ELECTRODE);
#endif
  configureDiagnostics();

  DebugSerial.printf("ADS1294 ready, id=0x%02X electrodes=%u cfg=%02X,%02X,%02X ch=%02X,%02X,%02X,%02X\n",
                deviceId_,
                (unsigned)ECG_ELECTRODE_COUNT,
                readRegister(REG_CONFIG1),
                readRegister(REG_CONFIG2),
                readRegister(REG_CONFIG3),
                readRegister(REG_CH1SET),
                readRegister(REG_CH2SET),
                readRegister(REG_CH3SET),
                readRegister(REG_CH4SET));
  DebugSerial.printf("ADS1294 diag rld=%u lead_off=%u loff=%02X rldp=%02X rldn=%02X loffp=%02X loffn=%02X\n",
                (unsigned)ENABLE_ECG_RLD_DRIVE,
                (unsigned)ENABLE_ECG_LEAD_OFF_DETECTION,
                readRegister(REG_LOFF),
                readRegister(REG_RLD_SENSP),
                readRegister(REG_RLD_SENSN),
                readRegister(REG_LOFF_SENSP),
                readRegister(REG_LOFF_SENSN));
  startContinuousRead();
  ready_ = true;
#endif
}

void EcgAds1294::configureDiagnostics()
{
#if ENABLE_ECG_RLD_DRIVE && ECG_ELECTRODE_COUNT != 3
  writeRegister(REG_CONFIG3, readRegister(REG_CONFIG3) | 0x0C);
  writeRegister(REG_RLD_SENSP, ECG_ACTIVE_ELECTRODE_MASK);
  writeRegister(REG_RLD_SENSN, ECG_ACTIVE_ELECTRODE_MASK);
#else
  writeRegister(REG_RLD_SENSP, 0x00);
  writeRegister(REG_RLD_SENSN, 0x00);
#endif

#if ENABLE_ECG_LEAD_OFF_DETECTION
  writeRegister(REG_LOFF, 0x00);
  writeRegister(REG_LOFF_SENSP, ECG_ACTIVE_ELECTRODE_MASK);
  writeRegister(REG_LOFF_SENSN, ECG_ACTIVE_ELECTRODE_MASK);
#else
  writeRegister(REG_LOFF_SENSP, 0x00);
  writeRegister(REG_LOFF_SENSN, 0x00);
#endif

  refreshLeadOffStatus(millis());
}

void EcgAds1294::refreshLeadOffStatus(uint32_t nowMs)
{
#if ENABLE_ECG_LEAD_OFF_DETECTION
  if (lastLeadOffReadMs_ != 0 && nowMs - lastLeadOffReadMs_ < 500) {
    return;
  }
  lastLeadOffReadMs_ = nowMs;
  leadOffPositive_ = readRegister(REG_LOFF_STATP) & ECG_ACTIVE_ELECTRODE_MASK;
  leadOffNegative_ = readRegister(REG_LOFF_STATN) & ECG_ACTIVE_ELECTRODE_MASK;
#else
  (void)nowMs;
  leadOffPositive_ = 0;
  leadOffNegative_ = 0;
#endif
}

void EcgAds1294::updateSampleDiagnostics(EcgSample &sample)
{
  sample.leadOffPositive = leadOffPositive_;
  sample.leadOffNegative = leadOffNegative_;
  sample.diagnosticFlags = ECG_DIAG_NONE;

#if ENABLE_ECG_RLD_DRIVE
  sample.diagnosticFlags |= ECG_DIAG_RLD_ENABLED;
#endif
#if ENABLE_ECG_LEAD_OFF_DETECTION
  sample.diagnosticFlags |= ECG_DIAG_LEAD_OFF_ENABLED;
  if ((leadOffPositive_ | leadOffNegative_) != 0) {
    sample.diagnosticFlags |= ECG_DIAG_LEAD_OFF;
  }
#endif

#if ENABLE_ECG_DC_SATURATION_DIAGNOSTIC
  for (size_t channel = 0; channel < ECG_DIAG_CHANNEL_COUNT; ++channel) {
    if (abs(sample.channels[channel]) >= ECG_ADC_SATURATION_LIMIT) {
      sample.saturationMask |= (1U << channel);
    }
  }
  if (sample.saturationMask != 0) {
    sample.diagnosticFlags |= ECG_DIAG_DC_SATURATION;
  }
#endif

  const float ch1 = (float)sample.channels[0];
  const float ch2 = (float)sample.channels[1];
  if (havePreviousFrame_) {
    const float common = 0.5f * (ch1 + ch2);
    const float previousCommon = 0.5f * ((float)previousCh1_ + (float)previousCh2_);
    const float differential = ch1 - ch2;
    const float previousDifferential = (float)previousCh1_ - (float)previousCh2_;
    const float commonStep = fabsf(common - previousCommon);
    const float differentialStep = fabsf(differential - previousDifferential);
    commonModeStepAvg_ = (commonModeStepAvg_ * 0.96f) + (commonStep * 0.04f);
    differentialStepAvg_ = (differentialStepAvg_ * 0.96f) + (differentialStep * 0.04f);
  }

  sample.commonModeStep = commonModeStepAvg_;
  sample.differentialStep = differentialStepAvg_;

#if ENABLE_ECG_NOISE_DIAGNOSTICS
  if (commonModeStepAvg_ > ECG_CABLE_NOISE_STEP_LIMIT) {
    sample.diagnosticFlags |= ECG_DIAG_CABLE_NOISE;
  }
#endif

#if ENABLE_ECG_RLD_STABILITY_DIAGNOSTIC
  if (commonModeStepAvg_ > ECG_RLD_UNSTABLE_STEP_LIMIT &&
      differentialStepAvg_ > ECG_RLD_UNSTABLE_STEP_LIMIT) {
    if (rldUnstableCount_ < 255) {
      ++rldUnstableCount_;
    }
  } else if (rldUnstableCount_ > 0) {
    --rldUnstableCount_;
  }

  if (rldUnstableCount_ >= 8) {
    sample.diagnosticFlags |= ECG_DIAG_RLD_UNSTABLE;
  }
#endif

  previousCh1_ = sample.channels[0];
  previousCh2_ = sample.channels[1];
  havePreviousFrame_ = true;
}

int32_t EcgAds1294::readInt24(uint8_t b0, uint8_t b1, uint8_t b2) const
{
  int32_t value = ((int32_t)b0 << 16) | ((int32_t)b1 << 8) | b2;
  if (value & 0x00800000) {
    value |= 0xFF000000;
  }
  return value;
}

bool EcgAds1294::readDataFrame(EcgSample &sample)
{
  uint8_t data[3 + (ECG_CHANNELS * 3)] = {};
  lockSpiDisplayGuard();
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(CMD_RDATA);
  delayMicroseconds(8);
  for (uint8_t &byte : data) {
    byte = ecgSpi.transfer(0x00);
  }
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
  unlockSpiDisplayGuard();

  sample.timestampMs = millis();
  sample.sequence = ++sequence_;
  sample.status = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
  for (size_t channel = 0; channel < ECG_CHANNELS; ++channel) {
    const size_t index = 3 + (channel * 3);
    sample.channels[channel] = readInt24(data[index], data[index + 1], data[index + 2]);
  }
  refreshLeadOffStatus(sample.timestampMs);
  updateSampleDiagnostics(sample);
  const bool allZero = sample.status == 0 &&
                       sample.channels[0] == 0 &&
                       sample.channels[1] == 0 &&
                       sample.channels[2] == 0 &&
                       sample.channels[3] == 0;
  if (allZero) {
    ++zeroFrames_;
  } else {
    zeroFrames_ = 0;
  }
  sample.valid = true;
  return true;
}

int EcgAds1294::drdyLevel() const
{
#if !ENABLE_ECG_SENSOR
  return HIGH;
#else
  return digitalRead(ECG_DRDY);
#endif
}

bool EcgAds1294::poll(EcgSample &sample)
{
  sample = {};
  if (!ready_) {
    return false;
  }

  const uint32_t nowUs = micros();
  if (nowUs - lastFrameUs_ < ECG_FRAME_PERIOD_US) {
    return false;
  }

  const bool ok = readDataFrame(sample);
  if (ok) {
    lastFrameUs_ = nowUs;
    ++samples_;
    if (zeroFrames_ >= 5) {
      DebugSerial.printf("ECG_RECOVER,reason=zero_frame,samples=%lu\n",
                    (unsigned long)samples_);
      startContinuousRead();
      return false;
    }
  } else {
    ++failures_;
  }
  return ok;
}
