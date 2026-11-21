// Headless LVGL renderer: builds the real firmware UI with demo data and writes one
// screenshot per page/navigation step (PPM, converted to PNG by make_screenshots.py).
#include <lvgl.h>
#include <stdio.h>
#include <string>
#include "../../include/board_config.h"
#include "../../src/chat/chat.h"
#include "../../src/hal/audio.h"
#include "../../src/hal/settings.h"
#include "../../src/ui/ui.h"

extern uint32_t gFakeMillis;
void demoAddPeer(const char* id, const char* nick, bool online, bool secure, uint32_t ago);
void demoMsg(const std::string& conv, const char* from, const char* fromId, const char* text, int minute, bool out = false,
             uint8_t status = ST_NONE, std::shared_ptr<Media> media = nullptr, bool verified = true);
void demoSystem(const std::string& conv, const char* text, int minute);

static lv_color_t fb[SCREEN_W * SCREEN_H];
static lv_color_t drawBuf[SCREEN_W * SCREEN_H];
static std::string outDir;

static void flush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* px) {
  int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; ++y)
    for (int x = a->x1; x <= a->x2; ++x) fb[y * SCREEN_W + x] = px[(y - a->y1) * w + (x - a->x1)];
  lv_disp_flush_ready(d);
}

static lv_indev_data_t touch = {};
static void readTouch(lv_indev_drv_t*, lv_indev_data_t* d) { *d = touch; }

static void run(int ms) {
  for (int t = 0; t < ms; t += 5) {
    lv_tick_inc(5);
    gFakeMillis += 5;
    lv_timer_handler();
  }
}

static void shot(const char* name) {
  run(400);
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(nullptr);
  std::string path = outDir + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
    uint32_t c = lv_color_to32(fb[i]);
    uint8_t rgb[3] = {(uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("wrote %s\n", path.c_str());
}

static std::shared_ptr<Media> loadRaw(const char* file, const char* name, const char* mime) {
  auto m = std::make_shared<Media>();
  m->name = name;
  m->mime = mime;
  FILE* f = fopen(file, "rb");
  if (!f) { fprintf(stderr, "missing %s\n", file); exit(1); }
  uint8_t b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) m->data.insert(m->data.end(), b, b + n);
  fclose(f);
  return m;
}

static void typeInto(const char* text) {
  lv_obj_t* ta = nullptr;
  // the focused textarea of the active screen
  lv_obj_t* scr = lv_scr_act();
  std::vector<lv_obj_t*> stack{scr};
  while (!stack.empty()) {
    lv_obj_t* o = stack.back(); stack.pop_back();
    if (lv_obj_check_type(o, &lv_textarea_class) && lv_obj_is_visible(o) && lv_obj_has_state(o, LV_STATE_FOCUSED)) ta = o;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(o); ++i) stack.push_back(lv_obj_get_child(o, i));
  }
  if (ta) lv_textarea_set_text(ta, text);
}

