// Maxim MAX17048 ModelGauge fuel gauge: state of charge without a sense resistor.
#pragma once

#include <Arduino.h>

struct Battery {
  bool present = false;
  float volts = 0.0f;
  float percent = 0.0f;
  float ratePctPerHour = 0.0f;  // > 0 while charging
  bool charging = false;
};

class Max17048 {
 public:
  bool begin();
  bool read(Battery& out);
  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};
