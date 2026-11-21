#include "chat_view.h"
#include <stdio.h>
#include "../../include/board_config.h"
#include "../hal/audio.h"
#include "../hal/settings.h"
#include "media.h"
#include "theme.h"
#include "ui.h"

using namespace theme;

std::string fmtClock(uint64_t epochMs) {
  int64_t s = (int64_t)(epochMs / 1000) + gSettings.utcOffset * 3600;
  int day = (int)((s % 86400 + 86400) % 86400);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", day / 3600, (day / 60) % 60);
  return b;
}

std::string fmtSize(size_t n) {
  char b[16];
  if (n < 1024) snprintf(b, sizeof(b), "%uB", (unsigned)n);
  else if (n < 1024 * 1024) snprintf(b, sizeof(b), "%.1fKB", n / 1024.0);
  else snprintf(b, sizeof(b), "%.1fMB", n / 1048576.0);
  return b;
}

const char* statusGlyph(uint8_t st) {
  switch (st) {
    case ST_SENDING: return " ..";
    case ST_SENT: return " " LV_SYMBOL_OK;
    case ST_DELIVERED: return " " LV_SYMBOL_OK LV_SYMBOL_OK;
    case ST_READ: return " " LV_SYMBOL_EYE_OPEN;
    case ST_FAILED: return " " LV_SYMBOL_WARNING;
    default: return "";
  }
}

static uint32_t statusColor(uint8_t st) {
  switch (st) {
    case ST_SENDING: return AMBER;
    case ST_DELIVERED: return CYAN;
    case ST_READ: return 0x9CFF3D;
    case ST_FAILED: return ALERT;
    default: return GREEN_MID;
  }
}

