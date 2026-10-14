// Minimal register-level driver for the ST LSM6DSOX 6-axis IMU.
//
// Active mode: 208 Hz, +/-8 g, +/-2000 dps, block data update, a 75 us
// data-ready pulse on INT1 per sample.
// Sleep mode:  accelerometer only, 26 Hz low-power, gyro off; a latched
// wake-up (slope) interrupt on INT1 wakes the ESP32-S3 when you pick it up.
#pragma once

#include <Arduino.h>

struct ImuSample {
  float gx, gy, gz;  // rad/s, chip axes
  float ax, ay, az;  // g, chip axes
};

class Lsm6dsox {
 public:
  bool begin(uint8_t preferredAddress);
  bool configureActive();
  bool read(ImuSample& s);
  bool configureWakeOnMotion();
  uint8_t clearWakeSource();  // reads WAKE_UP_SRC, which also releases a latched INT1
  bool ok() const { return ok_; }
  uint8_t address() const { return addr_; }

 private:
  bool write(uint8_t reg, uint8_t value);
  uint8_t addr_ = 0x6A;
  bool ok_ = false;
};
