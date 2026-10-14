// Newline-delimited JSON over USB serial: the protocol spoken by the web
// console (docs/js/console.js). The wand only talks once a host has said
// something, so an unplugged USB port never stalls the firmware.
//
//   -> {"cmd":"hello"}                         <- {"evt":"hello", fw, board, id, spells, settings}
//   -> {"cmd":"stream","on":true}              <- {"evt":"live", az, el, q, casting, grip} at 30 Hz
//   -> {"cmd":"set","wifi_ssid":"...",...}     <- {"evt":"ok","cmd":"set"}
//   -> {"cmd":"record","name":"Accio"}         <- {"evt":"armed"} ... {"evt":"recorded", pts}
//   -> {"cmd":"forget","id":"accio"}           <- {"evt":"ok","cmd":"forget"}
//   -> {"cmd":"test","id":"malfoid"}           plays a spell's light + haptics
//   -> {"cmd":"status"} / "sleep" / "reboot"
//   unsolicited: {"evt":"cast", ok, id, score, pts, ...}, {"evt":"status", ...}
#pragma once

#include <ArduinoJson.h>

namespace console {

using Handler = void (*)(JsonDocument& msg);

void begin(Handler handler);
void poll();
void send(const JsonDocument& doc);
void error(const char* cmd, const char* message);
void ok(const char* cmd);
bool active(uint32_t now);  // a host spoke to us recently

}  // namespace console
