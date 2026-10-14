// Wi-Fi + MQTT, with Home Assistant MQTT discovery.
//
// Topics (base defaults to "malfoid", id = last 3 MAC bytes):
//   <base>/<id>/spell    payload = spell id, one message per successful cast
//   <base>/<id>/state    retained JSON: battery, voltage, charging, last_spell, rssi
//   <base>/<id>/status   retained "online" / "sleeping" / "offline" (LWT)
//
// Every spell becomes a Home Assistant *device trigger*, so automations can be
// built in the HA UI with "Malfoid: Lumen cast" and so on - no YAML needed.
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "fuel_gauge.h"
#include "settings.h"

class NetLink {
 public:
  using ConnectHook = void (*)();

  void begin(const Settings& s, const char* deviceId, ConnectHook onMqttConnect);
  void reconfigure();  // settings changed: drop connections and start over
  void loop(uint32_t now);
  void publishSpell(const char* id, float score, uint32_t now);
  void publishState(const Battery& b, const char* lastSpell);
  void announceSpell(const char* id);
  void announceSensors();
  void shutdown();

  bool wifiUp() const { return WiFi.status() == WL_CONNECTED; }
  bool mqttUp() { return mqtt_.connected(); }
  int rssi() const { return wifiUp() ? WiFi.RSSI() : 0; }

 private:
  struct Pending {
    char id[16];
    float score;
    uint32_t at;
    bool used;
  };

  void topic(char* out, size_t cap, const char* leaf) const;
  bool connectMqtt();
  void flushPending(uint32_t now);
  bool publishJson(const char* topic, const char* json, bool retained);

  const Settings* s_ = nullptr;
  char deviceId_[8] = "";
  WiFiClient tcp_;
  PubSubClient mqtt_{tcp_};
  ConnectHook onConnect_ = nullptr;
  bool wifiStarted_ = false;
  uint32_t lastWifiTry_ = 0;
  uint32_t lastMqttTry_ = 0;
  bool mqttTried_ = false;
  Pending pending_[4] = {};
};
