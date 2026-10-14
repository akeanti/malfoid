// "Thumb on the emerald": capacitive pad (ESP32-S3 touch) or the BOOT button
// gates casting. Hysteresis plus slow baseline tracking for humidity drift.
#pragma once

#include <Arduino.h>

class Grip {
 public:
  void begin(int touchPin, int buttonPin);
  void calibrate();  // call with nothing touching the pad
  bool update();     // ~50 Hz; returns held()
  bool held() const { return held_; }
  float level() const { return level_; }  // 0..1, for the console's meter
  uint32_t baseline() const { return static_cast<uint32_t>(baseline_); }

 private:
  int touchPin_ = -1;
  int buttonPin_ = -1;
  float baseline_ = 0.0f;
  float level_ = 0.0f;
  bool touched_ = false;
  bool held_ = false;
};
