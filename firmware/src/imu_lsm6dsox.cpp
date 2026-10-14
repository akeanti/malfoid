#include "imu_lsm6dsox.h"

#include "bus.h"

namespace {

// Register map (LSM6DSOX datasheet, section 8).
constexpr uint8_t COUNTER_BDR_REG1 = 0x0B;
constexpr uint8_t INT1_CTRL = 0x0D;
constexpr uint8_t WHO_AM_I = 0x0F;
constexpr uint8_t CTRL1_XL = 0x10;
constexpr uint8_t CTRL2_G = 0x11;
constexpr uint8_t CTRL3_C = 0x12;
constexpr uint8_t CTRL6_C = 0x15;
constexpr uint8_t CTRL7_G = 0x16;
constexpr uint8_t WAKE_UP_SRC = 0x1B;
constexpr uint8_t OUTX_L_G = 0x22;  // gyro X/Y/Z then accel X/Y/Z, 12 bytes
constexpr uint8_t TAP_CFG0 = 0x56;
constexpr uint8_t TAP_CFG2 = 0x58;
constexpr uint8_t WAKE_UP_THS = 0x5B;
constexpr uint8_t WAKE_UP_DUR = 0x5C;
constexpr uint8_t MD1_CFG = 0x5E;

constexpr uint8_t kWhoAmI = 0x6C;

// Field values.
constexpr uint8_t ODR_208HZ = 0x50;
constexpr uint8_t ODR_26HZ = 0x20;
constexpr uint8_t FS_XL_2G = 0x00;
constexpr uint8_t FS_XL_8G = 0x0C;
constexpr uint8_t FS_G_2000DPS = 0x0C;
constexpr uint8_t CTRL3_BDU = 0x40;
constexpr uint8_t CTRL3_IF_INC = 0x04;
constexpr uint8_t CTRL3_SW_RESET = 0x01;
constexpr uint8_t CTRL6_XL_HM_MODE = 0x10;  // 1 = high-performance OFF (low-power modes)
constexpr uint8_t DATAREADY_PULSED = 0x80;
constexpr uint8_t INT1_DRDY_G = 0x02;
constexpr uint8_t TAP_CFG0_LIR = 0x01;
constexpr uint8_t TAP_CFG0_INT_CLR_ON_READ = 0x40;
constexpr uint8_t TAP_CFG2_INTERRUPTS_ENABLE = 0x80;
constexpr uint8_t MD1_INT1_WU = 0x20;

// Sensitivities for the chosen full scales.
constexpr float kAccelGPerLsb = 0.244e-3f;                    // +/-8 g
constexpr float kGyroRadPerLsb = 70e-3f * 3.14159265f / 180;  // +/-2000 dps

}  // namespace

bool Lsm6dsox::write(uint8_t reg, uint8_t value) { return bus::writeReg(addr_, reg, value); }

bool Lsm6dsox::begin(uint8_t preferredAddress) {
  ok_ = false;
  const uint8_t candidates[2] = {preferredAddress, static_cast<uint8_t>(preferredAddress ^ 0x01)};
  for (uint8_t a : candidates) {
    uint8_t who = 0;
    if (bus::readReg(a, WHO_AM_I, who) && who == kWhoAmI) {
      addr_ = a;
      ok_ = true;
      break;
    }
  }
  if (!ok_) return false;

  write(CTRL3_C, CTRL3_SW_RESET);
  for (int i = 0; i < 20; ++i) {
    uint8_t c3 = CTRL3_SW_RESET;
    delay(2);
    if (bus::readReg(addr_, CTRL3_C, c3) && !(c3 & CTRL3_SW_RESET)) break;
  }
  clearWakeSource();
  return configureActive();
}

bool Lsm6dsox::configureActive() {
  bool good = true;
  good &= write(CTRL3_C, CTRL3_BDU | CTRL3_IF_INC);
  good &= write(CTRL6_C, 0x00);  // accelerometer high-performance mode
  good &= write(CTRL7_G, 0x00);  // gyro high-performance mode
  good &= write(MD1_CFG, 0x00);  // no wake-up routing while awake
  good &= write(TAP_CFG2, 0x00);
  good &= write(TAP_CFG0, 0x00);
  good &= write(CTRL1_XL, ODR_208HZ | FS_XL_8G);
  good &= write(CTRL2_G, ODR_208HZ | FS_G_2000DPS);
  good &= write(COUNTER_BDR_REG1, DATAREADY_PULSED);
  good &= write(INT1_CTRL, INT1_DRDY_G);  // gyro and accel share the ODR, one pulse per sample
  ok_ = good;
  return good;
}

bool Lsm6dsox::read(ImuSample& s) {
  uint8_t b[12];
  if (!bus::readRegs(addr_, OUTX_L_G, b, sizeof(b))) return false;
  auto raw = [&](int i) { return static_cast<int16_t>(b[2 * i] | (b[2 * i + 1] << 8)); };
  s.gx = raw(0) * kGyroRadPerLsb;
  s.gy = raw(1) * kGyroRadPerLsb;
  s.gz = raw(2) * kGyroRadPerLsb;
  s.ax = raw(3) * kAccelGPerLsb;
  s.ay = raw(4) * kAccelGPerLsb;
  s.az = raw(5) * kAccelGPerLsb;
  return true;
}

bool Lsm6dsox::configureWakeOnMotion() {
  bool good = true;
  good &= write(INT1_CTRL, 0x00);                // stop data-ready pulses
  good &= write(CTRL2_G, 0x00);                  // gyro power-down
  good &= write(CTRL6_C, CTRL6_XL_HM_MODE);      // accel low-power family
  good &= write(CTRL1_XL, ODR_26HZ | FS_XL_2G);  // 26 Hz, finest wake threshold steps
  good &= write(TAP_CFG0, TAP_CFG0_LIR | TAP_CFG0_INT_CLR_ON_READ);  // latch until read
  good &= write(WAKE_UP_DUR, 0x00);              // one sample above threshold is enough
  good &= write(WAKE_UP_THS, 0x04);              // 4 x (2 g / 64) = 125 mg slope
  good &= write(TAP_CFG2, TAP_CFG2_INTERRUPTS_ENABLE);
  good &= write(MD1_CFG, MD1_INT1_WU);
  delay(80);  // let the slope filter settle so we don't wake on our own reconfiguration
  clearWakeSource();
  return good;
}

uint8_t Lsm6dsox::clearWakeSource() {
  uint8_t src = 0;
  bus::readReg(addr_, WAKE_UP_SRC, src);
  return src;
}
