#pragma once

#include <Arduino.h>
#include <stdarg.h>

class DebugSerialPort {
public:
  void begin(unsigned long baud)
  {
    Serial.begin(baud);
    Serial0.begin(baud);
  }

  int available()
  {
    return Serial.available() + Serial0.available();
  }

  int read()
  {
    if (Serial.available() > 0) {
      return Serial.read();
    }
    return Serial0.read();
  }

  size_t print(const char *value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t print(char value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t print(int value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t print(unsigned int value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t print(long value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t print(unsigned long value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t print(const String &value)
  {
    const size_t a = Serial.print(value);
    Serial0.print(value);
    return a;
  }

  size_t println()
  {
    const size_t a = Serial.println();
    Serial0.println();
    return a;
  }

  template <typename T>
  size_t println(const T &value)
  {
    const size_t a = Serial.println(value);
    Serial0.println(value);
    return a;
  }

  int printf(const char *format, ...)
  {
    char buffer[384] = {};
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
    Serial0.print(buffer);
    return written;
  }
};

extern DebugSerialPort DebugSerial;
