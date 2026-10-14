// The glowing tip: one SK6812 at the end of the board, piped up the shaft.
//
// A base layer (emerald breathing, the Lumen torch, or dark) with short
// overlays on top (spell flash, fizzle, low-battery warning).
#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class TipLight {
 public:
  void begin(int dataPin, int powerPin);
  void setCasting(bool on, uint32_t now);
  void flash(uint32_t rgb, uint16_t ms, bool shimmer, uint32_t now);
  void fizzle(uint32_t now);
  void warn(uint32_t now);
  void setTorch(bool on) { torch_ = on; }
  bool torch() const { return torch_; }
  void update(uint32_t now);
  void powerDown();

 private:
  enum class Overlay : uint8_t { None, Flash, Fizzle, Warn };
  void show(uint8_t r, uint8_t g, uint8_t b);
  void startOverlay(Overlay o, uint16_t ms, uint32_t now);

  Adafruit_NeoPixel px_{1, -1, NEO_GRB + NEO_KHZ800};
  int powerPin_ = -1;
  bool casting_ = false;
  bool torch_ = false;
  uint32_t castStart_ = 0;
  Overlay overlay_ = Overlay::None;
  uint32_t overlayStart_ = 0;
  uint16_t overlayMs_ = 0;
  uint32_t flashRgb_ = 0;
  bool shimmer_ = false;
  uint32_t shown_ = 0xFFFFFFFF;
};
