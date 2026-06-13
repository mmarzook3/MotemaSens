#include "accel_sensor.h"

#include "debug_serial.h"
#include "sensor_config.h"
#include <Wire.h>

static constexpr uint32_t ACCEL_SAMPLE_PERIOD_MS = 8;

static int16_t qmiInt16(uint8_t lo, uint8_t hi)
{
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

bool AccelSensor::writeRegister(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(address_);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool AccelSensor::readRegister(uint8_t reg, uint8_t *buffer, uint8_t length)
{
  Wire.beginTransmission(address_);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(address_, length);
  if (received != length) {
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool AccelSensor::readByte(uint8_t address, uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(address, (uint8_t)1) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

bool AccelSensor::begin()
{
#if !ENABLE_ACCEL_SENSOR
  ready_ = false;
  return false;
#else
  Wire.begin(IMU_SDA, IMU_SCL);
  Wire.setClock(400000);
  delay(20);

  uint8_t who = 0;
  for (uint8_t address : { (uint8_t)0x6A, (uint8_t)0x6B }) {
    if (readByte(address, 0x00, who) && who == 0x05) {
      address_ = address;
      break;
    }
  }

  if (who != 0x05) {
    DebugSerial.println("QMI8658 not found");
    ready_ = false;
    return false;
  }

  writeRegister(0x02, 0x80);
  delay(50);

  ready_ = writeRegister(0x02, 0x60) &&
           writeRegister(0x08, 0x00) &&
           writeRegister(0x03, 0x23) &&
           writeRegister(0x04, 0x43) &&
           writeRegister(0x08, 0x03);

  if (ready_) {
    DebugSerial.printf("QMI8658 ready at 0x%02X, accel +/-8g 1000Hz\n", address_);
  } else {
    DebugSerial.println("QMI8658 init failed");
  }
  return ready_;
#endif
}

bool AccelSensor::readAccel(AccelSample &sample)
{
  uint8_t status = 0;
  if (!readRegister(0x2E, &status, 1) || (status & 0x01) == 0) {
    return false;
  }

  uint8_t buffer[6] = {};
  if (!readRegister(0x35, buffer, sizeof(buffer))) {
    return false;
  }

  sample.rawX = qmiInt16(buffer[0], buffer[1]);
  sample.rawY = qmiInt16(buffer[2], buffer[3]);
  sample.rawZ = qmiInt16(buffer[4], buffer[5]);
  sample.x = (float)sample.rawX / 4096.0f;
  sample.y = (float)sample.rawY / 4096.0f;
  sample.z = (float)sample.rawZ / 4096.0f;
  sample.valid = true;
  return true;
}

bool AccelSensor::poll(uint32_t now, AccelSample &sample)
{
  sample = {};
  if (!ready_ || now - lastReadMs_ < ACCEL_SAMPLE_PERIOD_MS) {
    return false;
  }
  lastReadMs_ = now;

  sample.valid = readAccel(sample);
  if (sample.valid) {
    ++samplesSinceDebug_;
  } else {
    ++failuresSinceDebug_;
  }
  return true;
}

void AccelSensor::resetDebugCounters()
{
  samplesSinceDebug_ = 0;
  failuresSinceDebug_ = 0;
}