// ---------------------------------------------------------------------------
ChatView::ChatView(lv_obj_t* parent, const std::string& convId) : convId_(convId) {
  root_ = lv_obj_create(parent);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  list_ = lv_obj_create(root_);
  lv_obj_set_width(list_, LV_PCT(100));
  lv_obj_set_flex_grow(list_, 1);
  lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(list_, 4, 0);
  lv_obj_set_style_pad_row(list_, 3, 0);
  lv_obj_set_scroll_dir(list_, LV_DIR_VER);

  bar_ = lv_obj_create(root_);
  lv_obj_set_size(bar_, LV_PCT(100), 46);
  lv_obj_set_flex_flow(bar_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(bar_, 3, 0);
  lv_obj_set_style_pad_column(bar_, 3, 0);
  lv_obj_set_style_bg_color(bar_, lv_color_hex(PANEL), 0);
  lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(bar_, lv_color_hex(GREEN_DARK), 0);
  lv_obj_set_style_border_width(bar_, 1, 0);
  lv_obj_set_style_border_side(bar_, LV_BORDER_SIDE_TOP, 0);
  lv_obj_clear_flag(bar_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* attach = lv_btn_create(bar_);
  lv_obj_set_size(attach, 40, 38);
  lv_obj_t* l = lv_label_create(attach);
  lv_label_set_text(l, LV_SYMBOL_PLUS);
  lv_obj_center(l);
  lv_obj_set_style_border_color(attach, lv_color_hex(MAGENTA), 0);
  lv_obj_set_style_text_color(attach, lv_color_hex(MAGENTA), 0);
  lv_obj_add_event_cb(attach, onAttach, LV_EVENT_CLICKED, this);

  ta_ = lv_textarea_create(bar_);
  lv_obj_set_height(ta_, 38);
  lv_obj_set_flex_grow(ta_, 1);
  lv_textarea_set_one_line(ta_, true);
  lv_textarea_set_placeholder_text(ta_, convId_.empty() ? "> broadcast to #mesh_" : "> encrypted message_");
  lv_textarea_set_max_length(ta_, convId_.empty() ? 500 : 255);
  lv_obj_set_style_pad_top(ta_, 10, 0);
  lv_obj_set_style_text_color(ta_, lv_color_hex(GREEN_DIM), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_add_event_cb(ta_, onTaEvent, LV_EVENT_ALL, this);

  if (audioCanRecord()) {
    mic_ = lv_btn_create(bar_);
    lv_obj_set_size(mic_, 40, 38);
    l = lv_label_create(mic_);
    lv_label_set_text(l, LV_SYMBOL_AUDIO);
    lv_obj_center(l);
    lv_obj_set_style_border_color(mic_, lv_color_hex(AMBER), 0);
    lv_obj_set_style_text_color(mic_, lv_color_hex(AMBER), 0);
    lv_obj_add_event_cb(mic_, onMic, LV_EVENT_ALL, this);
  }

  lv_obj_t* send = lv_btn_create(bar_);
  lv_obj_set_size(send, 44, 38);
  l = lv_label_create(send);
  lv_label_set_text(l, LV_SYMBOL_RIGHT);
  lv_obj_center(l);
  lv_obj_add_event_cb(send, onSend, LV_EVENT_CLICKED, this);

  kb_ = lv_keyboard_create(root_);
  lv_obj_set_size(kb_, LV_PCT(100), 150);
  lv_keyboard_set_textarea(kb_, ta_);
  lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb_, onKbEvent, LV_EVENT_ALL, this);

  rebuild();
}

ChatView::~ChatView() {}

void ChatView::rebuild() {
  lv_obj_clean(list_);
  statusSpans_.clear();
  statusGroups_.clear();
  Conversation& c = gChat.conv(convId_);
  for (auto& m : c.msgs) append(m, false);
  scrollToEnd(false);
}

static void freeImgCb(lv_event_t* e) { imageFree(static_cast<UiImage*>(lv_event_get_user_data(e))); }
static void freeMediaRefCb(lv_event_t* e) { delete static_cast<std::shared_ptr<Media>*>(lv_event_get_user_data(e)); }

static void openImageCb(lv_event_t* e) {
  auto* ref = static_cast<std::shared_ptr<Media>*>(lv_event_get_user_data(e));
  uiOpenViewer(*ref);
}

static void playCb(lv_event_t* e) {
  auto* ref = static_cast<std::shared_ptr<Media>*>(lv_event_get_user_data(e));
  if (!audioCanPlay()) { uiToast("no speaker on this board"); return; }
  if (audioPlaying()) audioStop();
  else audioPlay(*ref);
}

static lv_span_t* addSpan(lv_obj_t* sg, const std::string& text, uint32_t color, const lv_font_t* font = nullptr) {
  lv_span_t* s = lv_spangroup_new_span(sg);
  lv_span_set_text(s, text.c_str());
  lv_style_set_text_color(&s->style, lv_color_hex(color));
  if (font) lv_style_set_text_font(&s->style, font);
  return s;
}

void ChatView::append(const ChatMsg& m, bool scroll) {
  while (lv_obj_get_child_cnt(list_) >= MAX_MESSAGES_PER_CONV) lv_obj_del(lv_obj_get_child(list_, 0));

  lv_obj_t* item = lv_obj_create(list_);
  lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(item, 3, 0);
  lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
  if (m.outgoing) {
    lv_obj_set_style_border_color(item, lv_color_hex(GREEN_DIM), 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_side(item, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_pad_left(item, 4, 0);
  }

  lv_obj_t* sg = lv_spangroup_create(item);
  lv_obj_set_width(sg, LV_PCT(100));
  lv_spangroup_set_mode(sg, LV_SPAN_MODE_BREAK);
  lv_spangroup_set_overflow(sg, LV_SPAN_OVERFLOW_CLIP);

  addSpan(sg, fmtClock(m.ts) + " ", GREEN_DIM, &monoSmall);
  if (m.system) {
    addSpan(sg, "* " + m.text, 0x4FB3BF);
  } else {
    if (!m.verified) addSpan(sg, "? ", AMBER);
    addSpan(sg, "<" + m.nick + "> ", m.outgoing ? GREEN_PALE : lv_color_to32(nickColor(m.senderId.c_str())) & 0xFFFFFF);
    if (!m.text.empty()) addSpan(sg, m.text, m.outgoing ? 0xD8FFE0 : GREEN);
    if (m.outgoing && !convId_.empty()) {
      lv_span_t* st = addSpan(sg, statusGlyph(m.status), statusColor(m.status));
      statusSpans_[m.id] = st;
      statusGroups_[m.id] = sg;
    }
  }
  lv_spangroup_refr_mode(sg);

  if (m.media) {
    auto ref = [&] { return new std::shared_ptr<Media>(m.media); };
    if (m.media->isImage()) {
      UiImage* img = imageDecode(m.media->data, 240, 150);
      if (img) {
        lv_obj_t* im = lv_img_create(item);
        lv_img_set_src(im, &img->dsc);
        lv_obj_set_style_border_color(im, lv_color_hex(MAGENTA), 0);
        lv_obj_set_style_border_width(im, 1, 0);
        lv_obj_add_flag(im, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(im, freeImgCb, LV_EVENT_DELETE, img);
        auto* r = ref();
        lv_obj_add_event_cb(im, openImageCb, LV_EVENT_CLICKED, r);
        lv_obj_add_event_cb(im, freeMediaRefCb, LV_EVENT_DELETE, r);
      } else {
        lv_obj_t* l = lv_label_create(item);
        lv_label_set_text_fmt(l, "[IMG %s %s - cannot decode]", m.media->name.c_str(), fmtSize(m.media->data.size()).c_str());
        lv_obj_set_style_text_color(l, lv_color_hex(GREEN_DIM), 0);
      }
    } else if (m.media->isAudio()) {
      lv_obj_t* b = lv_btn_create(item);
      lv_obj_set_size(b, 250, 34);
      lv_obj_set_style_border_color(b, lv_color_hex(AMBER), 0);
      lv_obj_set_style_text_color(b, lv_color_hex(AMBER), 0);
      lv_obj_t* l = lv_label_create(b);
      float d = audioDurationSec(*m.media);
      char txt[48];
      if (d > 0) snprintf(txt, sizeof(txt), LV_SYMBOL_PLAY "  VOICE  %.1fs   %s", d, fmtSize(m.media->data.size()).c_str());
      else snprintf(txt, sizeof(txt), LV_SYMBOL_PLAY "  VOICE   %s", fmtSize(m.media->data.size()).c_str());
      lv_label_set_text(l, txt);
      lv_obj_center(l);
      auto* r = ref();
      lv_obj_add_event_cb(b, playCb, LV_EVENT_CLICKED, r);
      lv_obj_add_event_cb(b, freeMediaRefCb, LV_EVENT_DELETE, r);
    } else {
      lv_obj_t* l = lv_label_create(item);
      lv_label_set_text_fmt(l, LV_SYMBOL_FILE " %s (%s)", m.media->name.c_str(), fmtSize(m.media->data.size()).c_str());
    }
  }
  if (scroll) scrollToEnd(true);
}

void ChatView::updateStatus(const std::string& msgId, uint8_t st) {
  auto it = statusSpans_.find(msgId);
  if (it == statusSpans_.end()) return;
  lv_span_set_text(it->second, statusGlyph(st));
  lv_style_set_text_color(&it->second->style, lv_color_hex(statusColor(st)));
  lv_spangroup_refr_mode(statusGroups_[msgId]);
}

void ChatView::scrollToEnd(bool anim) {
  lv_obj_update_layout(list_);
  lv_obj_scroll_to_y(list_, LV_COORD_MAX, anim ? LV_ANIM_ON : LV_ANIM_OFF);
}

void ChatView::focusInput() {
  lv_obj_add_state(ta_, LV_STATE_FOCUSED);
  lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
  scrollToEnd(false);
}

void ChatView::hideKeyboard() {
  lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(ta_, LV_STATE_FOCUSED);
}

void ChatView::send() {
  std::string text = lv_textarea_get_text(ta_);
  size_t a = text.find_first_not_of(" \t\n"), b = text.find_last_not_of(" \t\n");
  if (a == std::string::npos) return;
  text = text.substr(a, b - a + 1);
  std::string err;
  if (gChat.sendText(convId_, text, &err)) lv_textarea_set_text(ta_, "");
  else uiToast(err.c_str());
}

void ChatView::onSend(lv_event_t* e) { static_cast<ChatView*>(lv_event_get_user_data(e))->send(); }

void ChatView::onTaEvent(lv_event_t* e) {
  auto* v = static_cast<ChatView*>(lv_event_get_user_data(e));
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_FOCUSED || c == LV_EVENT_CLICKED) {
    lv_obj_clear_flag(v->kb_, LV_OBJ_FLAG_HIDDEN);
    v->scrollToEnd(false);
  }
}

void ChatView::onKbEvent(lv_event_t* e) {
  auto* v = static_cast<ChatView*>(lv_event_get_user_data(e));
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) v->send();
  else if (c == LV_EVENT_CANCEL) v->hideKeyboard();
}

void ChatView::onAttach(lv_event_t* e) {
  auto* v = static_cast<ChatView*>(lv_event_get_user_data(e));
  v->hideKeyboard();
  uiOpenAttach(v->convId_);
}

void ChatView::onMic(lv_event_t* e) {
  auto* v = static_cast<ChatView*>(lv_event_get_user_data(e));
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_PRESSED) {
    if (audioRecordStart()) uiShowRecording(true);
    else uiToast("microphone busy");
  } else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
    if (!audioRecording() && !uiRecordingShown()) return;
    uiShowRecording(false);
    Bytes wav;
    if (!audioRecordStop(wav)) { uiToast("hold to record (min 0.5s)"); return; }
    std::string name = "voice_" + makeUUID().substr(0, 8) + makeUUID().substr(0, 8) + ".wav";
    for (auto& ch : name) ch = (ch >= 'A' && ch <= 'F') ? ch + 32 : ch;
    std::string err;
    if (!gChat.sendMedia(v->convId_, name, "audio/wav", std::move(wav), &err)) uiToast(err.c_str());
  }
}
