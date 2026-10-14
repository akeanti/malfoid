#include "console.h"

#include <Arduino.h>

#include "malfoid_config.h"

namespace console {

namespace {
Handler gHandler = nullptr;
char gLine[640];
size_t gLen = 0;
bool gOverflow = false;
uint32_t gLastRx = 0;
bool gHeard = false;
}  // namespace

void begin(Handler handler) { gHandler = handler; }

bool active(uint32_t now) {
  return gHeard && static_cast<int32_t>(now - gLastRx) < static_cast<int32_t>(cfg::kConsoleKeepAwakeMs);
}

void send(const JsonDocument& doc) {
  if (!active(millis())) return;
  serializeJson(doc, Serial);
  Serial.write('\n');
}

void error(const char* cmd, const char* message) {
  JsonDocument doc;
  doc["evt"] = "error";
  doc["cmd"] = cmd;
  doc["msg"] = message;
  send(doc);
}

void ok(const char* cmd) {
  JsonDocument doc;
  doc["evt"] = "ok";
  doc["cmd"] = cmd;
  send(doc);
}

void poll() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c != '\n') {
      if (gLen < sizeof(gLine) - 1) gLine[gLen++] = static_cast<char>(c);
      else gOverflow = true;
      continue;
    }
    gLine[gLen] = '\0';
    const bool overflowed = gOverflow;
    const size_t len = gLen;
    gLen = 0;
    gOverflow = false;
    if (len == 0) continue;

    gHeard = true;
    gLastRx = millis();
    if (overflowed) {
      error("", "line too long");
      continue;
    }
    JsonDocument msg;
    if (deserializeJson(msg, gLine) != DeserializationError::Ok || !msg["cmd"].is<const char*>()) {
      error("", "expected a JSON object with a \"cmd\"");
      continue;
    }
    if (gHandler != nullptr) gHandler(msg);
  }
}

}  // namespace console
