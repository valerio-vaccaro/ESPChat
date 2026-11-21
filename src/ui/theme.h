// Matrix-style dark/green theme.
#pragma once
#include <lvgl.h>

extern "C" const lv_font_t font_mono_13;
extern "C" const lv_font_t font_mono_16;
extern "C" const lv_font_t font_mono_20;

namespace theme {
// palette
constexpr uint32_t BG = 0x000000;
constexpr uint32_t PANEL = 0x020D04;
constexpr uint32_t PANEL2 = 0x041A08;
constexpr uint32_t GREEN = 0x00FF41;      // primary "matrix" green
constexpr uint32_t GREEN_MID = 0x00C032;
constexpr uint32_t GREEN_DIM = 0x008F11;
constexpr uint32_t GREEN_DARK = 0x003B00;
constexpr uint32_t GREEN_PALE = 0xB6FFC4;  // own nick / highlights
constexpr uint32_t ALERT = 0xFF3B3B;       // red: errors, unread LED, panic
// accents
constexpr uint32_t CYAN = 0x00E5FF;        // encryption, info, links
constexpr uint32_t AMBER = 0xFFB300;       // warnings, handshakes, voice
constexpr uint32_t MAGENTA = 0xFF4FD8;     // media / images
constexpr uint32_t VIOLET = 0xB388FF;      // identity / keys
constexpr uint32_t BLUE = 0x2F8CFF;        // unread private messages LED

// DejaVu Sans Mono (generated, Latin-1 + punctuation) with Montserrat fallback for symbols
extern lv_font_t mono;       // 16 px
extern lv_font_t monoSmall;  // 13 px
extern lv_font_t monoLarge;  // 20 px

void init(lv_disp_t* disp);
lv_color_t nickColor(const char* peerId);  // stable shade of green per peer
}  // namespace theme
