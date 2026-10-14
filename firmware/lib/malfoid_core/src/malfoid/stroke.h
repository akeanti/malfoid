// Stroke preprocessing: turn a raw, unevenly sampled path into a fixed-size,
// position- and scale-independent shape that the recognizer can compare.
//
// The same steps are mirrored in docs/js/recognizer.js so the website demo
// behaves exactly like the wand.
#pragma once

#include <cstddef>

#include "geometry.h"

namespace malfoid {

constexpr size_t kStrokePoints = 32;

// Below this thinness a stroke is treated as a line (see normalize()).
// Chosen with tools/tune_recognizer.mjs.
constexpr float kOneDRatio = 0.2f;

struct Stroke {
  Vec2 pts[kStrokePoints];
};

// Total polyline length of `n` points.
float pathLength(const Vec2* pts, size_t n);

// Resamples a polyline into kStrokePoints points spaced evenly along its length
// (the resampling step of the $1 unistroke recognizer). Returns false when the
// path is degenerate (fewer than two points or zero length).
bool resample(const Vec2* pts, size_t n, Stroke& out);

// sqrt(minor / major) of the principal-axis variances around c:
// ~0 for a straight line at any tilt, ~1 for a circle.
float thinness(const Stroke& s, Vec2 c);

// Moves the centroid to the origin, then scales: line-like strokes uniformly
// (so a flick stays a line), 2-D shapes independently per axis to a unit box.
void normalize(Stroke& s);

// resample() followed by normalize().
bool prepare(const Vec2* pts, size_t n, Stroke& out);

}  // namespace malfoid
