#include "fuel_gauge.h"

#include "bus.h"

namespace {
constexpr uint8_t kAddress = 0x36;
constexpr uint8_t VCELL = 0x02;    // 78.125 uV / LSB
constexpr uint8_t SOC = 0x04;      // 1/256 % / LSB
constexpr uint8_t VERSION = 0x08;  // 0x001x
constexpr uint8_t CRATE = 0x16;    // signed, 0.208 % / hour / LSB
}  // namespace

bool Max17048::begin() {
  uint16_t version = 0;
  ok_ = bus::readReg16BE(kAddress, VERSION, version) && (version & 0xFFF0) == 0x0010;
  return ok_;
}

bool Max17048::read(Battery& out) {
  uint16_t vcell = 0, soc = 0, crate = 0;
  if (!ok_ || !bus::readReg16BE(kAddress, VCELL, vcell) || !bus::readReg16BE(kAddress, SOC, soc)) {
    out.present = false;
    return false;
  }
  bus::readReg16BE(kAddress, CRATE, crate);
  out.present = true;
  out.volts = vcell * 78.125e-6f;
  out.percent = soc / 256.0f;
  if (out.percent > 100.0f) out.percent = 100.0f;
  out.ratePctPerHour = static_cast<int16_t>(crate) * 0.208f;
  out.charging = out.ratePctPerHour > 0.5f;
  return true;
}
