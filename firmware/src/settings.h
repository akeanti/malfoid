// Runtime settings and owner-recorded spells, persisted in NVS flash.
#pragma once

#include <Arduino.h>
#include <malfoid_core.h>

#include "malfoid_config.h"
#include "sensors.h"

struct Settings {
  char name[24] = "Malfoid";
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[33] = "";
  char mqttPass[65] = "";
  char mqttBase[24] = "malfoid";
  sensors::Trigger trigger = sensors::Trigger::Grip;
  float acceptScore = malfoid::Recognizer::kAcceptScore;
  uint16_t sleepAfterS = cfg::kDefaultSleepAfterS;
};

struct CustomSpell {
  char id[16];
  char name[24];
  float xy[2 * malfoid::kStrokePoints];  // normalized stroke, interleaved x, y
};

namespace store {

constexpr size_t kMaxCustomSpells = 16;

void load(Settings& s);
void save(const Settings& s);
size_t loadSpells(CustomSpell* out, size_t cap);
bool saveSpells(const CustomSpell* spells, size_t count);

}  // namespace store
