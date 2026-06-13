#pragma once

#include <Arduino.h>

static constexpr size_t ECG_CHANNELS = 4;

enum EcgDiagnosticFlags : uint16_t {
  ECG_DIAG_NONE = 0x0000,
  ECG_DIAG_LEAD_OFF = 0x0001,
  ECG_DIAG_DC_SATURATION = 0x0002,
  ECG_DIAG_CABLE_NOISE = 0x0004,
  ECG_DIAG_RLD_UNSTABLE = 0x0008,
  ECG_DIAG_RLD_ENABLED = 0x0010,
  ECG_DIAG_LEAD_OFF_ENABLED = 0x0020,
};

struct EcgSample {
  uint32_t timestampMs = 0;
  uint8_t acqSeq8 = 0;
  uint32_t sequence = 0;
  int32_t channels[ECG_CHANNELS] = {};
  uint32_t status = 0;
  uint8_t leadOffPositive = 0;
  uint8_t leadOffNegative = 0;
  uint8_t saturationMask = 0;
  uint16_t diagnosticFlags = ECG_DIAG_NONE;
  float commonModeStep = 0.0f;
  float differentialStep = 0.0f;
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
  void configureDiagnostics();
  void refreshLeadOffStatus(uint32_t nowMs);
  void updateSampleDiagnostics(EcgSample &sample);
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
  uint8_t leadOffPositive_ = 0;
  uint8_t leadOffNegative_ = 0;
  uint32_t lastLeadOffReadMs_ = 0;
  bool havePreviousFrame_ = false;
  int32_t previousCh1_ = 0;
  int32_t previousCh2_ = 0;
  float commonModeStepAvg_ = 0.0f;
  float differentialStepAvg_ = 0.0f;
  uint8_t rldUnstableCount_ = 0;
};