int main(int argc, char** argv) {
  outDir = argc > 1 ? argv[1] : ".";
  std::string assets = argc > 2 ? argv[2] : ".";
  lv_init();
  static lv_disp_draw_buf_t db;
  lv_disp_draw_buf_init(&db, drawBuf, nullptr, SCREEN_W * SCREEN_H);
  static lv_disp_drv_t dd;
  lv_disp_drv_init(&dd);
  dd.hor_res = SCREEN_W;
  dd.ver_res = SCREEN_H;
  dd.flush_cb = flush;
  dd.draw_buf = &db;
  lv_disp_drv_register(&dd);
  static lv_indev_drv_t id;
  lv_indev_drv_init(&id);
  id.type = LV_INDEV_TYPE_POINTER;
  id.read_cb = readTouch;
  lv_indev_drv_register(&id);

  gChat.begin();
  auto rabbit = loadRaw((assets + "/white_rabbit.raw").c_str(), "img_5C1A.jpg", "image/jpeg");
  auto doodle = loadRaw((assets + "/doodle.raw").c_str(), "img_9E07.jpg", "image/jpeg");
  std::vector<uint8_t> silence(8000 * 4 + 1600, 128);
  auto voice = std::make_shared<Media>();
  voice->name = "voice_8d2f0c55aa13e9b1.wav";
  voice->mime = "audio/wav";
  voice->data = makeWav8(silence.data(), silence.size(), 8000);
  auto voice2 = std::make_shared<Media>(*voice);
  voice2->data = makeWav8(silence.data(), 8000 * 7 + 3200, 8000);
  voice2->name = "voice_11aa22bb33cc44dd.m4a";

  const char *TRI = "a1b2c3d4e5f60718", *MOR = "77e1aa09c3d24b5f", *ORA = "c0ffee1234567890", *TNK = "5eed5eed00112233",
             *ME = "3fa94c1e8b07d2a6";
  demoAddPeer(TRI, "trinity", true, true, 2000);
  demoAddPeer(MOR, "morpheus", true, false, 9000);
  demoAddPeer(TNK, "tank", true, false, 31000);
  demoAddPeer(ORA, "oracle", false, false, 12 * 60000);

  demoSystem("", "mesh online. you are @neo [3fa94c1e]", 0);
  demoMsg("", "morpheus", MOR, "anyone on the mesh near the old library?", 1);
  demoMsg("", "trinity", TRI, "copy. 3 hops from you, signal is clean", 2);
  demoMsg("", "neo", ME, "just flashed my wt32 node. hello mesh!", 3, true, ST_SENT);
  demoMsg("", "oracle", ORA, "welcome. you're not the one, you're a node ;)", 4);
  demoMsg("", "anon7f3a", "7f3a000000000000", "is this thing encrypted?", 5, false, ST_NONE, nullptr, false);
  demoMsg("", "morpheus", MOR, "#mesh is signed (Ed25519), not encrypted. DMs use Noise XX.", 6);
  demoMsg("", "trinity", TRI, nullptr, 7, false, ST_NONE, rabbit);
  demoMsg("", "tank", TNK, nullptr, 8, false, ST_NONE, voice);
  demoMsg("", "neo", ME, "follow the white rabbit", 9, true, ST_SENT);
  gChat.markRead("");

  demoSystem(TRI, "secure session established (Noise XX)", 3);
  demoMsg(TRI, "trinity", TRI, "neo. you there?", 4);
  demoMsg(TRI, "neo", ME, "yes. the doodle pad works!", 5, true, ST_READ);
  demoMsg(TRI, "neo", ME, nullptr, 5, true, ST_DELIVERED, doodle);
  demoMsg(TRI, "trinity", TRI, "ha. meet at the usual place. bring the board.", 7);
  demoMsg(TRI, "trinity", TRI, nullptr, 8, false, ST_NONE, voice2);
  demoMsg(TRI, "neo", ME, "on my way", 9, true, ST_SENT);
  gChat.markRead(TRI);
  demoMsg(MOR, "morpheus", MOR, "what if I told you...", 10);
  demoMsg(MOR, "morpheus", MOR, "ping me when you are secured", 11);
  demoMsg("", "tank", TNK, "operator online. lines are open.", 12);  // unread on #mesh until the tab opens

  gSettings.utcOffset = 1;
  uiInit();
  run(300);
  shot("01_splash");
  run(3500);
  shot("02_mesh_public_channel");
  uiFocusInput();
  typeInto("knock knock, neo");
  shot("03_mesh_keyboard");
  uiShowMain(0);
  uiOpenAttach("");
  shot("04_attach_menu");
  uiOpenDoodle("");
  uiDoodleDemo();
  shot("05_doodle");
  uiShowMain(0);
  run(400);
  uiOpenViewer(rabbit);
  shot("06_image_viewer");
  uiShowMain(1);
  demoMsg("", "oracle", ORA, "the red led means unread on #mesh, the blue one private", 13);
  shot("07_peers");
  uiOpenDM(TRI);
  shot("08_private_chat");
  uiShowRecording(true);
  run(1100);
  shot("09_voice_recording");
  uiShowRecording(false);
  uiOpenSdPicker(TRI);
  shot("10_sd_image_picker");
  uiShowMain(2);
  shot("11_identity");
  uiShowMain(3);
  shot("12_keys_hidden");
  uiShowKeysRevealed(true);
  shot("13_keys_revealed");
  uiShowKeysRevealed(false);
  uiFocusImport("4ccd089b28ff96da9db6c346ec114e0f 5b8a319f35aba624da8cf6ed4fb8a6fb");
  shot("14_key_import");
  uiShowMain(4);
  shot("15_settings");
  lv_obj_scroll_to_y(uiSettingsPage(), LV_COORD_MAX, LV_ANIM_OFF);
  shot("16_settings_bottom");
  uiConfirmPanic();
  shot("17_panic_confirm");
  lv_obj_del(lv_obj_get_child(lv_layer_top(), lv_obj_get_child_cnt(lv_layer_top()) - 1));
  run(100);
  uiShowScreensaver(true);
  run(1500);
  shot("18_screensaver");
  return 0;
}
