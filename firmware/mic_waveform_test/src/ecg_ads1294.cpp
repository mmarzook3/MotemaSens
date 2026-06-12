#include "ecg_ads1294.h"

#include "sensor_config.h"
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
static constexpr uint8_t REG_CH1SET = 0x05;
static constexpr uint8_t REG_CH2SET = 0x06;
static constexpr uint8_t REG_CH3SET = 0x07;
static constexpr uint8_t REG_CH4SET = 0x08;
static constexpr uint32_t ECG_FRAME_PERIOD_US = 10000;
static constexpr uint32_t ECG_RECOVERY_PERIOD_MS = 1000;

static SPIClass ecgSpi(HSPI);
static SPISettings ecgSettings(ECG_SPI_HZ, MSBFIRST, SPI_MODE1);

void EcgAds1294::command(uint8_t value)
{
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(value);
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
  delayMicroseconds(4);
}

uint8_t EcgAds1294::readRegister(uint8_t address)
{
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(CMD_RREG | (address & 0x1F));
  ecgSpi.transfer(0x00);
  const uint8_t value = ecgSpi.transfer(0x00);
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
  delayMicroseconds(4);
  return value;
}

void EcgAds1294::writeRegister(uint8_t address, uint8_t value)
{
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(CMD_WREG | (address & 0x1F));
  ecgSpi.transfer(0x00);
  ecgSpi.transfer(value);
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();
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
    Serial.printf("ADS1294 not ready, id=0x%02X\n", deviceId_);
    ready_ = false;
    return;
  }

  // Conservative bring-up setup: internal reference on, 250 SPS class data rate,
  // normal electrode input on all four channels with gain 6.
  writeRegister(REG_CONFIG1, 0x96);
  writeRegister(REG_CONFIG2, 0xC0);
  writeRegister(REG_CONFIG3, 0xE0);
  writeRegister(REG_CH1SET, 0x00);
  writeRegister(REG_CH2SET, 0x00);
  writeRegister(REG_CH3SET, 0x00);
  writeRegister(REG_CH4SET, 0x00);

  Serial.printf("ADS1294 ready, id=0x%02X cfg=%02X,%02X,%02X ch=%02X,%02X,%02X,%02X\n",
                deviceId_,
                readRegister(REG_CONFIG1),
                readRegister(REG_CONFIG2),
                readRegister(REG_CONFIG3),
                readRegister(REG_CH1SET),
                readRegister(REG_CH2SET),
                readRegister(REG_CH3SET),
                readRegister(REG_CH4SET));
  startContinuousRead();
  ready_ = true;
#endif
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
  ecgSpi.beginTransaction(ecgSettings);
  digitalWrite(ECG_CS, LOW);
  ecgSpi.transfer(CMD_RDATA);
  delayMicroseconds(8);
  for (uint8_t &byte : data) {
    byte = ecgSpi.transfer(0x00);
  }
  digitalWrite(ECG_CS, HIGH);
  ecgSpi.endTransaction();

  sample.timestampMs = millis();
  sample.sequence = ++sequence_;
  sample.status = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
  for (size_t channel = 0; channel < ECG_CHANNELS; ++channel) {
    const size_t index = 3 + (channel * 3);
    sample.channels[channel] = readInt24(data[index], data[index + 1], data[index + 2]);
  }
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
      Serial.printf("ECG_RECOVER,reason=zero_frame,samples=%lu\n",
                    (unsigned long)samples_);
      startContinuousRead();
      return false;
    }
  } else {
    ++failures_;
  }
  return ok;
}
