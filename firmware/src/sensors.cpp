#include "sensors.h"

#include "board_pins.h"
#include "malfoid_config.h"

using namespace malfoid;

namespace sensors {

namespace {

Lsm6dsox* gImu = nullptr;
Grip* gGrip = nullptr;
TaskHandle_t gTask = nullptr;
QueueHandle_t gCasts = nullptr;
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
Live gLive;
volatile Trigger gTrigger = Trigger::Grip;
volatile bool gStop = false;
StrokeRecorder gRecorder;  // 4 KB - kept off the task stack

void IRAM_ATTR onDataReady() {
  BaseType_t woken = pdFALSE;
  if (gTask != nullptr) vTaskNotifyGiveFromISR(gTask, &woken);
  if (woken == pdTRUE) portYIELD_FROM_ISR();
}

Vec3 remap(float x, float y, float z) {
  const float in[3] = {x, y, z};
  return {in[cfg::kImuAxis[0]] * cfg::kImuSign[0], in[cfg::kImuAxis[1]] * cfg::kImuSign[1],
          in[cfg::kImuAxis[2]] * cfg::kImuSign[2]};
}

void queueCast(CastStatus status, uint32_t durationMs) {
  Cast c{};
  c.status = status;
  c.durationMs = static_cast<uint16_t>(durationMs > 65535 ? 65535 : durationMs);
  c.rawPoints = static_cast<uint16_t>(gRecorder.size());
  c.sweepRad = gRecorder.sweep();
  if (c.status == CastStatus::Ok) {
    if (durationMs < cfg::kMinCastMs) c.status = CastStatus::TooShort;
    else if (c.sweepRad < cfg::kMinSweepRad || !prepare(gRecorder.points(), gRecorder.size(), c.stroke))
      c.status = CastStatus::TooSmall;
  }
  gRecorder.clear();
  xQueueSend(gCasts, &c, 0);
}

void task(void*) {
  Madgwick ahrs(0.08f);
  GyroBias bias;
  ImuSample s{};
  if (gImu->read(s)) {
    const Vec3 a = remap(s.ax, s.ay, s.az);
    ahrs.initFromAccel(a.x, a.y, a.z);
  }

  uint32_t lastUs = micros();
  uint32_t n = 0;
  bool casting = false;
  bool overran = false;  // cast exceeded kMaxCastMs; wait for release
  uint32_t castStart = 0;
  uint32_t quietSince = 0;
  uint32_t lastActivity = millis();

  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(12));  // data-ready pulse, or poll if one was missed
    if (gStop) {
      vTaskSuspend(nullptr);
      continue;
    }
    if (!gImu->read(s)) continue;

    const uint32_t nowUs = micros();
    float dt = (nowUs - lastUs) * 1e-6f;
    lastUs = nowUs;
    if (dt <= 0.0f || dt > 0.05f) dt = 1.0f / 208.0f;

    const Vec3 a = remap(s.ax, s.ay, s.az);
    const Vec3 rawG = remap(s.gx, s.gy, s.gz);
    bias.update(rawG, a, dt);
    const Vec3 g = bias.correct(rawG);
    ahrs.update(g.x, g.y, g.z, a.x, a.y, a.z, dt);
    const Quat q = ahrs.orientation();
    const float rate = norm(g);
    const uint32_t nowMs = millis();
    ++n;

    bool gate = false;
    if (gTrigger == Trigger::Grip) {
      if (n % 4 == 0) gGrip->update();  // ~52 Hz is plenty for a thumb
      gate = gGrip->held();
    } else if (!casting) {
      gate = rate > cfg::kMotionStartRad;
    } else {
      if (rate > cfg::kMotionStopRad) quietSince = nowMs;
      gate = nowMs - quietSince < cfg::kMotionStopMs;
    }

    if (!casting && gate) {
      gRecorder.begin(q);
      casting = true;
      overran = false;
      castStart = quietSince = nowMs;
    } else if (casting && gate) {
      if (!overran && n % cfg::kStrokeDecimation == 0) gRecorder.add(q);
      if (!overran && nowMs - castStart > cfg::kMaxCastMs) {
        overran = true;
        queueCast(CastStatus::TooLong, nowMs - castStart);
      }
    } else if (casting && !gate) {
      if (!overran) queueCast(CastStatus::Ok, nowMs - castStart);
      casting = false;
    }

    if (rate > cfg::kActivityRad || gate) lastActivity = nowMs;
    const Sky sky = toSky(tipDirection(q));
    portENTER_CRITICAL(&gMux);
    gLive.q = q;
    gLive.azimuthDeg = sky.azimuth * kDegPerRad;
    gLive.elevationDeg = sky.elevation * kDegPerRad;
    gLive.casting = casting && !overran;
    gLive.gripHeld = gGrip->held();
    gLive.gripLevel = gGrip->level();
    gLive.biasSettled = bias.settled();
    gLive.lastActivityMs = lastActivity;
    gLive.samples = n;
    portEXIT_CRITICAL(&gMux);
  }
}

}  // namespace

bool begin(Lsm6dsox& imu, Grip& grip) {
  gImu = &imu;
  gGrip = &grip;
  gCasts = xQueueCreate(4, sizeof(Cast));
  gLive.lastActivityMs = millis();
  xTaskCreatePinnedToCore(task, "sensors", 6144, nullptr, 5, &gTask, 1);
  pinMode(pins::kImuInt1, INPUT);
  attachInterrupt(digitalPinToInterrupt(pins::kImuInt1), onDataReady, RISING);
  return gTask != nullptr && gCasts != nullptr;
}

void setTrigger(Trigger t) { gTrigger = t; }

Live live() {
  portENTER_CRITICAL(&gMux);
  const Live copy = gLive;
  portEXIT_CRITICAL(&gMux);
  return copy;
}

bool nextCast(Cast& out) { return gCasts != nullptr && xQueueReceive(gCasts, &out, 0) == pdTRUE; }

void stop() {
  detachInterrupt(digitalPinToInterrupt(pins::kImuInt1));
  gStop = true;
  delay(30);  // the task parks itself at the top of its loop, never mid-transaction
}

}  // namespace sensors
