#pragma once
#include <stdint.h>
#include <string>

struct Settings {
  std::string nickname;
  bool relay = true;
  uint8_t brightness = 200;
  uint8_t volume = 70;
  int8_t utcOffset = 0;       // hours
  bool screensaver = true;
  bool centralRole = false;   // also connect to phones as BLE central
  void load();
  void save();
};
extern Settings gSettings;
