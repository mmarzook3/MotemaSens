#pragma once

#include <Arduino.h>

struct AccelSample {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t rawZ = 0;
  uint8_t diagnosticFlags = 0;
  bool xHealthy = true;
  bool yHealthy = true;
  bool zHealthy = true;
  bool valid = false;
};

static constexpr uint8_t ACCEL_DIAG_X_SATURATED = 0x01;
static constexpr uint8_t ACCEL_DIAG_Y_SATURATED = 0x02;
static constexpr uint8_t ACCEL_DIAG_Z_SATURATED = 0x04;
static constexpr uint8_t ACCEL_DIAG_RECOVERING = 0x08;

class AccelSensor {
public:
  bool begin();
  bool poll(uint32_t now, AccelSample &sample);
  bool ready() const { return ready_; }
  uint32_t sampleCount() const { return samplesSinceDebug_; }
  uint32_t failureCount() const { return failuresSinceDebug_; }
  uint8_t diagnosticFlags() const { return diagnosticFlags_; }
  void resetDebugCounters();

private:
  bool readRegister(uint8_t reg, uint8_t *buffer, uint8_t length);
  bool readByte(uint8_t address, uint8_t reg, uint8_t &value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readAccel(AccelSample &sample);
  bool configure();
  void recover(uint32_t now);

  bool ready_ = false;
  uint8_t address_ = 0x6A;
  uint32_t lastReadMs_ = 0;
  uint32_t samplesSinceDebug_ = 0;
  uint32_t failuresSinceDebug_ = 0;
  uint32_t saturatedStreak_ = 0;
  uint32_t lastRecoveryMs_ = 0;
  uint8_t recoveryAttempts_ = 0;
  uint8_t diagnosticFlags_ = 0;
};
