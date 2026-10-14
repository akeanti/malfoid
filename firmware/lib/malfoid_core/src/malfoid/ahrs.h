// Orientation tracking and "where is the tip pointing" projection.
//
// World frame: z up, x/y horizontal (x = heading at power-on, y = to its left).
// Body frame:  +X runs along the wand towards the tip.
//
// Because we track the *direction of the tip* rather than integrating raw gyro
// axes, rolling your wrist while casting does not distort the drawn shape.
#pragma once

#include <cstddef>

#include "geometry.h"

namespace malfoid {

// Madgwick gradient-descent AHRS, 6-axis (gyro + accelerometer) variant.
class Madgwick {
 public:
  explicit Madgwick(float beta = 0.08f) : beta_(beta) {}

  void reset() { q_ = Quat{}; }

  // Levels the filter from a resting accelerometer reading (heading = 0).
  void initFromAccel(float ax, float ay, float az);

  // gx..gz in rad/s, ax..az in any unit (only direction is used), dt in seconds.
  // The accelerometer correction is skipped while |a| is far from 1 g, so the
  // violent accelerations of a flick don't drag the horizon around.
  void update(float gx, float gy, float gz, float ax, float ay, float az, float dt);

  const Quat& orientation() const { return q_; }
  void setBeta(float beta) { beta_ = beta; }
  void setAccelGate(float lowG, float highG) {
    gateLow_ = lowG;
    gateHigh_ = highG;
  }

 private:
  Quat q_;
  float beta_;
  float gateLow_ = 0.85f;
  float gateHigh_ = 1.15f;
};

// Unit vector of the body +X axis (the tip) expressed in the world frame.
Vec3 tipDirection(const Quat& q);

struct Sky {
  float azimuth;    // radians, counter-clockwise seen from above
  float elevation;  // radians, + is up
};

Sky toSky(const Vec3& dir);

// Records the path traced by the tip as a 2-D stroke, in radians of sweep,
// relative to where the cast started. x grows to the caster's right, y upward.
class StrokeRecorder {
 public:
  static constexpr size_t kMaxPoints = 512;

  void begin(const Quat& q);
  bool add(const Quat& q);  // returns false once the buffer is full
  void clear() {
    n_ = 0;
    active_ = false;
  }

  bool active() const { return active_; }
  size_t size() const { return n_; }
  const Vec2* points() const { return pts_; }
  float sweep() const;  // total path length, radians

 private:
  Vec2 project(const Quat& q) const;

  Vec2 pts_[kMaxPoints];
  size_t n_ = 0;
  float az0_ = 0.0f;
  float el0_ = 0.0f;
  bool active_ = false;
};

// Learns the gyro zero-rate offset whenever the wand rests still for a second.
class GyroBias {
 public:
  void update(const Vec3& gyro, const Vec3& accel, float dt);
  Vec3 correct(const Vec3& gyro) const { return {gyro.x - bias_.x, gyro.y - bias_.y, gyro.z - bias_.z}; }
  const Vec3& bias() const { return bias_; }
  void set(const Vec3& bias) {
    bias_ = bias;
    settled_ = true;
  }
  bool settled() const { return settled_; }

 private:
  void restart() {
    sum_ = Vec3{};
    samples_ = 0;
    still_ = 0.0f;
  }

  Vec3 bias_;
  Vec3 sum_;
  int samples_ = 0;
  float still_ = 0.0f;
  bool settled_ = false;
};

}  // namespace malfoid
