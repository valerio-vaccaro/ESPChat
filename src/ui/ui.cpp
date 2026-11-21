// Landscape (480x320) matrix-style UI.
#include "ui.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../../include/board_config.h"
#include "../chat/chat.h"
#include "../hal/audio.h"
#include "../hal/display.h"
#include "../hal/settings.h"
#include "../hal/storage.h"
#include "../platform.h"
#include "chat_view.h"
#include "matrix_rain.h"
#include "media.h"
#include "theme.h"

using namespace theme;

#define APP_VERSION "1.1.0"

static constexpr int HDR_H = 32;
static constexpr int SIDEBAR_W = 84;
static constexpr int KB_H = 150;

// ---------------------------------------------------------------------------
// state
static lv_obj_t* scrMain = nullptr;
static lv_obj_t* scrDM = nullptr;
static lv_obj_t* tabview = nullptr;
static lv_obj_t* hdrStatus = nullptr;
static lv_obj_t* hdrTitle = nullptr;
static lv_obj_t* peersList = nullptr;
static lv_obj_t* peersEmpty = nullptr;
static lv_obj_t* idPage = nullptr;
static lv_obj_t* keysPage = nullptr;
static lv_obj_t* cfgPage = nullptr;
static lv_obj_t* settingsInfo = nullptr;
static lv_obj_t* nickTa = nullptr;
static lv_obj_t* importTa = nullptr;
static lv_obj_t* globalKb = nullptr;
static lv_obj_t* kbPage = nullptr;  // page currently padded for the global keyboard
static lv_obj_t* saver = nullptr;
static lv_obj_t* recOverlay = nullptr;
static lv_obj_t* recLabel = nullptr;
static lv_obj_t* dmLock = nullptr;
static lv_obj_t* attachPanel = nullptr;
static ChatView* publicView = nullptr;
static ChatView* dmView = nullptr;
static std::string dmPeer;
static bool peersDirty = true;
static bool cursorOn = true;
static bool keysRevealed = false;

struct Led { lv_obj_t* led; lv_obj_t* count; bool priv; };  // red = #mesh unread, blue = private unread
static std::vector<Led> leds;

enum { TAB_MESH, TAB_PEERS, TAB_ID, TAB_KEYS, TAB_CFG };

static ChatView* activeView() {
  if (scrDM && lv_scr_act() == scrDM) return dmView;
  if (lv_scr_act() == scrMain && lv_tabview_get_tab_act(tabview) == TAB_MESH) return publicView;
  return nullptr;
}

// ---------------------------------------------------------------------------
// helpers
static lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, uint32_t color = GREEN, const lv_font_t* font = nullptr) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  if (font) lv_obj_set_style_text_font(l, font, 0);
  return l;
}

static lv_obj_t* makeBtn(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* ud = nullptr, uint32_t accent = 0) {
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_center(l);
  if (accent) {
    lv_obj_set_style_border_color(b, lv_color_hex(accent), 0);
    lv_obj_set_style_text_color(b, lv_color_hex(accent), 0);
  }
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

static void wrapLabel(lv_obj_t* l) {
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
}

static lv_obj_t* column(lv_obj_t* parent, lv_coord_t w = LV_PCT(100)) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, 3, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

static lv_obj_t* row(lv_obj_t* parent, lv_coord_t h = LV_SIZE_CONTENT) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_set_size(r, LV_PCT(100), h);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(r, 8, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  return r;
}

static lv_obj_t* sectionTitle(lv_obj_t* parent, const char* t, uint32_t color = CYAN) {
  lv_obj_t* l = makeLabel(parent, t, color);
  lv_obj_set_style_pad_top(l, 6, 0);
  return l;
}

static lv_obj_t* page(lv_obj_t* tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(tab, 8, 0);
  lv_obj_set_style_pad_row(tab, 4, 0);
  return tab;
}

static lv_obj_t* makeHeader(lv_obj_t* parent, int h) {
  lv_obj_t* hdr = lv_obj_create(parent);
  lv_obj_set_size(hdr, SCREEN_W, h);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(PANEL), 0);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(hdr, lv_color_hex(GREEN_DIM), 0);
  lv_obj_set_style_border_width(hdr, 1, 0);
  lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_hor(hdr, 6, 0);
  lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
  return hdr;
}

// unread "LED": red, blinking while anything is unread
static void ledDeleted(lv_event_t* e) {
  lv_obj_t* o = lv_event_get_target(e);
  for (size_t i = 0; i < leds.size(); ++i)
    if (leds[i].led == o) { leds.erase(leds.begin() + i); break; }
}

static void addLed(lv_obj_t* box, lv_coord_t x, uint32_t color, bool priv) {
  lv_obj_t* led = lv_led_create(box);
  lv_obj_set_size(led, 13, 13);
  lv_obj_align(led, LV_ALIGN_LEFT_MID, x + 2, 0);
  lv_led_set_color(led, lv_color_hex(color));
  lv_obj_set_style_shadow_spread(led, 2, 0);
  lv_led_off(led);
  lv_obj_t* cnt = makeLabel(box, "", color, &monoSmall);
  lv_obj_align(cnt, LV_ALIGN_LEFT_MID, x + 20, 0);
  lv_obj_add_event_cb(led, ledDeleted, LV_EVENT_DELETE, nullptr);
  leds.push_back({led, cnt, priv});
}

// Two virtual LEDs: red = unread on #mesh, blue = unread private messages.
static lv_obj_t* makeLed(lv_obj_t* parent, lv_align_t align, lv_coord_t x) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_set_size(box, 104, 24);
  lv_obj_align(box, align, x, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  addLed(box, 0, ALERT, false);
  addLed(box, 52, BLUE, true);
  return box;
}

static void ledTimer(lv_timer_t*) {
  static bool phase = false;
  phase = !phase;
  int pub = gChat.conv("").unread, priv = gChat.totalUnreadPrivate();
  for (auto& l : leds) {
    int unread = l.priv ? priv : pub;
    if (unread) {
      if (phase) lv_led_on(l.led);
      else lv_led_set_brightness(l.led, 90);
      lv_label_set_text_fmt(l.count, "%d", unread);
    } else {
      lv_led_set_brightness(l.led, 40);
      lv_label_set_text(l.count, "");
    }
  }
}

