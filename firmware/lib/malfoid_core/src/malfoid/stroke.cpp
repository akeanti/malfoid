#include "stroke.h"

#include <algorithm>
#include <cmath>

namespace malfoid {

float pathLength(const Vec2* pts, size_t n) {
  float total = 0.0f;
  for (size_t i = 1; i < n; ++i) total += distance(pts[i - 1], pts[i]);
  return total;
}

bool resample(const Vec2* pts, size_t n, Stroke& out) {
  if (n < 2) return false;
  const float total = pathLength(pts, n);
  if (total <= 1e-6f) return false;

  const float step = total / static_cast<float>(kStrokePoints - 1);
  out.pts[0] = pts[0];
  size_t k = 1;
  float carried = 0.0f;  // distance walked since the last emitted point
  Vec2 prev = pts[0];

  for (size_t i = 1; i < n && k < kStrokePoints; ++i) {
    const Vec2 cur = pts[i];
    float seg = distance(prev, cur);
    while (seg > 0.0f && carried + seg >= step && k < kStrokePoints) {
      const float t = (step - carried) / seg;
      const Vec2 q = prev + (cur - prev) * t;
      out.pts[k++] = q;
      prev = q;
      seg = distance(prev, cur);
      carried = 0.0f;
    }
    carried += seg;
    prev = cur;
  }
  // Floating-point rounding can leave the final point(s) unemitted.
  while (k < kStrokePoints) out.pts[k++] = pts[n - 1];
  return true;
}

float thinness(const Stroke& s, Vec2 c) {
  float sxx = 0.0f, syy = 0.0f, sxy = 0.0f;
  for (const Vec2& p : s.pts) {
    const Vec2 d = p - c;
    sxx += d.x * d.x;
    syy += d.y * d.y;
    sxy += d.x * d.y;
  }
  const float mean = 0.5f * (sxx + syy);
  const float half = 0.5f * (sxx - syy);
  const float spread = std::sqrt(half * half + sxy * sxy);
  const float major = mean + spread;
  const float minor = std::max(0.0f, mean - spread);
  return major > 0.0f ? std::sqrt(minor / major) : 0.0f;
}

void normalize(Stroke& s) {
  Vec2 c;
  float minX = s.pts[0].x, maxX = s.pts[0].x, minY = s.pts[0].y, maxY = s.pts[0].y;
  for (const Vec2& p : s.pts) {
    c = c + p;
    minX = std::min(minX, p.x);
    maxX = std::max(maxX, p.x);
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }
  c = c * (1.0f / static_cast<float>(kStrokePoints));
  const float w = maxX - minX;
  const float h = maxY - minY;
  float size = std::max(w, h);
  if (size < 1e-6f) size = 1.0f;

  // Thin, line-like strokes keep their aspect ratio; 2-D shapes are stretched
  // to a square so a wide M still looks like an M.
  const bool oneD = w < 1e-6f || h < 1e-6f || thinness(s, c) < kOneDRatio;
  const float kx = oneD ? 1.0f / size : 1.0f / w;
  const float ky = oneD ? 1.0f / size : 1.0f / h;
  for (Vec2& p : s.pts) p = {(p.x - c.x) * kx, (p.y - c.y) * ky};
}

bool prepare(const Vec2* pts, size_t n, Stroke& out) {
  if (!resample(pts, n, out)) return false;
  normalize(out);
  return true;
}

}  // namespace malfoid
