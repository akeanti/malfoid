// Template-matching spell recognizer.
//
// Each spell is one or more normalized template strokes. A drawn stroke is
// compared against every template with dynamic time warping (DTW) inside a
// Sakoe-Chiba band, which tolerates uneven drawing speed without letting the
// alignment cheat across the whole shape. Direction matters on purpose:
// an upward flick and a downward flick are different spells.
#pragma once

#include <cstddef>

#include "stroke.h"

namespace malfoid {

struct Template {
  char id[16];    // spell id, lowercase, NUL-terminated
  Stroke stroke;  // normalized
  bool custom;    // recorded by the owner rather than built in
};

struct Match {
  int index = -1;           // best template, or -1 when there are no templates
  float distance = 1e9f;    // mean DTW cost per point, in normalized units
  float score = 0.0f;       // 0..1, see Recognizer::score()
  int runnerUp = -1;        // best template that belongs to a *different* spell
  float runnerUpDistance = 1e9f;
  float runnerUpScore = 0.0f;
};

class Recognizer {
 public:
  static constexpr size_t kMaxTemplates = 64;
  static constexpr int kDefaultWindow = 4;
  // Tuned with tools/tune_recognizer.mjs: ~97% of sloppy synthetic casts accepted
  // correctly, ~0.1% accepted as the wrong spell.
  static constexpr float kZeroScoreDistance = 0.35f;  // mean DTW distance that scores 0
  static constexpr float kAcceptScore = 0.55f;
  static constexpr float kMinMargin = 0.05f;  // best must beat the runner-up spell by this

  // True when the match is confident and unambiguous enough to cast.
  static bool accepts(const Match& m, float minScore = kAcceptScore, float minMargin = kMinMargin) {
    return m.index >= 0 && m.score >= minScore && m.score - m.runnerUpScore >= minMargin;
  }

  void clear();

  // Loads the templates from spellbook_data.h. Returns how many were added.
  size_t loadBuiltins();

  bool add(const char* id, const Stroke& normalized, bool custom);
  bool addPath(const char* id, const Vec2* pts, size_t n, bool custom);

  // Removes custom templates with this id (all custom templates when id is null).
  size_t removeCustom(const char* id);

  Match classify(const Stroke& normalized) const;

  size_t size() const { return count_; }
  const Template& at(size_t i) const { return templates_[i]; }
  void setWindow(int window) { window_ = window < 1 ? 1 : window; }

  static float dtw(const Stroke& a, const Stroke& b, int window);
  static float score(float distance);

 private:
  Template templates_[kMaxTemplates] = {};
  size_t count_ = 0;
  int window_ = kDefaultWindow;
};

}  // namespace malfoid