void uiToast(const char* text) {
  if (!text || !*text) return;
  lv_obj_t* t = lv_obj_create(lv_layer_top());
  lv_obj_set_size(t, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(t, lv_color_hex(PANEL2), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(t, lv_color_hex(CYAN), 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_pad_all(t, 8, 0);
  lv_obj_t* l = makeLabel(t, text, GREEN_PALE);
  lv_obj_set_style_max_width(l, SCREEN_W - 60, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, HDR_H + 6);
  lv_obj_del_delayed(t, 2400);
}

// ---------------------------------------------------------------------------
// global keyboard (identity / keys text fields)
static void kbHide() {
  if (!globalKb) return;
  lv_obj_add_flag(globalKb, LV_OBJ_FLAG_HIDDEN);
  if (kbPage) lv_obj_set_style_pad_bottom(kbPage, 8, 0);
  kbPage = nullptr;
  if (nickTa) lv_obj_clear_state(nickTa, LV_STATE_FOCUSED);
  if (importTa) lv_obj_clear_state(importTa, LV_STATE_FOCUSED);
}

static void kbShowFor(lv_obj_t* ta, lv_obj_t* pg) {
  lv_keyboard_set_textarea(globalKb, ta);
  lv_obj_clear_flag(globalKb, LV_OBJ_FLAG_HIDDEN);
  kbPage = pg;
  lv_obj_set_style_pad_bottom(pg, KB_H + 10, 0);
  lv_obj_update_layout(pg);
  lv_obj_scroll_to_view(ta, LV_ANIM_OFF);
}

static void nickSave(lv_event_t*);
static void importClicked(lv_event_t*);

static void globalKbEvent(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  lv_obj_t* ta = lv_keyboard_get_textarea(globalKb);
  if (c == LV_EVENT_READY) {
    if (ta == nickTa) nickSave(e);
    else if (ta == importTa) { kbHide(); importClicked(e); }
    else kbHide();
  } else if (c == LV_EVENT_CANCEL) {
    kbHide();
  }
}

static void taFocus(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c != LV_EVENT_FOCUSED && c != LV_EVENT_CLICKED) return;
  kbShowFor(lv_event_get_target(e), (lv_obj_t*)lv_event_get_user_data(e));
}

// ---------------------------------------------------------------------------
// recording overlay
void uiShowRecording(bool on) {
  if (on && !recOverlay) {
    recOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(recOverlay, 260, 90);
    lv_obj_align(recOverlay, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_color(recOverlay, lv_color_hex(PANEL), 0);
    lv_obj_set_style_bg_opa(recOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(recOverlay, lv_color_hex(ALERT), 0);
    lv_obj_set_style_border_width(recOverlay, 2, 0);
    lv_obj_set_style_shadow_color(recOverlay, lv_color_hex(ALERT), 0);
    lv_obj_set_style_shadow_width(recOverlay, 16, 0);
    lv_obj_set_style_shadow_opa(recOverlay, LV_OPA_40, 0);
    recLabel = makeLabel(recOverlay, "", AMBER, &monoLarge);
    lv_obj_set_style_text_align(recLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(recLabel);
  } else if (!on && recOverlay) {
    lv_obj_del(recOverlay);
    recOverlay = recLabel = nullptr;
  }
}
bool uiRecordingShown() { return recOverlay != nullptr; }

// ---------------------------------------------------------------------------
// screensaver
static void saverTouched(lv_event_t*) { uiShowScreensaver(false); }

void uiShowScreensaver(bool on) {
  if (on && !saver) {
    MatrixRain* r = new MatrixRain(lv_layer_top(), SCREEN_W, SCREEN_H);
    saver = r->root();
    lv_obj_add_flag(saver, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(saver, saverTouched, LV_EVENT_PRESSED, nullptr);
    lv_obj_t* l = makeLabel(saver, "", GREEN_PALE);
    int unread = gChat.totalUnread();
    if (unread) lv_label_set_text_fmt(l, "@%s  //  %d unread  //  tap to wake", gChat.nickname().c_str(), unread);
    else lv_label_set_text_fmt(l, "@%s  //  tap to wake", gChat.nickname().c_str());
    lv_obj_set_style_text_color(l, lv_color_hex(unread ? ALERT : GREEN_PALE), 0);
    lv_obj_set_style_bg_color(l, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(l, 4, 0);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -14);
    displaySetBrightness(gSettings.brightness / 3);
  } else if (!on && saver) {
    lv_obj_del(saver);
    saver = nullptr;
    displaySetBrightness(gSettings.brightness);
    lv_disp_trig_activity(nullptr);
  }
}

// ---------------------------------------------------------------------------
// splash
static void splashDone(lv_timer_t* t) {
  lv_timer_del(t);
  uiShowMain(TAB_MESH);
}

void uiShowSplash() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  new MatrixRain(scr, SCREEN_W, SCREEN_H);
  lv_obj_t* box = lv_obj_create(scr);
  lv_obj_set_size(box, 320, 170);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(BG), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_90, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(GREEN), 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_shadow_color(box, lv_color_hex(GREEN), 0);
  lv_obj_set_style_shadow_width(box, 26, 0);
  lv_obj_set_style_shadow_opa(box, LV_OPA_50, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, 8, 0);
  lv_obj_t* t = makeLabel(box, "B I T C H A T", GREEN_PALE, &monoLarge);
  lv_obj_set_style_text_letter_space(t, 3, 0);
  makeLabel(box, "decentralized mesh chat", CYAN);
  makeLabel(box, "----------------------------", GREEN_DARK);
  makeLabel(box, "> wake up, neo..._", GREEN);
  lv_obj_t* b = makeLabel(box, "", GREEN_DIM, &monoSmall);
  lv_label_set_text_fmt(b, "%s  //  v%s", BOARD_NAME, APP_VERSION);
  lv_scr_load(scr);
  lv_timer_t* tm = lv_timer_create(splashDone, 3200, nullptr);
  lv_timer_set_repeat_count(tm, 1);
}

// ---------------------------------------------------------------------------
// PEERS
static void peerClicked(lv_event_t* e) { uiOpenDM(*static_cast<std::string*>(lv_event_get_user_data(e))); }
static void freeStr(lv_event_t* e) { delete static_cast<std::string*>(lv_event_get_user_data(e)); }

static std::string agoText(uint32_t ms) {
  if (ms == UINT32_MAX) return "never";
  char b[16];
  if (ms < 60000) snprintf(b, sizeof(b), "%us ago", (unsigned)(ms / 1000));
  else if (ms < 3600000) snprintf(b, sizeof(b), "%um ago", (unsigned)(ms / 60000));
  else snprintf(b, sizeof(b), "%uh ago", (unsigned)(ms / 3600000));
  return b;
}

static void refreshPeers() {
  peersDirty = false;
  if (!peersList) return;
  lv_obj_clean(peersList);
  auto peers = gChat.peerList();
  for (auto& p : peers) {
    lv_obj_t* r = lv_btn_create(peersList);
    lv_obj_set_size(r, LV_PCT(100), 52);
    lv_obj_set_style_pad_all(r, 6, 0);
    lv_color_t nc = nickColor(p.id.c_str());
    if (p.online) lv_obj_set_style_border_color(r, nc, 0);
    else lv_obj_set_style_border_color(r, lv_color_hex(GREEN_DARK), 0);
    lv_obj_t* dot = makeLabel(r, "●", p.online ? GREEN : GREEN_DARK, &monoLarge);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* nick = makeLabel(r, "", 0);
    lv_obj_set_style_text_color(nick, p.online ? nc : lv_color_hex(GREEN_DIM), 0);
    lv_label_set_text_fmt(nick, "@%s", p.nick.c_str());
    lv_obj_align(nick, LV_ALIGN_TOP_LEFT, 22, -2);
    lv_obj_t* sub = makeLabel(r, "", GREEN_DIM, &monoSmall);
    lv_label_set_text_fmt(sub, "%s   %s", p.id.c_str(), p.online ? "online" : agoText(p.lastSeenMs).c_str());
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 22, 2);
    if (p.secure || p.handshaking) {
      lv_obj_t* st = makeLabel(r, p.secure ? "E2E" : "...", p.secure ? CYAN : AMBER);
      lv_obj_set_style_border_color(st, lv_color_hex(p.secure ? CYAN : AMBER), 0);
      lv_obj_set_style_border_width(st, 1, 0);
      lv_obj_set_style_pad_hor(st, 4, 0);
      lv_obj_align(st, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    if (p.unread) {
      lv_obj_t* badge = makeLabel(r, "", BG, &monoSmall);
      lv_label_set_text_fmt(badge, " %d ", p.unread);
      lv_obj_set_style_bg_color(badge, lv_color_hex(ALERT), 0);
      lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(badge, 8, 0);
      lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -50, 0);
    }
    auto* id = new std::string(p.id);
    lv_obj_add_event_cb(r, peerClicked, LV_EVENT_CLICKED, id);
    lv_obj_add_event_cb(r, freeStr, LV_EVENT_DELETE, id);
  }
  if (peersEmpty) {
    if (peers.empty()) lv_obj_clear_flag(peersEmpty, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(peersEmpty, LV_OBJ_FLAG_HIDDEN);
  }
  int unread = gChat.totalUnreadPrivate();
  static char cap[32];
  if (unread) snprintf(cap, sizeof(cap), LV_SYMBOL_LIST "\nPEERS %d", unread);
  else snprintf(cap, sizeof(cap), LV_SYMBOL_LIST "\nPEERS");
  lv_tabview_rename_tab(tabview, TAB_PEERS, cap);
}

static void buildPeersTab(lv_obj_t* tab) {
  page(tab);
  makeLabel(tab, "// NODES IN RANGE", CYAN);
  lv_obj_t* hint = makeLabel(tab, "tap a node to open an end-to-end encrypted channel (Noise XX)", GREEN_DIM, &monoSmall);
  wrapLabel(hint);
  peersEmpty = makeLabel(tab, "\n  scanning the mesh...\n  no nodes yet - keep bitchat open on a nearby phone.", AMBER);
  peersList = column(tab);
  lv_obj_set_style_pad_row(peersList, 5, 0);
}

// ---------------------------------------------------------------------------
// ID (public identity)
static void nickSave(lv_event_t*) {
  gChat.setNickname(lv_textarea_get_text(nickTa));
  lv_textarea_set_text(nickTa, gChat.nickname().c_str());
  kbHide();
  uiToast("nickname broadcast to the mesh");
}

static lv_obj_t* qrBox(lv_obj_t* parent, int size, const std::string& data, const char* caption, uint32_t color) {
  lv_obj_t* c = column(parent, size + 12);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* frame = lv_obj_create(c);
  lv_obj_set_size(frame, size + 12, size + 12);
  lv_obj_set_style_bg_color(frame, lv_color_hex(0xE8FFEE), 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(color), 0);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* qr = lv_qrcode_create(frame, size, lv_color_hex(0x000000), lv_color_hex(0xE8FFEE));
  lv_qrcode_update(qr, data.data(), data.size());
  lv_obj_center(qr);
  makeLabel(c, caption, color, &monoSmall);
  return c;
}

static std::string groupHex(const std::string& h, int every = 4) {
  std::string g;
  for (size_t i = 0; i < h.size(); i += every) g += h.substr(i, every) + (i + every < h.size() ? " " : "");
  return g;
}

static void keyLabel(lv_obj_t* parent, const char* title, const std::string& value, uint32_t color) {
  makeLabel(parent, title, color, &monoSmall);
  lv_obj_t* v = makeLabel(parent, groupHex(value).c_str(), GREEN_PALE, &monoSmall);
  wrapLabel(v);
}

static void buildIdTab(lv_obj_t* tab) {
  idPage = page(tab);
  lv_obj_clean(tab);
  nickTa = nullptr;
  KeyInfo k = gChat.keys();
  sectionTitle(tab, "// IDENTITY", VIOLET);

  lv_obj_t* nr = row(tab, 40);
  lv_obj_set_flex_align(nr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  makeLabel(nr, "nick", GREEN_MID);
  nickTa = lv_textarea_create(nr);
  lv_obj_set_flex_grow(nickTa, 1);
  lv_obj_set_height(nickTa, 38);
  lv_textarea_set_one_line(nickTa, true);
  lv_textarea_set_max_length(nickTa, 32);
  lv_textarea_set_text(nickTa, gChat.nickname().c_str());
  lv_obj_add_event_cb(nickTa, taFocus, LV_EVENT_ALL, tab);
  lv_obj_t* save = makeBtn(nr, "SET", nickSave, nullptr, GREEN);
  lv_obj_set_size(save, 60, 38);

  lv_obj_t* r = row(tab);
  lv_obj_t* left = column(r, 236);
  makeLabel(left, "peer id", CYAN, &monoSmall);
  makeLabel(left, gChat.myPeerId().c_str(), GREEN_PALE, &mono);
  keyLabel(left, "noise static key (X25519)", k.noisePub, CYAN);
  keyLabel(left, "signing key (Ed25519)", k.signPub, CYAN);
  std::string qr = "bitchat:" + gChat.myPeerId() + "?noise=" + k.noisePub + "&sign=" + k.signPub;
  qrBox(r, 120, qr, "public keys", VIOLET);

  makeLabel(tab, "fingerprint (sha256 of noise key)", CYAN, &monoSmall);
  lv_obj_t* fp = makeLabel(tab, groupHex(gChat.myFingerprint()).c_str(), GREEN_MID, &monoSmall);
  wrapLabel(fp);
  lv_obj_t* hint = makeLabel(tab, "peers verify you by this fingerprint. private keys: see KEYS.", GREEN_DIM, &monoSmall);
  wrapLabel(hint);
}

// ---------------------------------------------------------------------------
// KEYS (private keys, import / export)
static void buildKeysTab(lv_obj_t* tab);

static void revealClicked(lv_event_t*) {
  keysRevealed = !keysRevealed;
  buildKeysTab(keysPage);
}

static void exportClicked(lv_event_t*) {
  KeyInfo k = gChat.keys();
  std::string txt = k.noisePriv + k.signPriv + "\n";
  uiToast(storageWrite(IDENTITY_KEY_FILE, txt) ? "saved to SD:" IDENTITY_KEY_FILE : "no SD card");
}

static std::string pendingImport;
static void importConfirmed(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  const char* txt = lv_msgbox_get_active_btn_text(mbox);
  bool go = txt && !strcmp(txt, "REPLACE");
  lv_msgbox_close(mbox);
  if (!go) return;
  std::string err;
  if (!gChat.importIdentity(pendingImport, &err)) { uiToast(err.c_str()); return; }
  keysRevealed = false;
  buildIdTab(idPage);
  buildKeysTab(keysPage);
  peersDirty = true;
  uiToast("identity imported - announcing new keys");
}

static void askImport(const std::string& hex) {
  std::string clean;
  for (char c : hex)
    if (isxdigit((unsigned char)c)) clean += c;
  if (clean.size() != 64 && clean.size() != 128) {
    uiToast("expected 64 hex (x25519) or 128 hex (x25519 + ed25519 seed)");
    return;
  }
  pendingImport = clean;
  static const char* btns[] = {"REPLACE", "CANCEL", ""};
  lv_obj_t* m = lv_msgbox_create(nullptr, LV_SYMBOL_WARNING " IMPORT IDENTITY",
                                 clean.size() == 64 ? "Replace the noise key (a new signing key will be generated)? "
                                                      "Your peer id will change."
                                                    : "Replace both private keys? Your peer id will change.",
                                 btns, false);
  lv_obj_set_width(m, 340);
  lv_obj_center(m);
  lv_obj_set_style_border_color(m, lv_color_hex(AMBER), 0);
  lv_obj_add_event_cb(m, importConfirmed, LV_EVENT_VALUE_CHANGED, nullptr);
}

static void importClicked(lv_event_t*) {
  if (importTa) askImport(lv_textarea_get_text(importTa));
}

static void loadSdClicked(lv_event_t*) {
  Bytes b;
  if (!storageRead(IDENTITY_KEY_FILE, b, 1024)) { uiToast("cannot read SD:" IDENTITY_KEY_FILE); return; }
  askImport(std::string(b.begin(), b.end()));
}

static void buildKeysTab(lv_obj_t* tab) {
  keysPage = page(tab);
  lv_obj_clean(tab);
  importTa = nullptr;
  KeyInfo k = gChat.keys();
  sectionTitle(tab, "// PRIVATE KEYS", ALERT);
  lv_obj_t* w = makeLabel(tab, LV_SYMBOL_WARNING " anyone holding these keys can impersonate you and read your DMs.",
                          AMBER, &monoSmall);
  wrapLabel(w);

  lv_obj_t* r = row(tab);
  lv_obj_t* left = column(r, 236);
  if (keysRevealed) {
    keyLabel(left, "noise private (X25519)", k.noisePriv, ALERT);
    keyLabel(left, "signing seed (Ed25519)", k.signPriv, ALERT);
    qrBox(r, 120, k.noisePriv + k.signPriv, "private keys", ALERT);
  } else {
    makeLabel(left, "noise private (X25519)", ALERT, &monoSmall);
    makeLabel(left, "░░░░ hidden ░░░░", GREEN_DIM, &monoSmall);
    makeLabel(left, "signing seed (Ed25519)", ALERT, &monoSmall);
    makeLabel(left, "░░░░ hidden ░░░░", GREEN_DIM, &monoSmall);
    lv_obj_t* ph = lv_obj_create(r);
    lv_obj_set_size(ph, 132, 132);
    lv_obj_set_style_border_color(ph, lv_color_hex(GREEN_DARK), 0);
    lv_obj_set_style_border_width(ph, 2, 0);
    lv_obj_t* l = makeLabel(ph, "QR\nhidden", GREEN_DIM);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
  }
  lv_obj_t* br = row(tab, 42);
  lv_obj_t* b = makeBtn(br, keysRevealed ? LV_SYMBOL_EYE_CLOSE " HIDE" : LV_SYMBOL_EYE_OPEN " REVEAL", revealClicked,
                        nullptr, AMBER);
  lv_obj_set_size(b, 130, 38);
  b = makeBtn(br, LV_SYMBOL_SAVE " EXPORT SD", exportClicked, nullptr, CYAN);
  lv_obj_set_size(b, 150, 38);
  if (!storageAvailable()) lv_obj_add_state(b, LV_STATE_DISABLED);

  sectionTitle(tab, "// IMPORT EXTERNAL KEY", VIOLET);
  lv_obj_t* h = makeLabel(tab, "paste 64 hex chars (x25519 private key) or 128 hex (x25519 + ed25519 seed), "
                               "or load " IDENTITY_KEY_FILE " from the SD card.", GREEN_DIM, &monoSmall);
  wrapLabel(h);
  importTa = lv_textarea_create(tab);
  lv_obj_set_size(importTa, LV_PCT(100), 60);
  lv_textarea_set_max_length(importTa, 200);
  lv_textarea_set_placeholder_text(importTa, "hex private key...");
  lv_textarea_set_accepted_chars(importTa, "0123456789abcdefABCDEF :");
  lv_obj_set_style_text_font(importTa, &monoSmall, 0);
  lv_obj_add_event_cb(importTa, taFocus, LV_EVENT_ALL, tab);
  lv_obj_t* ir = row(tab, 42);
  b = makeBtn(ir, LV_SYMBOL_DOWNLOAD " IMPORT", importClicked, nullptr, VIOLET);
  lv_obj_set_size(b, 130, 38);
  b = makeBtn(ir, LV_SYMBOL_SD_CARD " LOAD SD", loadSdClicked, nullptr, CYAN);
  lv_obj_set_size(b, 150, 38);
  if (!storageAvailable()) lv_obj_add_state(b, LV_STATE_DISABLED);
}

// ---------------------------------------------------------------------------
// CFG
static void relayChanged(lv_event_t* e) {
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  gChat.setRelay(on);
  gSettings.relay = on;
  gSettings.save();
}
static void centralChanged(lv_event_t* e) {
  gSettings.centralRole = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  gSettings.save();
  gChat.setCentralRole(gSettings.centralRole);
}
static void saverChanged(lv_event_t* e) {
  gSettings.screensaver = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  gSettings.save();
}
static void brightChanged(lv_event_t* e) {
  gSettings.brightness = lv_slider_get_value(lv_event_get_target(e));
  displaySetBrightness(gSettings.brightness);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) gSettings.save();
}
static void volChanged(lv_event_t* e) {
  gSettings.volume = lv_slider_get_value(lv_event_get_target(e));
  audioSetVolume(gSettings.volume);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) gSettings.save();
}
static void utcChanged(lv_event_t* e) {
  gSettings.utcOffset = (int)lv_dropdown_get_selected(lv_event_get_target(e)) - 12;
  gSettings.save();
}
static void panicClicked(lv_event_t*) { uiConfirmPanic(); }

static lv_obj_t* settingRow(lv_obj_t* parent, const char* name) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_set_size(r, LV_PCT(100), 40);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* l = makeLabel(r, name, GREEN);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  return r;
}

static void refreshSettingsInfo() {
  if (!settingsInfo) return;
  auto peers = gChat.peerList();
  int online = 0;
  for (auto& p : peers) online += p.online;
  lv_label_set_text_fmt(settingsInfo,
                        "board     %s\nlinks     %d\nnodes     %d online / %d known\nclock     %s %s\n"
                        "heap      %u KB\npsram     %u KB\nfirmware  v%s",
                        BOARD_NAME, gChat.links(), online, (int)peers.size(), fmtClock(gChat.nowMs()).c_str(),
                        gChat.timeSynced() ? "(mesh sync)" : "(unsynced)", (unsigned)(sysFreeHeap() / 1024),
                        (unsigned)(sysFreePsram() / 1024), APP_VERSION);
}

static void buildCfgTab(lv_obj_t* tab) {
  cfgPage = page(tab);
  sectionTitle(tab, "// MESH");
  lv_obj_t* r = settingRow(tab, "relay packets for others");
  lv_obj_t* sw = lv_switch_create(r);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  if (gChat.relayEnabled()) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, relayChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  r = settingRow(tab, "also connect to phones");
  sw = lv_switch_create(r);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  if (gSettings.centralRole) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, centralChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  sectionTitle(tab, "// DISPLAY");
  r = settingRow(tab, "brightness");
  lv_obj_t* sl = lv_slider_create(r);
  lv_obj_set_width(sl, 170);
  lv_obj_align(sl, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_slider_set_range(sl, 10, 255);
  lv_slider_set_value(sl, gSettings.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(sl, brightChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sl, brightChanged, LV_EVENT_RELEASED, nullptr);
  r = settingRow(tab, "digital rain screensaver");
  sw = lv_switch_create(r);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  if (gSettings.screensaver) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, saverChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  r = settingRow(tab, "utc offset (hours)");
  lv_obj_t* dd = lv_dropdown_create(r);
  std::string opts;
  for (int i = -12; i <= 14; ++i) opts += (i >= 0 ? "+" : "") + std::to_string(i) + (i < 14 ? "\n" : "");
  lv_dropdown_set_options(dd, opts.c_str());
  lv_dropdown_set_selected(dd, gSettings.utcOffset + 12);
  lv_obj_set_width(dd, 90);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(dd, utcChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  sectionTitle(tab, "// AUDIO", AMBER);
  if (audioCanPlay()) {
    r = settingRow(tab, "volume");
    sl = lv_slider_create(r);
    lv_obj_set_width(sl, 170);
    lv_obj_align(sl, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_slider_set_range(sl, 0, 100);
    lv_slider_set_value(sl, gSettings.volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(AMBER), LV_PART_INDICATOR);
    lv_obj_add_event_cb(sl, volChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(sl, volChanged, LV_EVENT_RELEASED, nullptr);
  }
  lv_obj_t* a = makeLabel(tab, "", GREEN_DIM, &monoSmall);
  wrapLabel(a);
  lv_label_set_text_fmt(a, "speaker:    %s\nmicrophone: %s", audioCanPlay() ? "yes (wav, m4a/aac)" : "not available",
                        audioCanRecord() ? "yes (hold " LV_SYMBOL_AUDIO " to record)" : "not fitted (see board_config.h)");

  sectionTitle(tab, "// STATUS");
  settingsInfo = makeLabel(tab, "", GREEN_MID, &monoSmall);
  wrapLabel(settingsInfo);
  refreshSettingsInfo();

  sectionTitle(tab, "// DANGER ZONE", ALERT);
  lv_obj_t* p = makeBtn(tab, LV_SYMBOL_WARNING "  PANIC: WIPE ALL + NEW IDENTITY", panicClicked, nullptr, ALERT);
  lv_obj_set_width(p, LV_PCT(100));
}

lv_obj_t* uiSettingsPage() { return cfgPage; }

static void panicConfirmed(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  const char* txt = lv_msgbox_get_active_btn_text(mbox);
  bool wipe = txt && !strcmp(txt, "WIPE");
  lv_msgbox_close(mbox);
  if (!wipe) return;
  gChat.panic();
  uiToast("identity wiped. new keys generated.");
  keysRevealed = false;
  buildIdTab(idPage);
  buildKeysTab(keysPage);
  if (publicView) publicView->rebuild();
  peersDirty = true;
}

void uiConfirmPanic() {
  static const char* btns[] = {"WIPE", "CANCEL", ""};
  lv_obj_t* m = lv_msgbox_create(nullptr, LV_SYMBOL_WARNING " PANIC",
                                 "Leave the mesh, delete all messages and peers, and generate a new identity?", btns, false);
  lv_obj_set_width(m, 340);
  lv_obj_center(m);
  lv_obj_set_style_border_color(m, lv_color_hex(ALERT), 0);
  lv_obj_add_event_cb(m, panicConfirmed, LV_EVENT_VALUE_CHANGED, nullptr);
}

// ---------------------------------------------------------------------------
// main screen
static void tabChanged(lv_event_t*) {
  uint16_t t = lv_tabview_get_tab_act(tabview);
  if (publicView) publicView->hideKeyboard();
  kbHide();
  if (t == TAB_MESH) gChat.markRead("");
  if (t == TAB_PEERS) peersDirty = true;
  if (t == TAB_CFG) refreshSettingsInfo();
}

static void buildMain() {
  scrMain = lv_obj_create(nullptr);
  lv_obj_clear_flag(scrMain, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* hdr = makeHeader(scrMain, HDR_H);
  hdrTitle = makeLabel(hdr, "BITCHAT_", GREEN_PALE, &monoLarge);
  lv_obj_align(hdrTitle, LV_ALIGN_LEFT_MID, 0, 0);
  makeLed(hdr, LV_ALIGN_LEFT_MID, 108);
  hdrStatus = makeLabel(hdr, "", CYAN, &monoSmall);
  lv_obj_align(hdrStatus, LV_ALIGN_RIGHT_MID, 0, 0);

  tabview = lv_tabview_create(scrMain, LV_DIR_LEFT, SIDEBAR_W);
  lv_obj_set_size(tabview, SCREEN_W, SCREEN_H - HDR_H);
  lv_obj_set_y(tabview, HDR_H);
  lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* tMesh = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME "\n#MESH");
  lv_obj_t* tPeers = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST "\nPEERS");
  lv_obj_t* tId = lv_tabview_add_tab(tabview, LV_SYMBOL_EYE_OPEN "\nID");
  lv_obj_t* tKeys = lv_tabview_add_tab(tabview, LV_SYMBOL_EYE_CLOSE "\nKEYS");
  lv_obj_t* tCfg = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS "\nCFG");
  lv_obj_t* btns = lv_tabview_get_tab_btns(tabview);
  lv_obj_set_style_border_side(btns, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_text_font(btns, &monoSmall, LV_PART_ITEMS);
  lv_obj_add_event_cb(tabview, tabChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_clear_flag(tMesh, LV_OBJ_FLAG_SCROLLABLE);
  publicView = new ChatView(tMesh, "");
  buildPeersTab(tPeers);
  buildIdTab(tId);
  buildKeysTab(tKeys);
  buildCfgTab(tCfg);

  globalKb = lv_keyboard_create(lv_layer_top());
  lv_obj_set_size(globalKb, SCREEN_W, KB_H);
  lv_obj_align(globalKb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(globalKb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(globalKb, globalKbEvent, LV_EVENT_ALL, nullptr);
}

void uiShowMain(int tab) {
  lv_tabview_set_act(tabview, tab, LV_ANIM_OFF);
  tabChanged(nullptr);
  if (lv_scr_act() != scrMain) lv_scr_load_anim(scrMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, lv_scr_act() != scrDM);
}

void uiShowKeysRevealed(bool on) {
  keysRevealed = on;
  buildKeysTab(keysPage);
}

void uiFocusImport(const char* text) {
  if (!importTa) return;
  lv_obj_add_state(importTa, LV_STATE_FOCUSED);
  lv_textarea_set_text(importTa, text);
  kbShowFor(importTa, keysPage);
}

// ---------------------------------------------------------------------------
// DM screen
static void dmBack(lv_event_t*) {
  lv_scr_load_anim(scrMain, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, true);
  peersDirty = true;
}

static void dmDeleted(lv_event_t*) {
  delete dmView;
  dmView = nullptr;
  scrDM = nullptr;
  dmLock = nullptr;
  dmPeer.clear();
}

static void dmLockClicked(lv_event_t*) {
  if (!gChat.peerSecure(dmPeer)) {
    gChat.startSession(dmPeer);
    uiToast("starting Noise XX handshake...");
  } else {
    uiToast("secured: Noise_XX_25519_ChaChaPoly_SHA256");
  }
}

static void refreshDmHeader() {
  if (!dmLock) return;
  bool sec = gChat.peerSecure(dmPeer);
  lv_label_set_text(lv_obj_get_child(dmLock, 0), sec ? "E2E" : "...");
  uint32_t c = sec ? CYAN : AMBER;
  lv_obj_set_style_border_color(dmLock, lv_color_hex(c), 0);
  lv_obj_set_style_text_color(dmLock, lv_color_hex(c), 0);
}

void uiOpenDM(const std::string& peerId) {
  if (scrDM) lv_obj_del(scrDM);
  dmPeer = peerId;
  scrDM = lv_obj_create(nullptr);
  lv_obj_clear_flag(scrDM, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scrDM, dmDeleted, LV_EVENT_DELETE, nullptr);
  lv_obj_t* hdr = makeHeader(scrDM, 40);
  lv_obj_t* back = makeBtn(hdr, LV_SYMBOL_LEFT, dmBack);
  lv_obj_set_size(back, 44, 32);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, -2, 0);
  lv_obj_t* t = makeLabel(hdr, "", 0, &monoLarge);
  lv_obj_set_style_text_color(t, nickColor(peerId.c_str()), 0);
  lv_label_set_text_fmt(t, "@%s", gChat.peerNick(peerId).c_str());
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 52, -9);
  lv_obj_t* s = makeLabel(hdr, "", GREEN_DIM, &monoSmall);
  lv_label_set_text_fmt(s, "%s  private channel", peerId.c_str());
  lv_obj_align(s, LV_ALIGN_LEFT_MID, 52, 11);
  makeLed(hdr, LV_ALIGN_RIGHT_MID, -66);
  dmLock = makeBtn(hdr, "...", dmLockClicked);
  lv_obj_set_size(dmLock, 60, 32);
  lv_obj_align(dmLock, LV_ALIGN_RIGHT_MID, 2, 0);

  lv_obj_t* body = lv_obj_create(scrDM);
  lv_obj_set_size(body, SCREEN_W, SCREEN_H - 40);
  lv_obj_set_y(body, 40);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  dmView = new ChatView(body, peerId);
  refreshDmHeader();
  gChat.markRead(peerId);
  if (!gChat.peerSecure(peerId)) gChat.startSession(peerId);
  lv_scr_load_anim(scrDM, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void uiFocusInput() {
  if (ChatView* v = activeView()) v->focusInput();
}

// ---------------------------------------------------------------------------
// attach menu
static std::string attachConv;
static void attachClose() {
  if (attachPanel) { lv_obj_del(attachPanel); attachPanel = nullptr; }
}
static void attachDoodle(lv_event_t*) { uiOpenDoodle(attachConv); }
static void attachSd(lv_event_t*) { uiOpenSdPicker(attachConv); }
static void attachCancel(lv_event_t*) { attachClose(); }

void uiOpenAttach(const std::string& convId) {
  attachClose();
  attachConv = convId;
  attachPanel = lv_obj_create(lv_layer_top());
  lv_obj_set_size(attachPanel, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(attachPanel, lv_color_hex(BG), 0);
  lv_obj_set_style_bg_opa(attachPanel, LV_OPA_70, 0);
  lv_obj_add_flag(attachPanel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(attachPanel, attachCancel, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* box = lv_obj_create(attachPanel);
  lv_obj_set_size(box, 380, LV_SIZE_CONTENT);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(PANEL), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(MAGENTA), 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_pad_all(box, 10, 0);
  lv_obj_set_style_pad_row(box, 8, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* t = makeLabel(box, "", MAGENTA);
  lv_label_set_text_fmt(t, "// TRANSMIT MEDIA %s", convId.empty() ? "TO #MESH" : "(ENCRYPTED)");
  lv_obj_t* b = makeBtn(box, LV_SYMBOL_EDIT "  DOODLE   - draw & send a jpeg", attachDoodle, nullptr, GREEN);
  lv_obj_set_width(b, LV_PCT(100));
  b = makeBtn(box, LV_SYMBOL_IMAGE "  SD CARD  - send an image", attachSd, nullptr, CYAN);
  lv_obj_set_width(b, LV_PCT(100));
  if (!storageAvailable()) lv_obj_add_state(b, LV_STATE_DISABLED);
  lv_obj_t* hint = makeLabel(box, "", AMBER, &monoSmall);
  wrapLabel(hint);
  lv_label_set_text(hint, audioCanRecord() ? LV_SYMBOL_AUDIO " voice notes: hold the mic button in the input bar"
                                           : LV_SYMBOL_AUDIO " voice notes: playback only (no microphone fitted)");
  b = makeBtn(box, "CANCEL", attachCancel);
  lv_obj_set_width(b, LV_PCT(100));
}

// ---------------------------------------------------------------------------
// doodle (landscape: canvas left, tools right)
static constexpr int DOODLE_W = 280, DOODLE_H = 276;
struct Doodle {
  lv_obj_t* scr;
  lv_obj_t* canvas;
  lv_color_t* buf;
  lv_color_t color;
  int width = 4;
  lv_point_t last;
  bool drawing = false;
  std::string conv;
  lv_obj_t* prev;
};
static Doodle* doodle = nullptr;

static void doodleClose() { lv_scr_load_anim(doodle->prev, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true); }
static void doodleDeleted(lv_event_t*) {
  free(doodle->buf);
  delete doodle;
  doodle = nullptr;
}
static void doodleLine(lv_point_t a, lv_point_t b) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = doodle->color;
  d.width = doodle->width;
  d.round_start = 1;
  d.round_end = 1;
  lv_point_t pts[2] = {a, b};
  lv_canvas_draw_line(doodle->canvas, pts, 2, &d);
}
static void doodleTouch(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c != LV_EVENT_PRESSED && c != LV_EVENT_PRESSING && c != LV_EVENT_RELEASED && c != LV_EVENT_PRESS_LOST) return;
  lv_indev_t* indev = lv_indev_get_act();
  if (!doodle || !indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  lv_area_t a;
  lv_obj_get_coords(doodle->canvas, &a);
  p.x -= a.x1;
  p.y -= a.y1;
  if (c == LV_EVENT_PRESSED) { doodle->last = p; doodle->drawing = true; doodleLine(p, p); }
  else if (c == LV_EVENT_PRESSING && doodle->drawing) { doodleLine(doodle->last, p); doodle->last = p; }
  else doodle->drawing = false;
}
static void doodleClear(lv_event_t*) { lv_canvas_fill_bg(doodle->canvas, lv_color_hex(BG), LV_OPA_COVER); }
static void doodleCancel(lv_event_t*) { doodleClose(); }
static void doodleSend(lv_event_t*) {
  Bytes jpg;
  if (!jpegEncode((const uint8_t*)doodle->buf, DOODLE_W, DOODLE_H, 70, jpg)) { uiToast("jpeg encoding failed"); return; }
  std::string name = "img_" + makeUUID() + ".jpg";
  std::string err;
  if (!gChat.sendMedia(doodle->conv, name, "image/jpeg", std::move(jpg), &err)) { uiToast(err.c_str()); return; }
  doodleClose();
}
static void doodleColor(lv_event_t* e) { doodle->color = lv_color_hex((uint32_t)(uintptr_t)lv_event_get_user_data(e)); }
static void doodleWidth(lv_event_t* e) {
  doodle->width = doodle->width == 4 ? 10 : 4;
  lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), doodle->width == 4 ? "thin" : "BOLD");
}

void uiOpenDoodle(const std::string& convId) {
  attachClose();
  lv_color_t* buf = (lv_color_t*)malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(DOODLE_W, DOODLE_H));
  if (!buf) { uiToast("out of memory"); return; }
  doodle = new Doodle();
  doodle->buf = buf;
  doodle->conv = convId;
  doodle->color = lv_color_hex(GREEN);
  doodle->prev = lv_scr_act();
  doodle->scr = lv_obj_create(nullptr);
  lv_obj_clear_flag(doodle->scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(doodle->scr, doodleDeleted, LV_EVENT_DELETE, nullptr);
  lv_obj_t* hdr = makeHeader(doodle->scr, HDR_H);
  lv_obj_t* t = makeLabel(hdr, "", MAGENTA);
  lv_label_set_text_fmt(t, LV_SYMBOL_EDIT " DOODLE -> %s", convId.empty() ? "#mesh" : ("@" + gChat.peerNick(convId)).c_str());
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);

  doodle->canvas = lv_canvas_create(doodle->scr);
  lv_canvas_set_buffer(doodle->canvas, buf, DOODLE_W, DOODLE_H, LV_IMG_CF_TRUE_COLOR);
  lv_canvas_fill_bg(doodle->canvas, lv_color_hex(BG), LV_OPA_COVER);
  lv_obj_set_pos(doodle->canvas, 6, HDR_H + 6);
  lv_obj_set_style_outline_color(doodle->canvas, lv_color_hex(MAGENTA), 0);
  lv_obj_set_style_outline_width(doodle->canvas, 1, 0);
  lv_obj_add_flag(doodle->canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(doodle->canvas, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(doodle->canvas, doodleTouch, LV_EVENT_ALL, nullptr);

  lv_obj_t* tools = lv_obj_create(doodle->scr);
  lv_obj_set_size(tools, SCREEN_W - DOODLE_W - 20, SCREEN_H - HDR_H - 8);
  lv_obj_set_pos(tools, DOODLE_W + 14, HDR_H + 4);
  lv_obj_set_flex_flow(tools, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(tools, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(tools, 6, 0);
  lv_obj_clear_flag(tools, LV_OBJ_FLAG_SCROLLABLE);
  static const uint32_t colors[] = {GREEN, CYAN, AMBER, MAGENTA, VIOLET, 0xFFFFFF, ALERT, BG};
  for (uint32_t c : colors) {
    lv_obj_t* b = lv_btn_create(tools);
    lv_obj_set_size(b, 40, 34);
    lv_obj_set_style_bg_color(b, lv_color_hex(c), 0);
    lv_obj_add_event_cb(b, doodleColor, LV_EVENT_CLICKED, (void*)(uintptr_t)c);
  }
  lv_obj_t* w = makeBtn(tools, "thin", doodleWidth);
  lv_obj_set_size(w, 84, 34);
  lv_obj_t* b = makeBtn(tools, LV_SYMBOL_TRASH " CLR", doodleClear, nullptr, AMBER);
  lv_obj_set_size(b, 84, 34);
  b = makeBtn(tools, LV_SYMBOL_CLOSE " BACK", doodleCancel);
  lv_obj_set_size(b, 84, 40);
  b = makeBtn(tools, "SEND " LV_SYMBOL_RIGHT, doodleSend, nullptr, GREEN);
  lv_obj_set_size(b, 84, 40);
  lv_obj_set_style_bg_color(b, lv_color_hex(GREEN_DARK), 0);
  lv_scr_load_anim(doodle->scr, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

void uiDoodleDemo() {
  if (!doodle) return;
  auto L = [](int x0, int y0, int x1, int y1) { doodleLine({(lv_coord_t)x0, (lv_coord_t)y0}, {(lv_coord_t)x1, (lv_coord_t)y1}); };
  doodle->width = 6;
  for (int i = 0; i < 36; ++i) {
    float a0 = i * 6.2832f / 36, a1 = (i + 1) * 6.2832f / 36;
    L(140 + 70 * cosf(a0), 165 + 55 * sinf(a0), 140 + 70 * cosf(a1), 165 + 55 * sinf(a1));
  }
  L(100, 120, 85, 30); L(85, 30, 115, 115);
  L(165, 115, 180, 25); L(180, 25, 185, 118);
  doodle->color = lv_color_hex(ALERT);
  L(115, 155, 117, 157); L(165, 155, 167, 157);
  doodle->color = lv_color_hex(MAGENTA);
  doodle->width = 3;
  L(130, 190, 140, 197); L(140, 197, 150, 190);
  doodle->color = lv_color_hex(CYAN);
  L(20, 255, 260, 255);
}

// ---------------------------------------------------------------------------
// SD picker
static std::string sdConv;
static lv_obj_t* sdPrev = nullptr;
static void sdBack(lv_event_t*) { lv_scr_load_anim(sdPrev, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true); }
static void sdPick(lv_event_t* e) {
  auto* path = static_cast<std::string*>(lv_event_get_user_data(e));
  Bytes data;
  size_t limit = sdConv.empty() ? MAX_PUBLIC_MEDIA_BYTES - 512 : 100 * 1024;
  if (!storageRead(*path, data, limit)) { uiToast("file too large or unreadable"); return; }
  std::string lower = *path;
  for (auto& c : lower) c = tolower(c);
  bool png = lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".png") == 0;
  std::string name = "img_" + makeUUID() + (png ? ".png" : ".jpg");
  std::string err;
  if (!gChat.sendMedia(sdConv, name, png ? "image/png" : "image/jpeg", std::move(data), &err)) { uiToast(err.c_str()); return; }
  sdBack(nullptr);
}

void uiOpenSdPicker(const std::string& convId) {
  attachClose();
  sdConv = convId;
  sdPrev = lv_scr_act();
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_t* hdr = makeHeader(scr, 40);
  lv_obj_t* back = makeBtn(hdr, LV_SYMBOL_LEFT, sdBack);
  lv_obj_set_size(back, 44, 32);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, -2, 0);
  lv_obj_t* t = makeLabel(hdr, LV_SYMBOL_SD_CARD "  SD:/bitchat/send", CYAN, &monoLarge);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 52, 0);
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_size(list, SCREEN_W, SCREEN_H - 40);
  lv_obj_set_y(list, 40);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(list, 8, 0);
  lv_obj_set_style_pad_row(list, 5, 0);
  auto files = storageListImages();
  if (files.empty()) makeLabel(list, "no .jpg/.png files found.\ncopy images to /bitchat/send", AMBER);
  for (auto& f : files) {
    lv_obj_t* b = makeBtn(list, (LV_SYMBOL_IMAGE "  " + f).c_str(), nullptr, nullptr, MAGENTA);
    lv_obj_set_width(b, LV_PCT(100));
    auto* p = new std::string(f);
    lv_obj_add_event_cb(b, sdPick, LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(b, freeStr, LV_EVENT_DELETE, p);
  }
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

// ---------------------------------------------------------------------------
// image viewer
static lv_obj_t* viewerPrev = nullptr;
static void viewerClose(lv_event_t*) { lv_scr_load_anim(viewerPrev, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true); }
static void viewerFree(lv_event_t* e) { imageFree(static_cast<UiImage*>(lv_event_get_user_data(e))); }

void uiOpenViewer(std::shared_ptr<Media> m) {
  if (!m) return;
  viewerPrev = lv_scr_act();
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, viewerClose, LV_EVENT_CLICKED, nullptr);
  UiImage* img = imageDecode(m->data, SCREEN_W, SCREEN_H - 34);
  if (img) {
    lv_obj_t* im = lv_img_create(scr);
    lv_img_set_src(im, &img->dsc);
    lv_obj_align(im, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_event_cb(scr, viewerFree, LV_EVENT_DELETE, img);
  }
  lv_obj_t* cap = makeLabel(scr, "", MAGENTA, &monoSmall);
  lv_label_set_text_fmt(cap, LV_SYMBOL_IMAGE " %s  %s   //  tap to close", m->name.c_str(), fmtSize(m->data.size()).c_str());
  lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

// ---------------------------------------------------------------------------
// periodic refresh
static void statusTimer(lv_timer_t*) {
  cursorOn = !cursorOn;
  lv_label_set_text(hdrTitle, cursorOn ? "BITCHAT_" : "BITCHAT ");
  auto peers = gChat.peerList();
  int online = 0;
  for (auto& p : peers) online += p.online;
  lv_label_set_text_fmt(hdrStatus, LV_SYMBOL_BLUETOOTH " %d links   ● %d nodes   %s%s", gChat.links(), online,
                        fmtClock(gChat.nowMs()).c_str(), gChat.timeSynced() ? "" : "?");
  if (lv_scr_act() == scrMain && lv_tabview_get_tab_act(tabview) == TAB_CFG) refreshSettingsInfo();
  refreshDmHeader();
  if (recOverlay) {
    char b[64];
    snprintf(b, sizeof(b), LV_SYMBOL_AUDIO " REC %.1fs / %ds\nrelease to send", audioRecordSeconds(), VOICE_MAX_SECONDS);
    lv_label_set_text(recLabel, b);
  }
}

static void peersTimer(lv_timer_t*) {
  if (peersDirty) refreshPeers();
  if (gSettings.screensaver && !saver && lv_disp_get_inactive_time(nullptr) > 120000 && !audioPlaying())
    uiShowScreensaver(true);
}

void uiInit() {
  theme::init(lv_disp_get_default());
  buildMain();

  gChat.onMessage = [](const std::string& convId, const ChatMsg& m) {
    if (convId.empty()) {
      publicView->append(m);
      if (lv_scr_act() == scrMain && lv_tabview_get_tab_act(tabview) == TAB_MESH) gChat.conv("").unread = 0;
    } else if (dmView && dmPeer == convId) {
      dmView->append(m);
      if (lv_scr_act() == scrDM) gChat.markRead(convId);
    }
    if (!m.outgoing && !m.system && !convId.empty() && !(dmView && dmPeer == convId))
      uiToast(("@" + m.nick + ": " + (m.media ? std::string("[media]") : m.text.substr(0, 40))).c_str());
    peersDirty = true;
  };
  gChat.onStatus = [](const std::string& convId, const std::string& id, uint8_t st) {
    if (dmView && dmPeer == convId) dmView->updateStatus(id, st);
  };
  gChat.onPeersChanged = [] { peersDirty = true; };

  lv_timer_create(statusTimer, 1000, nullptr);
  lv_timer_create(peersTimer, 500, nullptr);
  lv_timer_create(ledTimer, 450, nullptr);
  statusTimer(nullptr);
  refreshPeers();
  uiShowSplash();
}

void uiTick() {
  if (recOverlay && !audioRecording() && audioRecordSeconds() >= VOICE_MAX_SECONDS - 0.05f) {
    // auto-stop at the maximum duration
    uiShowRecording(false);
    Bytes wav;
    ChatView* v = activeView();
    if (v && audioRecordStop(wav)) {
      std::string name = "voice_" + makeUUID().substr(0, 8) + makeUUID().substr(0, 8) + ".wav";
      for (auto& ch : name) ch = (ch >= 'A' && ch <= 'F') ? ch + 32 : ch;
      gChat.sendMedia(v->convId(), name, "audio/wav", std::move(wav));
    }
  }
}
