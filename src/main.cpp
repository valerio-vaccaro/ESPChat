// ESPChat - bitchat compatible BLE mesh messenger for WT32-SC01 / WT32-SC01 Plus.
#include <Arduino.h>
#include <lvgl.h>
#include "../include/board_config.h"
#include "chat/chat.h"
#include "crypto/identity.h"
#include "hal/audio.h"
#include "hal/display.h"
#include "hal/settings.h"
#include "hal/storage.h"
#include "mesh/ble_mesh.h"
#include "ui/ui.h"

void setup() {
  Serial.begin(115200);
  delay(50);
  log_i("ESPChat on %s, psram %u", BOARD_NAME, ESP.getPsramSize());

  gSettings.load();
  gIdentity.loadOrCreate();
  log_i("peer id %s", gIdentity.peerIdHex.c_str());

  displayBegin();
  storageBegin();
  audioBegin();
  audioSetVolume(gSettings.volume);

  mesh::begin(gSettings.centralRole);
  gChat.begin();
  uiInit();
  displaySetBrightness(gSettings.brightness);
}

void loop() {
  gChat.loop();
  uiTick();
  lv_timer_handler();
  delay(5);
}
