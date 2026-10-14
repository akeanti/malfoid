#include "bus.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace bus {

namespace {
SemaphoreHandle_t gMutex = nullptr;
}

void begin(int sda, int scl, uint32_t hz) {
  if (gMutex == nullptr) gMutex = xSemaphoreCreateRecursiveMutex();
  Wire.begin(sda, scl, hz);
}

Lock::Lock() { xSemaphoreTakeRecursive(gMutex, portMAX_DELAY); }
Lock::~Lock() { xSemaphoreGiveRecursive(gMutex); }

bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Lock lock;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
  Lock lock;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<uint16_t>(addr), len, true) != len) return false;
  for (size_t i = 0; i < len; ++i) buf[i] = static_cast<uint8_t>(Wire.read());
  return true;
}

bool readReg(uint8_t addr, uint8_t reg, uint8_t& value) { return readRegs(addr, reg, &value, 1); }

bool readReg16BE(uint8_t addr, uint8_t reg, uint16_t& value) {
  uint8_t b[2];
  if (!readRegs(addr, reg, b, 2)) return false;
  value = static_cast<uint16_t>((b[0] << 8) | b[1]);
  return true;
}

}  // namespace bus
