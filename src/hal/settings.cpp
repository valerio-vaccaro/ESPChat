#include "settings.h"
#include <Preferences.h>

Settings gSettings;

void Settings::load() {
  Preferences p;
  p.begin("bc_cfg", true);
  nickname = p.getString("nick", "").c_str();
  relay = p.getBool("relay", true);
  brightness = p.getUChar("bright", 200);
  volume = p.getUChar("vol", 70);
  utcOffset = p.getChar("utc", 0);
  screensaver = p.getBool("saver", true);
  centralRole = p.getBool("central", false);
  p.end();
}

void Settings::save() {
  Preferences p;
  p.begin("bc_cfg", false);
  p.putString("nick", nickname.c_str());
  p.putBool("relay", relay);
  p.putUChar("bright", brightness);
  p.putUChar("vol", volume);
  p.putChar("utc", utcOffset);
  p.putBool("saver", screensaver);
  p.putBool("central", centralRole);
  p.end();
}
