// Shared I2C bus. The sensor task and the main loop both talk on it, so every
// transaction holds a recursive mutex.
#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace bus {

void begin(int sda, int scl, uint32_t hz);

class Lock {
 public:
  Lock();
  ~Lock();
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;
};

bool writeReg(uint8_t addr, uint8_t reg, uint8_t value);
bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len);
bool readReg(uint8_t addr, uint8_t reg, uint8_t& value);
bool readReg16BE(uint8_t addr, uint8_t reg, uint16_t& value);

}  // namespace bus
