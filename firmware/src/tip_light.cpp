#include "tip_light.h"

#include <math.h>

namespace {

inline uint8_t scale(uint8_t c, float k) {
  const float v = c * k;
  return v <= 0 ? 0 : (v >= 255 ? 255 : static_cast<uint8_t>(v));
}

inline uint8_t R(uint32_t rgb) { return (rgb >> 16) & 0xFF; }
inline uint8_t G(uint32_t rgb) { return (rgb >> 8) & 0xFF; }
inline uint8_t B(uint32_t rgb) { return rgb & 0xFF; }

}  // namespace

void TipLight::begin(int dataPin, int powerPin) {
  powerPin_ = powerPin;
  if (powerPin_ >= 0) {
    pinMode(powerPin_, OUTPUT);
    digitalWrite(powerPin_, HIGH);
    delay(2);
  }
  px_.setPin(dataPin);
  px_.begin();
  px_.clear();
  px_.show();
}

void TipLight::show(uint8_t r, uint8_t g, uint8_t b) {
  const uint32_t c = Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::Color(r, g, b));
  if (c == shown_) return;
  shown_ = c;
  px_.setPixelColor(0, c);
  px_.show();
}

void TipLight::startOverlay(Overlay o, uint16_t ms, uint32_t now) {
  overlay_ = o;
  overlayMs_ = ms;
  overlayStart_ = now;
}

void TipLight::setCasting(bool on, uint32_t now) {
  if (on && !casting_) castStart_ = now;
  casting_ = on;
}

void TipLight::flash(uint32_t rgb, uint16_t ms, bool shimmer, uint32_t now) {
  flashRgb_ = rgb;
  shimmer_ = shimmer;
  startOverlay(Overlay::Flash, ms, now);
}

void TipLight::fizzle(uint32_t now) { startOverlay(Overlay::Fizzle, 450, now); }
void TipLight::warn(uint32_t now) { startOverlay(Overlay::Warn, 1200, now); }

void TipLight::update(uint32_t now) {
  if (overlay_ != Overlay::None && now - overlayStart_ >= overlayMs_) overlay_ = Overlay::None;

  if (overlay_ != Overlay::None) {
    const float t = static_cast<float>(now - overlayStart_) / overlayMs_;
    switch (overlay_) {
      case Overlay::Flash: {
        float k = (1.0f - t) * (1.0f - t);
        if (shimmer_) k *= 0.6f + 0.4f * static_cast<float>(random(0, 1000)) / 1000.0f;
        show(scale(R(flashRgb_), k), scale(G(flashRgb_), k), scale(B(flashRgb_), k));
        return;
      }
      case Overlay::Fizzle: {
        const float k = (1.0f - t) * (random(0, 2) ? 0.9f : 0.15f);
        show(scale(220, k), scale(30, k), scale(20, k));
        return;
      }
      case Overlay::Warn: {
        const bool on = static_cast<int>(t * 6) % 2 == 0;
        show(on ? 200 : 0, on ? 110 : 0, 0);
        return;
      }
      case Overlay::None:
        break;
    }
  }

  if (casting_) {  // silver gathers while you draw
    const float k = 0.15f + 0.85f * fminf(1.0f, (now - castStart_) / 600.0f);
    show(scale(200, k), scale(212, k), scale(224, k));
  } else if (torch_) {
    show(235, 200, 160);
  } else {  // slow emerald breathing, very dim
    const float phase = (now % 4000) / 4000.0f;
    const float k = 0.05f + 0.10f * (0.5f - 0.5f * cosf(phase * 2.0f * static_cast<float>(M_PI)));
    show(scale(40, k), scale(220, k), scale(120, k));
  }
}

void TipLight::powerDown() {
  px_.clear();
  px_.show();
  shown_ = 0;
  if (powerPin_ >= 0) digitalWrite(powerPin_, LOW);
}
