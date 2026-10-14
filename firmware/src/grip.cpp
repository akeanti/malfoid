#include "grip.h"

#include "malfoid_config.h"

void Grip::begin(int touchPin, int buttonPin) {
  touchPin_ = touchPin;
  buttonPin_ = buttonPin;
  if (buttonPin_ >= 0) pinMode(buttonPin_, INPUT_PULLUP);
  calibrate();
}

void Grip::calibrate() {
  if (touchPin_ < 0) return;
  touchRead(touchPin_);  // first read initializes the touch peripheral
  delay(20);
  float sum = 0.0f;
  constexpr int kSamples = 16;
  for (int i = 0; i < kSamples; ++i) {
    sum += touchRead(touchPin_);
    delay(4);
  }
  baseline_ = sum / kSamples;
  touched_ = false;
}

bool Grip::update() {
  const bool button = buttonPin_ >= 0 && digitalRead(buttonPin_) == LOW;
  if (touchPin_ >= 0 && baseline_ > 0.0f) {
    const float raw = static_cast<float>(touchRead(touchPin_));
    const float rise = (raw - baseline_) / baseline_;
    if (!touched_ && rise > cfg::kTouchOnRatio) touched_ = true;
    else if (touched_ && rise < cfg::kTouchOffRatio) touched_ = false;
    if (!touched_) baseline_ += (raw - baseline_) * 0.002f;
    level_ = constrain(rise / (2.0f * cfg::kTouchOnRatio), 0.0f, 1.0f);
  }
  held_ = button || touched_;
  return held_;
}
