// Malfoid - an open-source gesture wand with a platinum attitude.
//
// Main loop (core 1, low priority): recognize finished casts, run spell
// effects, talk to the web console and MQTT, watch the battery, and put the
// wand into deep sleep when it is set down. The time-critical sensing runs in
// its own task - see sensors.cpp.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <malfoid_core.h>

#include "board_pins.h"
#include "bus.h"
#include "console.h"
#include "fuel_gauge.h"
#include "grip.h"
#include "haptics.h"
#include "imu_lsm6dsox.h"
#include "malfoid_config.h"
#include "net_link.h"
#include "sensors.h"
#include "settings.h"
#include "tip_light.h"

using namespace malfoid;

namespace {

constexpr uint32_t kSilver = 0xC9D1D9;
constexpr uint32_t kEmerald = 0x2FD38A;

Lsm6dsox imu;
Max17048 gauge;
Haptics haptics;
TipLight light;
Grip grip;
NetLink net;
Settings settings;
Recognizer recognizer;
CustomSpell customSpells[store::kMaxCustomSpells];
size_t customCount = 0;
Battery battery;

char deviceId[8] = "";
char lastSpell[16] = "";
char recordId[16] = "";  // non-empty while the next cast should be recorded
char recordName[24] = "";
bool streaming = false;
bool wasCasting = false;
uint32_t lastBatteryPoll = 0;
uint32_t lastStatePublish = 0;
uint32_t lastStream = 0;
uint32_t lastStatus = 0;
uint32_t lastFaultBlink = 0;

// ---------------------------------------------------------------- helpers --

const spellbook::SpellInfo* builtinSpell(const char* id) {
  for (const auto& s : spellbook::kSpells)
    if (strcmp(s.id, id) == 0) return &s;
  return nullptr;
}

const char* displayName(const char* id) {
  if (const auto* s = builtinSpell(id)) return s->name;
  for (size_t i = 0; i < customCount; ++i)
    if (strcmp(customSpells[i].id, id) == 0) return customSpells[i].name;
  return id;
}

// "Wingardium Leviosa!" -> "wingardium_levi" (15 chars max, NVS-key friendly).
void slugify(const char* name, char* out, size_t cap) {
  size_t n = 0;
  bool gap = false;
  for (const char* p = name; *p && n + 1 < cap; ++p) {
    const char c = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    if (isalnum(static_cast<unsigned char>(c))) {
      if (gap && n > 0) out[n++] = '_';
      if (n + 1 < cap) out[n++] = c;
      gap = false;
    } else {
      gap = true;
    }
  }
  out[n] = '\0';
}

void copyStr(char* dst, size_t cap, const char* src) {
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

float round3(float v) { return roundf(v * 1000.0f) / 1000.0f; }

void rebuildRecognizer() {
  recognizer.clear();
  recognizer.loadBuiltins();
  for (size_t i = 0; i < customCount; ++i) {
    Stroke s;
    for (size_t k = 0; k < kStrokePoints; ++k) s.pts[k] = {customSpells[i].xy[2 * k], customSpells[i].xy[2 * k + 1]};
    recognizer.add(customSpells[i].id, s, true);
  }
}

void addPoints(JsonDocument& doc, const Stroke& s) {
  JsonArray pts = doc["pts"].to<JsonArray>();
  for (const Vec2& p : s.pts) {
    JsonArray xy = pts.add<JsonArray>();
    xy.add(round3(p.x));
    xy.add(round3(p.y));
  }
}

// ------------------------------------------------------------ spell effects --

void castEffects(const char* id, uint32_t now) {
  const spellbook::SpellInfo* info = builtinSpell(id);
  if (info == nullptr) {  // an owner-recorded spell
    light.flash(kSilver, 700, true, now);
    haptics.play(fx::kDoubleClick);
    return;
  }
  if (strcmp(id, "lumen") == 0) light.setTorch(true);
  if (strcmp(id, "umbra") == 0) light.setTorch(false);
  const bool signature = strcmp(id, "malfoid") == 0;
  light.flash(info->rgb, signature ? 1600 : 700, signature, now);
  haptics.play(info->haptics, sizeof(info->haptics));
}

void fizzle(uint32_t now) {
  light.fizzle(now);
  haptics.play(fx::kBuzz);
}

// --------------------------------------------------------------- console --

void sendHello() {
  JsonDocument doc;
  doc["evt"] = "hello";
  doc["fw"] = MALFOID_FW_VERSION;
  doc["board"] = MALFOID_BOARD_NAME;
  doc["id"] = deviceId;
  JsonObject hw = doc["hw"].to<JsonObject>();
  hw["imu"] = imu.ok();
  hw["haptics"] = haptics.ok();
  hw["gauge"] = gauge.ok();
  hw["touch_baseline"] = grip.baseline();

  JsonArray spells = doc["spells"].to<JsonArray>();
  for (const auto& s : spellbook::kSpells) {
    JsonObject o = spells.add<JsonObject>();
    o["id"] = s.id;
    o["name"] = s.name;
    o["custom"] = false;
  }
  for (size_t i = 0; i < customCount; ++i) {
    JsonObject o = spells.add<JsonObject>();
    o["id"] = customSpells[i].id;
    o["name"] = customSpells[i].name;
    o["custom"] = true;
  }

  JsonObject st = doc["settings"].to<JsonObject>();  // passwords are never echoed
  st["name"] = settings.name;
  st["wifi_ssid"] = settings.wifiSsid;
  st["wifi_pass_set"] = settings.wifiPass[0] != '\0';
  st["mqtt_host"] = settings.mqttHost;
  st["mqtt_port"] = settings.mqttPort;
  st["mqtt_user"] = settings.mqttUser;
  st["mqtt_pass_set"] = settings.mqttPass[0] != '\0';
  st["mqtt_base"] = settings.mqttBase;
  st["trigger"] = settings.trigger == sensors::Trigger::Motion ? "motion" : "grip";
  st["accept"] = round3(settings.acceptScore);
  st["sleep_after"] = settings.sleepAfterS;
  console::send(doc);
}

void sendStatus() {
  JsonDocument doc;
  doc["evt"] = "status";
  doc["wifi"] = net.wifiUp();
  doc["mqtt"] = net.mqttUp();
  doc["rssi"] = net.rssi();
  if (battery.present) {
    doc["bat"] = roundf(battery.percent * 10.0f) / 10.0f;
    doc["v"] = roundf(battery.volts * 100.0f) / 100.0f;
    doc["chg"] = battery.charging;
  }
  doc["uptime"] = millis() / 1000;
  doc["armed"] = recordId;
  console::send(doc);
}

void sendLive(const sensors::Live& live) {
  JsonDocument doc;
  doc["evt"] = "live";
  doc["az"] = roundf(live.azimuthDeg * 10.0f) / 10.0f;
  doc["el"] = roundf(live.elevationDeg * 10.0f) / 10.0f;
  JsonArray q = doc["q"].to<JsonArray>();
  q.add(round3(live.q.w));
  q.add(round3(live.q.x));
  q.add(round3(live.q.y));
  q.add(round3(live.q.z));
  doc["casting"] = live.casting;
  doc["held"] = live.gripHeld;
  doc["grip"] = round3(live.gripLevel);
  doc["bias_ok"] = live.biasSettled;
  console::send(doc);
}

template <size_t N>
bool takeStr(JsonDocument& msg, const char* key, char (&dst)[N]) {
  if (!msg[key].is<const char*>()) return false;
  copyStr(dst, N, msg[key].as<const char*>());
  return true;
}

void applySettings(JsonDocument& msg) {
  bool network = false;
  takeStr(msg, "name", settings.name);
  network |= takeStr(msg, "wifi_ssid", settings.wifiSsid);
  network |= takeStr(msg, "wifi_pass", settings.wifiPass);
  network |= takeStr(msg, "mqtt_host", settings.mqttHost);
  network |= takeStr(msg, "mqtt_user", settings.mqttUser);
  network |= takeStr(msg, "mqtt_pass", settings.mqttPass);
  network |= takeStr(msg, "mqtt_base", settings.mqttBase);
  if (msg["mqtt_port"].is<int>()) {
    const int port = msg["mqtt_port"];
    if (port < 1 || port > 65535) return console::error("set", "mqtt_port must be 1-65535");
    settings.mqttPort = static_cast<uint16_t>(port);
    network = true;
  }
  if (msg["trigger"].is<const char*>()) {
    const char* t = msg["trigger"];
    settings.trigger = strcmp(t, "motion") == 0 ? sensors::Trigger::Motion : sensors::Trigger::Grip;
    sensors::setTrigger(settings.trigger);
  }
  if (msg["accept"].is<float>()) settings.acceptScore = constrain(msg["accept"].as<float>(), 0.3f, 0.95f);
  if (msg["sleep_after"].is<int>()) settings.sleepAfterS = constrain(msg["sleep_after"].as<int>(), 30, 3600);

  store::save(settings);
  if (network) net.reconfigure();
  console::ok("set");
  sendHello();
}

void armRecording(const char* name) {
  char id[16];
  slugify(name, id, sizeof(id));
  if (id[0] == '\0') return console::error("record", "give the spell a name");
  if (customCount >= store::kMaxCustomSpells) return console::error("record", "spellbook full - forget a spell first");
  copyStr(recordId, sizeof(recordId), id);
  copyStr(recordName, sizeof(recordName), builtinSpell(id) ? builtinSpell(id)->name : name);
  JsonDocument doc;
  doc["evt"] = "armed";
  doc["id"] = recordId;
  doc["name"] = recordName;
  console::send(doc);
  light.flash(kSilver, 500, false, millis());
  haptics.play(fx::kDoubleClick);
}

void recordCast(const Stroke& stroke) {
  CustomSpell& cs = customSpells[customCount];
  copyStr(cs.id, sizeof(cs.id), recordId);
  copyStr(cs.name, sizeof(cs.name), recordName);
  for (size_t k = 0; k < kStrokePoints; ++k) {
    cs.xy[2 * k] = stroke.pts[k].x;
    cs.xy[2 * k + 1] = stroke.pts[k].y;
  }
  ++customCount;
  store::saveSpells(customSpells, customCount);
  rebuildRecognizer();
  net.announceSpell(cs.id);

  JsonDocument doc;
  doc["evt"] = "recorded";
  doc["id"] = cs.id;
  doc["name"] = cs.name;
  addPoints(doc, stroke);
  console::send(doc);
  recordId[0] = '\0';
  light.flash(kSilver, 900, true, millis());
  haptics.play(fx::kTripleClick);
}

void forget(const char* id) {
  size_t kept = 0;
  for (size_t i = 0; i < customCount; ++i)
    if (strcmp(customSpells[i].id, id) != 0) customSpells[kept++] = customSpells[i];
  if (kept == customCount) return console::error("forget", "no recorded spell with that id");
  customCount = kept;
  store::saveSpells(customSpells, customCount);
  rebuildRecognizer();
  console::ok("forget");
  sendHello();
}

[[noreturn]] void goToSleep(const char* why);

void onCommand(JsonDocument& msg) {
  const char* cmd = msg["cmd"];
  if (strcmp(cmd, "hello") == 0) {
    sendHello();
    sendStatus();
  } else if (strcmp(cmd, "stream") == 0) {
    streaming = msg["on"] | true;
    console::ok("stream");
  } else if (strcmp(cmd, "set") == 0) {
    applySettings(msg);
  } else if (strcmp(cmd, "record") == 0) {
    armRecording(msg["name"] | "");
  } else if (strcmp(cmd, "cancel") == 0) {
    recordId[0] = '\0';
    console::ok("cancel");
  } else if (strcmp(cmd, "forget") == 0) {
    forget(msg["id"] | "");
  } else if (strcmp(cmd, "test") == 0) {
    castEffects(msg["id"] | "malfoid", millis());
    console::ok("test");
  } else if (strcmp(cmd, "status") == 0) {
    sendStatus();
  } else if (strcmp(cmd, "sleep") == 0) {
    goToSleep("console");
  } else if (strcmp(cmd, "reboot") == 0) {
    console::ok("reboot");
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    console::error(cmd, "unknown command");
  }
}

// ----------------------------------------------------------------- casting --

void handleCast(const sensors::Cast& c) {
  const uint32_t now = millis();
  JsonDocument doc;
  doc["evt"] = "cast";
  doc["ms"] = c.durationMs;
  doc["sweep"] = roundf(c.sweepRad * kDegPerRad * 10.0f) / 10.0f;

  if (c.status != sensors::CastStatus::Ok) {
    doc["ok"] = false;
    doc["reason"] = c.status == sensors::CastStatus::TooShort  ? "too_short"
                    : c.status == sensors::CastStatus::TooLong ? "too_long"
                                                               : "too_small";
    console::send(doc);
    fizzle(now);
    return;
  }

  if (recordId[0] != '\0') return recordCast(c.stroke);

  const Match m = recognizer.classify(c.stroke);
  const bool ok = Recognizer::accepts(m, settings.acceptScore);
  const char* id = m.index >= 0 ? recognizer.at(m.index).id : "";
  doc["ok"] = ok;
  doc["id"] = id;
  doc["name"] = displayName(id);
  doc["score"] = round3(m.score);
  doc["dist"] = round3(m.distance);
  if (m.runnerUp >= 0) {
    doc["runner"] = recognizer.at(m.runnerUp).id;
    doc["runner_score"] = round3(m.runnerUpScore);
  }
  addPoints(doc, c.stroke);
  console::send(doc);

  if (!ok) return fizzle(now);
  castEffects(id, now);
  copyStr(lastSpell, sizeof(lastSpell), id);
  net.publishSpell(id, m.score, now);
  net.publishState(battery, lastSpell);
}

void onMqttConnected() {
  net.announceSensors();
  for (const auto& s : spellbook::kSpells) net.announceSpell(s.id);
  for (size_t i = 0; i < customCount; ++i) {
    bool dup = builtinSpell(customSpells[i].id) != nullptr;
    for (size_t j = 0; j < i && !dup; ++j) dup = strcmp(customSpells[j].id, customSpells[i].id) == 0;
    if (!dup) net.announceSpell(customSpells[i].id);
  }
  net.publishState(battery, lastSpell);
  lastStatePublish = millis();
}

// ------------------------------------------------------------------- power --

[[noreturn]] void goToSleep(const char* why) {
  JsonDocument doc;
  doc["evt"] = "sleep";
  doc["why"] = why;
  console::send(doc);
  Serial.flush();

  // A short emerald fade so she visibly "closes her eyes".
  light.setTorch(false);
  light.setCasting(false, millis());
  light.flash(kEmerald, 450, false, millis());
  const uint32_t until = millis() + 470;
  while (static_cast<int32_t>(millis() - until) < 0) {
    light.update(millis());
    delay(10);
  }

  if (imu.ok()) sensors::stop();
  net.shutdown();
  haptics.standby();
  light.powerDown();

  // ext1 (unlike ext0) lets the RTC peripheral domain power down, saving a few
  // uA. The IMU drives INT1 push-pull, so no RTC pull resistors are needed.
  if (imu.ok() && imu.configureWakeOnMotion()) {
    esp_sleep_enable_ext1_wakeup(1ULL << pins::kImuInt1, ESP_EXT1_WAKEUP_ANY_HIGH);
  }
  esp_deep_sleep_start();
}

void pollBattery(uint32_t now) {
  lastBatteryPoll = now;
  if (!gauge.read(battery)) return;
  if (battery.percent < cfg::kCriticalBatteryPct && !battery.charging) {
    light.warn(now);
    const uint32_t until = now + 1300;
    while (static_cast<int32_t>(millis() - until) < 0) {
      light.update(millis());
      delay(10);
    }
    goToSleep("battery");
  }
}

void makeDeviceId() {
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceId, sizeof(deviceId), "%06lx", static_cast<unsigned long>((mac >> 24) & 0xFFFFFF));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const bool wokeByMotion = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  if (wokeByMotion) rtc_gpio_deinit(static_cast<gpio_num_t>(pins::kImuInt1));  // hand the pin back to GPIO

  bus::begin(pins::kI2cSda, pins::kI2cScl, 400000);
  light.begin(pins::kTipLed, pins::kLedPower);
  makeDeviceId();
  store::load(settings);

  haptics.begin(pins::kHapticEnable);
  gauge.begin();
  grip.begin(pins::kGripTouch, pins::kCastButton);

  customCount = store::loadSpells(customSpells, store::kMaxCustomSpells);
  rebuildRecognizer();

  if (imu.begin(cfg::kImuAddress)) {
    sensors::begin(imu, grip);
    sensors::setTrigger(settings.trigger);
  }

  console::begin(onCommand);
  net.begin(settings, deviceId, onMqttConnected);
  pollBattery(millis());

  const uint32_t now = millis();
  light.flash(wokeByMotion ? kEmerald : kSilver, wokeByMotion ? 500 : 1400, !wokeByMotion, now);
  haptics.play(wokeByMotion ? fx::kSharpTick : fx::kRampUp);
}

void loop() {
  console::poll();
  net.loop(millis());

  sensors::Cast cast;
  while (imu.ok() && sensors::nextCast(cast)) handleCast(cast);

  const sensors::Live live = imu.ok() ? sensors::live() : sensors::Live{};
  const uint32_t now = millis();

  if (live.casting != wasCasting) {
    wasCasting = live.casting;
    light.setCasting(live.casting, now);
    if (live.casting) haptics.play(fx::kSharpTick);
  }
  if (!imu.ok() && now - lastFaultBlink > 2000) {  // no IMU: blink red, keep the console alive
    lastFaultBlink = now;
    light.fizzle(now);
  }
  light.update(now);

  if (console::active(now)) {
    if (streaming && now - lastStream >= 33) {
      lastStream = now;
      sendLive(live);
    }
    if (now - lastStatus >= 2000) {
      lastStatus = now;
      sendStatus();
    }
  } else {
    streaming = false;
  }

  if (now - lastBatteryPoll >= cfg::kBatteryPollMs) pollBattery(now);
  if (net.mqttUp() && now - lastStatePublish >= cfg::kStatePublishMs) {
    lastStatePublish = now;
    net.publishState(battery, lastSpell);
  }

  const int32_t idleMs = static_cast<int32_t>(now - live.lastActivityMs);
  if (imu.ok() && !live.casting && recordId[0] == '\0' && !console::active(now) &&
      idleMs > static_cast<int32_t>(settings.sleepAfterS) * 1000) {
    goToSleep("idle");
  }
  delay(5);
}
