// The sensing pipeline, running in its own FreeRTOS task on core 1:
//
//   IMU data-ready pulse (208 Hz) -> read -> axis remap -> gyro bias -> Madgwick AHRS
//     -> tip direction -> cast gate (thumb pad or motion) -> stroke recorder
//     -> finished casts are queued for the main loop to recognize.
#pragma once

#include <Arduino.h>
#include <malfoid_core.h>

#include "grip.h"
#include "imu_lsm6dsox.h"

namespace sensors {

enum class Trigger : uint8_t { Grip = 0, Motion = 1 };

enum class CastStatus : uint8_t { Ok, TooShort, TooSmall, TooLong };

struct Cast {
  CastStatus status;
  malfoid::Stroke stroke;  // normalized; only meaningful when status == Ok
  uint16_t rawPoints;
  uint16_t durationMs;
  float sweepRad;
};

struct Live {
  malfoid::Quat q;
  float azimuthDeg = 0.0f;
  float elevationDeg = 0.0f;
  bool casting = false;
  bool gripHeld = false;
  float gripLevel = 0.0f;
  bool biasSettled = false;
  uint32_t lastActivityMs = 0;
  uint32_t samples = 0;
};

bool begin(Lsm6dsox& imu, Grip& grip);
void setTrigger(Trigger t);
Live live();
bool nextCast(Cast& out);
void stop();  // detach the interrupt and park the task (before deep sleep)

}  // namespace sensors
