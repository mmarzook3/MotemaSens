#pragma once

#include <Arduino.h>

static constexpr size_t ECG_CHANNELS = 4;

struct EcgSample {
  uint32_t timestampMs = 0;
  uint32_t sequence = 0;
  int32_t channels[ECG_CHANNELS] = {};
  uint32_t status = 0;
  bool valid = false;
};

class EcgAds1294 {
public:
  void begin();
  bool poll(EcgSample &sample);
  bool ready() const { return ready_; }
  uint8_t deviceId() const { return deviceId_; }
  int drdyLevel() const;
  uint32_t samples() const { return samples_; }
  uint32_t failures() const { return failures_; }

private:
  void command(uint8_t value);
  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  void startContinuousRead();
  bool readDataFrame(EcgSample &sample);
  int32_t readInt24(uint8_t b0, uint8_t b1, uint8_t b2) const;

  bool ready_ = false;
  uint8_t deviceId_ = 0;
  uint32_t sequence_ = 0;
  uint32_t samples_ = 0;
  uint32_t failures_ = 0;
  uint32_t lastFrameUs_ = 0;
  uint32_t lastRecoveryMs_ = 0;
  uint32_t lastRecoverySamples_ = 0;
  uint8_t zeroFrames_ = 0;
};
