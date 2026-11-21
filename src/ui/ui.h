// Screens and navigation.
//
// Landscape 480x320, sidebar navigation:
//   SPLASH ──(3s)──> MAIN [ #MESH | PEERS | ID | KEYS | CFG ]
//   #MESH / DM  ──[+]──> ATTACH menu ──> DOODLE | SD IMAGES
//   PEERS ──tap node──> DM (private, Noise encrypted) ──[<]──> MAIN
//   ID   : nickname, peer id, public keys (hex + QR)
//   KEYS : private keys (hidden until REVEAL, hex + QR), export to SD, import external key
//   image thumbnail ──tap──> VIEWER ──tap──> back
//   idle 2 min ──> SCREENSAVER (digital rain) ──tap──> back
//   red blinking LED in the headers = unread messages
#pragma once
#include <lvgl.h>
#include <memory>
#include <string>
struct Media;

void uiInit();   // builds the UI and shows the splash screen
void uiTick();   // call from loop()

void uiShowSplash();
void uiShowMain(int tab);                 // 0 #MESH, 1 PEERS, 2 ID, 3 KEYS, 4 CFG
void uiOpenDM(const std::string& peerId);
void uiOpenAttach(const std::string& convId);
void uiOpenDoodle(const std::string& convId);
void uiOpenSdPicker(const std::string& convId);
void uiOpenViewer(std::shared_ptr<Media> m);
void uiShowScreensaver(bool on);
void uiConfirmPanic();
void uiFocusInput();                      // open keyboard on the active chat
void uiToast(const char* text);
void uiShowRecording(bool on);
bool uiRecordingShown();
lv_obj_t* uiSettingsPage();
void uiDoodleDemo();                        // draws a sample doodle (screenshot tool)
void uiShowKeysRevealed(bool on);           // KEYS page reveal state
void uiFocusImport(const char* text);       // KEYS page: focus import field with text
