// TI DRV2605L haptic driver with an LRA (linear resonant actuator).
// Effects are ids from the DRV2605L's licensed ROM library 6 (LRA).
#pragma once

#include <Adafruit_DRV2605.h>
#include <Arduino.h>

namespace fx {
constexpr uint8_t kSharpTick = 24;
constexpr uint8_t kStrongClick = 1;
constexpr uint8_t kDoubleClick = 10;
constexpr uint8_t kTripleClick = 12;
constexpr uint8_t kBuzz = 47;
constexpr uint8_t kPulsing = 52;
constexpr uint8_t kRampDown = 70;
constexpr uint8_t kRampUp = 82;
}  // namespace fx

class Haptics {
 public:
  bool begin(int enablePin);
  // Plays up to 8 effects back to back; a 0 ends the list early.
  void play(const uint8_t* effects, size_t count);
  void play(uint8_t effect) { play(&effect, 1); }
  void standby();
  bool ok() const { return ok_; }

 private:
  Adafruit_DRV2605 drv_;
  int enablePin_ = -1;
  bool ok_ = false;
};
