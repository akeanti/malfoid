#include "haptics.h"

#include "bus.h"

namespace {
constexpr uint8_t kModeInternalTrigger = 0x00;
constexpr uint8_t kModeStandbyBit = 0x40;
constexpr uint8_t kLibraryLra = 6;
constexpr size_t kSlots = 8;
}  // namespace

bool Haptics::begin(int enablePin) {
  enablePin_ = enablePin;
  if (enablePin_ >= 0) {
    pinMode(enablePin_, OUTPUT);
    digitalWrite(enablePin_, HIGH);
    delay(1);
  }
  bus::Lock lock;
  ok_ = drv_.begin(&Wire);
  if (!ok_) return false;
  drv_.useLRA();
  drv_.selectLibrary(kLibraryLra);
  drv_.setMode(kModeInternalTrigger);
  return true;
}

void Haptics::play(const uint8_t* effects, size_t count) {
  if (!ok_ || count == 0) return;
  bus::Lock lock;
  drv_.setMode(kModeInternalTrigger);  // also clears standby
  size_t slot = 0;
  for (; slot < count && slot < kSlots && effects[slot] != 0; ++slot) drv_.setWaveform(slot, effects[slot]);
  if (slot < kSlots) drv_.setWaveform(slot, 0);
  drv_.go();
}

void Haptics::standby() {
  if (ok_) {
    bus::Lock lock;
    drv_.stop();
    drv_.setMode(kModeStandbyBit);
  }
  if (enablePin_ >= 0) digitalWrite(enablePin_, LOW);
}
