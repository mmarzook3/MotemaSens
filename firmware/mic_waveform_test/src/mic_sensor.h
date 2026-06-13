#pragma once

#include <Arduino.h>

struct MicFrame {
  uint32_t timestampMs = 0;
  uint8_t acqSeq8 = 0;
  float normalizedLevel = 0.0f;
  float displayPoints[2] = {};
  size_t displayPointCount = 0;
  bool valid = false;
};

class MicSensor {
public:
  void begin();
  bool readFrame(MicFrame &frame);
  void resetDetectorState();
  float autoGain() const { return autoGain_; }

private:
  float dc_ = 0.0f;
  float lowFast_ = 0.0f;
  float lowSlow_ = 0.0f;
  float smoothedLevel_ = 0.0f;
  float displaySample_ = 0.0f;
  float autoGain_ = 10.0f;
};
