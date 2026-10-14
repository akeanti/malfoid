#include "net_link.h"

#include <ArduinoJson.h>
#include <malfoid_core.h>

#include "board_pins.h"
#include "malfoid_config.h"

namespace {

void addDevice(JsonDocument& doc, const Settings& s, const char* deviceId) {
  JsonObject dev = doc["device"].to<JsonObject>();
  char ident[24];
  snprintf(ident, sizeof(ident), "malfoid_%s", deviceId);
  dev["identifiers"].to<JsonArray>().add(ident);
  dev["name"] = s.name;
  dev["manufacturer"] = "Malfoid";
  dev["model"] = "Malfoid wand (" MALFOID_BOARD_NAME ")";
  dev["sw_version"] = MALFOID_FW_VERSION;
}

}  // namespace

void NetLink::begin(const Settings& s, const char* deviceId, ConnectHook onMqttConnect) {
  s_ = &s;
  strncpy(deviceId_, deviceId, sizeof(deviceId_) - 1);
  onConnect_ = onMqttConnect;
  mqtt_.setBufferSize(1024);  // discovery payloads are larger than PubSubClient's 256 B default
  mqtt_.setKeepAlive(30);
  WiFi.persistent(false);     // credentials live in our own NVS namespace
}

void NetLink::reconfigure() {
  if (mqtt_.connected()) mqtt_.disconnect();
  WiFi.disconnect(true);
  wifiStarted_ = false;
  mqttTried_ = false;
}

void NetLink::topic(char* out, size_t cap, const char* leaf) const {
  snprintf(out, cap, "%s/%s/%s", s_->mqttBase, deviceId_, leaf);
}

void NetLink::loop(uint32_t now) {
  if (s_ == nullptr || s_->wifiSsid[0] == '\0') return;  // not configured yet

  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiStarted_ || now - lastWifiTry_ > cfg::kWifiRetryMs) {
      char host[24];
      snprintf(host, sizeof(host), "malfoid-%s", deviceId_);
      WiFi.setHostname(host);
      WiFi.mode(WIFI_STA);
      WiFi.begin(s_->wifiSsid, s_->wifiPass);
      wifiStarted_ = true;
      lastWifiTry_ = now;
    }
    return;
  }

  if (s_->mqttHost[0] == '\0') return;
  if (!mqtt_.connected()) {
    if (mqttTried_ && now - lastMqttTry_ < cfg::kMqttRetryMs) return;
    mqttTried_ = true;
    lastMqttTry_ = now;
    if (!connectMqtt()) return;
  }
  mqtt_.loop();
  flushPending(now);
}

bool NetLink::connectMqtt() {
  mqtt_.setServer(s_->mqttHost, s_->mqttPort);
  char clientId[24], status[64];
  snprintf(clientId, sizeof(clientId), "malfoid-%s", deviceId_);
  topic(status, sizeof(status), "status");
  const char* user = s_->mqttUser[0] ? s_->mqttUser : nullptr;
  const char* pass = s_->mqttPass[0] ? s_->mqttPass : nullptr;
  if (!mqtt_.connect(clientId, user, pass, status, 1, true, "offline")) return false;
  mqtt_.publish(status, "online", true);
  if (onConnect_ != nullptr) onConnect_();
  return true;
}

void NetLink::publishSpell(const char* id, float score, uint32_t now) {
  if (mqtt_.connected()) {
    char t[64];
    topic(t, sizeof(t), "spell");
    mqtt_.publish(t, id, false);
    return;
  }
  // Not connected yet (e.g. just woke up): hold the cast briefly instead of losing it.
  Pending* slot = &pending_[0];
  for (Pending& p : pending_) {
    if (!p.used) {
      slot = &p;
      break;
    }
    if (p.at < slot->at) slot = &p;  // all full: overwrite the oldest
  }
  strncpy(slot->id, id, sizeof(slot->id) - 1);
  slot->id[sizeof(slot->id) - 1] = '\0';
  slot->score = score;
  slot->at = now;
  slot->used = true;
}

void NetLink::flushPending(uint32_t now) {
  for (Pending& p : pending_) {
    if (!p.used) continue;
    p.used = false;
    if (now - p.at <= cfg::kPendingCastTtlMs) publishSpell(p.id, p.score, now);
  }
}

bool NetLink::publishJson(const char* t, const char* json, bool retained) {
  return mqtt_.connected() && mqtt_.publish(t, json, retained);
}

void NetLink::publishState(const Battery& b, const char* lastSpell) {
  if (!mqtt_.connected()) return;
  JsonDocument doc;
  if (b.present) {
    doc["battery"] = static_cast<int>(b.percent + 0.5f);
    doc["voltage"] = roundf(b.volts * 100.0f) / 100.0f;
    doc["charging"] = b.charging;
  }
  doc["last_spell"] = lastSpell;
  doc["rssi"] = rssi();
  char json[192], t[64];
  serializeJson(doc, json, sizeof(json));
  topic(t, sizeof(t), "state");
  publishJson(t, json, true);
}

void NetLink::announceSpell(const char* id) {
  if (!mqtt_.connected()) return;
  JsonDocument doc;
  char spellTopic[64];
  topic(spellTopic, sizeof(spellTopic), "spell");
  doc["automation_type"] = "trigger";
  doc["topic"] = spellTopic;
  doc["type"] = "spell_cast";
  doc["subtype"] = id;
  doc["payload"] = id;
  addDevice(doc, *s_, deviceId_);
  char json[512], t[96];
  serializeJson(doc, json, sizeof(json));
  snprintf(t, sizeof(t), "homeassistant/device_automation/malfoid_%s/%s/config", deviceId_, id);
  publishJson(t, json, true);
}

void NetLink::announceSensors() {
  if (!mqtt_.connected()) return;
  char stateTopic[64];
  topic(stateTopic, sizeof(stateTopic), "state");
  struct Sensor {
    const char* key;
    const char* name;
    const char* tmpl;
    const char* unit;
    const char* deviceClass;
    const char* icon;
  };
  const Sensor sensors[] = {
      {"battery", "Battery", "{{ value_json.battery }}", "%", "battery", nullptr},
      {"last_spell", "Last spell", "{{ value_json.last_spell }}", nullptr, nullptr, "mdi:magic-staff"},
  };
  for (const Sensor& s : sensors) {
    JsonDocument doc;
    char uid[40];
    snprintf(uid, sizeof(uid), "malfoid_%s_%s", deviceId_, s.key);
    doc["name"] = s.name;
    doc["unique_id"] = uid;
    doc["state_topic"] = stateTopic;
    doc["value_template"] = s.tmpl;
    if (s.unit) doc["unit_of_measurement"] = s.unit;
    if (s.deviceClass) {
      doc["device_class"] = s.deviceClass;
      doc["state_class"] = "measurement";
    }
    if (s.icon) doc["icon"] = s.icon;
    addDevice(doc, *s_, deviceId_);
    char json[640], t[96];
    serializeJson(doc, json, sizeof(json));
    snprintf(t, sizeof(t), "homeassistant/sensor/malfoid_%s/%s/config", deviceId_, s.key);
    publishJson(t, json, true);
  }
}

void NetLink::shutdown() {
  if (mqtt_.connected()) {
    char status[64];
    topic(status, sizeof(status), "status");
    mqtt_.publish(status, "sleeping", true);
    mqtt_.loop();
    mqtt_.disconnect();
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
