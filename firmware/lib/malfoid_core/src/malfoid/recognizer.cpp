#include "recognizer.h"

#include <algorithm>
#include <cstring>

#include "spellbook_data.h"

namespace malfoid {

static_assert(spellbook::kPoints == kStrokePoints, "spellbook.json 'points' must match kStrokePoints");

void Recognizer::clear() { count_ = 0; }

size_t Recognizer::loadBuiltins() {
  size_t added = 0;
  Vec2 path[64];
  for (const auto& def : spellbook::kTemplates) {
    const size_t n = std::min<size_t>(def.count, sizeof(path) / sizeof(path[0]));
    for (size_t i = 0; i < n; ++i) path[i] = {def.xy[2 * i], def.xy[2 * i + 1]};
    if (addPath(spellbook::kSpells[def.spell].id, path, n, false)) ++added;
  }
  return added;
}

bool Recognizer::add(const char* id, const Stroke& normalized, bool custom) {
  if (count_ >= kMaxTemplates || id == nullptr || id[0] == '\0') return false;
  Template& t = templates_[count_];
  std::strncpy(t.id, id, sizeof(t.id) - 1);
  t.id[sizeof(t.id) - 1] = '\0';
  t.stroke = normalized;
  t.custom = custom;
  ++count_;
  return true;
}

bool Recognizer::addPath(const char* id, const Vec2* pts, size_t n, bool custom) {
  Stroke s;
  return prepare(pts, n, s) && add(id, s, custom);
}

size_t Recognizer::removeCustom(const char* id) {
  size_t kept = 0;
  for (size_t i = 0; i < count_; ++i) {
    const bool drop = templates_[i].custom && (id == nullptr || std::strcmp(templates_[i].id, id) == 0);
    if (!drop) templates_[kept++] = templates_[i];
  }
  const size_t removed = count_ - kept;
  count_ = kept;
  return removed;
}

float Recognizer::dtw(const Stroke& a, const Stroke& b, int window) {
  constexpr size_t N = kStrokePoints;
  constexpr float kInf = 1e30f;
  float prev[N + 1];
  float cur[N + 1];
  for (size_t j = 0; j <= N; ++j) prev[j] = kInf;
  prev[0] = 0.0f;

  for (size_t i = 1; i <= N; ++i) {
    cur[0] = kInf;
    for (size_t j = 1; j <= N; ++j) {
      const int gap = static_cast<int>(i) - static_cast<int>(j);
      if (gap > window || -gap > window) {
        cur[j] = kInf;
        continue;
      }
      const float best = std::min(prev[j - 1], std::min(prev[j], cur[j - 1]));
      cur[j] = distance(a.pts[i - 1], b.pts[j - 1]) + best;
    }
    std::memcpy(prev, cur, sizeof(prev));
  }
  return prev[N] / static_cast<float>(N);
}

float Recognizer::score(float distance) {
  const float s = 1.0f - distance / kZeroScoreDistance;
  return std::max(0.0f, std::min(1.0f, s));
}

Match Recognizer::classify(const Stroke& normalized) const {
  Match m;
  float dist[kMaxTemplates];
  for (size_t i = 0; i < count_; ++i) {
    dist[i] = dtw(normalized, templates_[i].stroke, window_);
    if (dist[i] < m.distance) {
      m.distance = dist[i];
      m.index = static_cast<int>(i);
    }
  }
  if (m.index < 0) return m;
  m.score = score(m.distance);

  const char* bestId = templates_[m.index].id;
  for (size_t i = 0; i < count_; ++i) {
    if (std::strcmp(templates_[i].id, bestId) != 0 && dist[i] < m.runnerUpDistance) {
      m.runnerUpDistance = dist[i];
      m.runnerUp = static_cast<int>(i);
    }
  }
  if (m.runnerUp >= 0) m.runnerUpScore = score(m.runnerUpDistance);
  return m;
}

}  // namespace malfoid
