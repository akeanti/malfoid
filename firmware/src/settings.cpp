#include "settings.h"

#include <Preferences.h>

namespace store {

namespace {

void getStr(Preferences& p, const char* key, char* dst, size_t cap) {
  if (p.isKey(key)) p.getString(key, dst, cap);
}

}  // namespace

void load(Settings& s) {
  Preferences p;
  if (!p.begin("malfoid", true)) return;  // namespace doesn't exist yet: keep defaults
  getStr(p, "name", s.name, sizeof(s.name));
  getStr(p, "ssid", s.wifiSsid, sizeof(s.wifiSsid));
  getStr(p, "wpass", s.wifiPass, sizeof(s.wifiPass));
  getStr(p, "mhost", s.mqttHost, sizeof(s.mqttHost));
  getStr(p, "muser", s.mqttUser, sizeof(s.mqttUser));
  getStr(p, "mpass", s.mqttPass, sizeof(s.mqttPass));
  getStr(p, "mbase", s.mqttBase, sizeof(s.mqttBase));
  s.mqttPort = p.getUShort("mport", s.mqttPort);
  s.trigger = static_cast<sensors::Trigger>(p.getUChar("trig", static_cast<uint8_t>(s.trigger)));
  s.acceptScore = p.getFloat("accept", s.acceptScore);
  s.sleepAfterS = p.getUShort("sleep", s.sleepAfterS);
  p.end();
}

void save(const Settings& s) {
  Preferences p;
  if (!p.begin("malfoid", false)) return;
  p.putString("name", s.name);
  p.putString("ssid", s.wifiSsid);
  p.putString("wpass", s.wifiPass);
  p.putString("mhost", s.mqttHost);
  p.putString("muser", s.mqttUser);
  p.putString("mpass", s.mqttPass);
  p.putString("mbase", s.mqttBase);
  p.putUShort("mport", s.mqttPort);
  p.putUChar("trig", static_cast<uint8_t>(s.trigger));
  p.putFloat("accept", s.acceptScore);
  p.putUShort("sleep", s.sleepAfterS);
  p.end();
}

size_t loadSpells(CustomSpell* out, size_t cap) {
  Preferences p;
  if (!p.begin("spells", true)) return 0;
  const size_t stored = p.getUChar("n", 0);
  size_t loaded = 0;
  char key[8];
  for (size_t i = 0; i < stored && loaded < cap; ++i) {
    snprintf(key, sizeof(key), "s%u", static_cast<unsigned>(i));
    if (p.getBytes(key, &out[loaded], sizeof(CustomSpell)) == sizeof(CustomSpell)) {
      out[loaded].id[sizeof(out[loaded].id) - 1] = '\0';
      out[loaded].name[sizeof(out[loaded].name) - 1] = '\0';
      ++loaded;
    }
  }
  p.end();
  return loaded;
}

bool saveSpells(const CustomSpell* spells, size_t count) {
  Preferences p;
  if (!p.begin("spells", false)) return false;
  p.clear();
  char key[8];
  bool ok = true;
  for (size_t i = 0; i < count; ++i) {
    snprintf(key, sizeof(key), "s%u", static_cast<unsigned>(i));
    ok &= p.putBytes(key, &spells[i], sizeof(CustomSpell)) == sizeof(CustomSpell);
  }
  p.putUChar("n", static_cast<uint8_t>(count));
  p.end();
  return ok;
}

}  // namespace store
