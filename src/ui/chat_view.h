// A conversation view: scrolling message log + input bar + on-screen keyboard.
#pragma once
#include <lvgl.h>
#include <map>
#include <string>
#include "../chat/chat.h"

class ChatView {
 public:
  ChatView(lv_obj_t* parent, const std::string& convId);
  ~ChatView();
  void rebuild();
  void append(const ChatMsg& m, bool scroll = true);
  void updateStatus(const std::string& msgId, uint8_t st);
  void focusInput();      // open the keyboard
  void hideKeyboard();
  const std::string& convId() const { return convId_; }
  lv_obj_t* root() const { return root_; }

 private:
  static void onSend(lv_event_t* e);
  static void onTaEvent(lv_event_t* e);
  static void onKbEvent(lv_event_t* e);
  static void onAttach(lv_event_t* e);
  static void onMic(lv_event_t* e);
  void send();
  void scrollToEnd(bool anim);

  std::string convId_;
  lv_obj_t* root_;
  lv_obj_t* list_;
  lv_obj_t* bar_;
  lv_obj_t* ta_;
  lv_obj_t* kb_;
  lv_obj_t* mic_ = nullptr;
  std::map<std::string, lv_span_t*> statusSpans_;
  std::map<std::string, lv_obj_t*> statusGroups_;
};

// Shared formatting helpers
std::string fmtClock(uint64_t epochMs);
std::string fmtSize(size_t bytes);
const char* statusGlyph(uint8_t st);
