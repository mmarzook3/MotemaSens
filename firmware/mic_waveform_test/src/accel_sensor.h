#pragma once

#include <Arduino.h>

struct AccelSample {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t rawZ = 0;
  bool valid = false;
};

class AccelSensor {
public:
  bool begin();
  bool poll(uint32_t now, AccelSample &sample);
  bool ready() const { return ready_; }
  uint32_t sampleCount() const { return samplesSinceDebug_; }
  uint32_t failureCount() const { return failuresSinceDebug_; }
  void resetDebugCounters();

private:
  bool readRegister(uint8_t reg, uint8_t *buffer, uint8_t length);
  bool readByte(uint8_t address, uint8_t reg, uint8_t &value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readAccel(AccelSample &sample);

  bool ready_ = false;
  uint8_t address_ = 0x6A;
  uint32_t lastReadMs_ = 0;
  uint32_t samplesSinceDebug_ = 0;
  uint32_t failuresSinceDebug_ = 0;
};
