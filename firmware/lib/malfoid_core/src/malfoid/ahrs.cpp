#include "ahrs.h"

#include <cmath>

namespace malfoid {

namespace {

constexpr float kStillGyro = 0.08f;   // rad/s (~4.6 deg/s) around the current bias
constexpr float kStillAccel = 0.06f;  // g away from 1 g
constexpr float kStillTime = 1.0f;    // seconds of stillness before learning

inline float invSqrt(float v) { return 1.0f / std::sqrt(v); }

}  // namespace

void Madgwick::initFromAccel(float ax, float ay, float az) {
  const float roll = std::atan2(ay, az);
  const float pitch = std::atan2(-ax, std::sqrt(ay * ay + az * az));
  const float cr = std::cos(roll * 0.5f), sr = std::sin(roll * 0.5f);
  const float cp = std::cos(pitch * 0.5f), sp = std::sin(pitch * 0.5f);
  q_ = {cr * cp, sr * cp, cr * sp, -sr * sp};
}

void Madgwick::update(float gx, float gy, float gz, float ax, float ay, float az, float dt) {
  float q0 = q_.w, q1 = q_.x, q2 = q_.y, q3 = q_.z;

  // Rate of change of the quaternion from the gyroscope.
  float qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
  float qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
  float qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
  float qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

  const float aNorm = std::sqrt(ax * ax + ay * ay + az * az);
  if (aNorm > 0.0f && aNorm >= gateLow_ && aNorm <= gateHigh_) {
    ax /= aNorm;
    ay /= aNorm;
    az /= aNorm;

    const float _2q0 = 2.0f * q0, _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3;
    const float _4q0 = 4.0f * q0, _4q1 = 4.0f * q1, _4q2 = 4.0f * q2;
    const float _8q1 = 8.0f * q1, _8q2 = 8.0f * q2;
    const float q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;

    // Gradient of the error between measured and predicted gravity.
    float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
    float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
    float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
    float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
    const float sNorm = s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
    if (sNorm > 0.0f) {
      const float r = invSqrt(sNorm);
      qDot1 -= beta_ * s0 * r;
      qDot2 -= beta_ * s1 * r;
      qDot3 -= beta_ * s2 * r;
      qDot4 -= beta_ * s3 * r;
    }
  }

  q0 += qDot1 * dt;
  q1 += qDot2 * dt;
  q2 += qDot3 * dt;
  q3 += qDot4 * dt;
  const float r = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q_ = {q0 * r, q1 * r, q2 * r, q3 * r};
}

Vec3 tipDirection(const Quat& q) {
  // First column of the body-to-world rotation matrix.
  return {1.0f - 2.0f * (q.y * q.y + q.z * q.z), 2.0f * (q.x * q.y + q.w * q.z), 2.0f * (q.x * q.z - q.w * q.y)};
}

Sky toSky(const Vec3& dir) {
  const float z = dir.z > 1.0f ? 1.0f : (dir.z < -1.0f ? -1.0f : dir.z);
  return {std::atan2(dir.y, dir.x), std::asin(z)};
}

Vec2 StrokeRecorder::project(const Quat& q) const {
  const Sky s = toSky(tipDirection(q));
  // Turning left increases azimuth, so the caster's "right" is -azimuth.
  // cos(elevation) keeps horizontal sweeps honest when pointing up or down.
  return {-wrapPi(s.azimuth - az0_) * std::cos(s.elevation), s.elevation - el0_};
}

void StrokeRecorder::begin(const Quat& q) {
  const Sky s = toSky(tipDirection(q));
  az0_ = s.azimuth;
  el0_ = s.elevation;
  n_ = 0;
  active_ = true;
  pts_[n_++] = {0.0f, 0.0f};
}

bool StrokeRecorder::add(const Quat& q) {
  if (!active_ || n_ >= kMaxPoints) return false;
  pts_[n_++] = project(q);
  return true;
}

float StrokeRecorder::sweep() const {
  float total = 0.0f;
  for (size_t i = 1; i < n_; ++i) total += distance(pts_[i - 1], pts_[i]);
  return total;
}

void GyroBias::update(const Vec3& gyro, const Vec3& accel, float dt) {
  const Vec3 rate = correct(gyro);
  const bool still = norm(rate) < kStillGyro && std::fabs(norm(accel) - 1.0f) < kStillAccel;
  if (!still) {
    restart();
    return;
  }
  sum_ = {sum_.x + gyro.x, sum_.y + gyro.y, sum_.z + gyro.z};
  ++samples_;
  still_ += dt;
  if (still_ < kStillTime) return;

  const float k = 1.0f / static_cast<float>(samples_);
  const Vec3 mean = {sum_.x * k, sum_.y * k, sum_.z * k};
  if (!settled_) {
    bias_ = mean;
    settled_ = true;
  } else {  // drift slowly; one still second shouldn't erase a good estimate
    bias_ = {0.8f * bias_.x + 0.2f * mean.x, 0.8f * bias_.y + 0.2f * mean.y, 0.8f * bias_.z + 0.2f * mean.z};
  }
  restart();
}

}  // namespace malfoid
