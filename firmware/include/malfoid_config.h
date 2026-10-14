// Compile-time tunables. Runtime settings (Wi-Fi, MQTT, ...) live in settings.h
// and are edited from the web console.
#pragma once

#include <cstdint>

#ifndef MALFOID_FW_VERSION
#define MALFOID_FW_VERSION "dev"
#endif

namespace cfg {

// ---- IMU mounting ---------------------------------------------------------
// Wand frame: +X runs to the tip, +Z points up out of the thumb pad when held.
// Entry i is the chip axis that feeds wand axis i (0 = X, 1 = Y, 2 = Z);
// kImuSign flips it. The default suits a breakout lying flat, arrow to the tip.
constexpr uint8_t kImuAxis[3] = {0, 1, 2};
constexpr int8_t kImuSign[3] = {+1, +1, +1};
constexpr uint8_t kImuAddress = 0x6A;  // 0x6B when SDO/SA0 is pulled high

// ---- Casting --------------------------------------------------------------
constexpr uint8_t kStrokeDecimation = 2;  // 208 Hz IMU -> 104 Hz stroke points
constexpr uint32_t kMinCastMs = 120;
constexpr uint32_t kMaxCastMs = 3000;
constexpr float kMinSweepRad = 0.20f;  // ~11 degrees of tip travel, or it's a twitch

// Motion trigger, for builds without a thumb pad.
constexpr float kMotionStartRad = 2.0f;  // rad/s
constexpr float kMotionStopRad = 0.7f;   // rad/s
constexpr uint32_t kMotionStopMs = 150;

// ---- Thumb pad (capacitive) -----------------------------------------------
// ESP32-S3 touch readings *rise* when touched. Ratios are relative to baseline.
constexpr float kTouchOnRatio = 0.030f;
constexpr float kTouchOffRatio = 0.015f;

// ---- Power ----------------------------------------------------------------
constexpr uint16_t kDefaultSleepAfterS = 120;
constexpr float kActivityRad = 0.35f;  // any rotation faster than this counts as "in use"
constexpr uint32_t kConsoleKeepAwakeMs = 5UL * 60UL * 1000UL;
constexpr float kCriticalBatteryPct = 3.0f;
constexpr uint32_t kBatteryPollMs = 10000;

// ---- Network --------------------------------------------------------------
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kMqttRetryMs = 5000;
constexpr uint32_t kStatePublishMs = 60000;
constexpr uint32_t kPendingCastTtlMs = 15000;  // casts made before Wi-Fi is up are sent late, not lost

}  // namespace cfg
