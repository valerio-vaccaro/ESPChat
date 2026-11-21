#include "matrix_rain.h"
#include <stdlib.h>
#include <string.h>
#include "theme.h"

static const char GLYPHS[] = "0123456789ABCDEFXZ:.=*+-<>|$#@%&";
static char glyph() { return GLYPHS[rand() % (sizeof(GLYPHS) - 1)]; }
static constexpr int CELL = 16;
static const lv_font_t* FONT = &lv_font_unscii_16;

// shade index -> colour (0 = background)
static const uint32_t SHADES[] = {theme::BG, 0x003B00, 0x00661A, 0x008F11, 0x00C032, 0x00FF41, 0xC8FFD4};
enum { S_EMPTY = 0, S_DARK = 1, S_DIM = 2, S_LOW = 3, S_MID = 4, S_GREEN = 5, S_HEAD = 6 };

MatrixRain::MatrixRain(lv_obj_t* parent, int w, int h, uint8_t opa) : w_(w), h_(h) {
  cols_ = w / CELL;
  rows_ = h / CELL;
  buf_ = (lv_color_t*)malloc(w * h * sizeof(lv_color_t));
  chars_ = (char*)calloc(cols_ * rows_, 1);
  shade_ = (uint8_t*)calloc(cols_ * rows_, 1);
  col_ = new Col[cols_];
  lv_color_t bg = lv_color_hex(theme::BG);
  if (buf_)
    for (int i = 0; i < w * h; ++i) buf_[i] = bg;

  memset(&dsc_, 0, sizeof(dsc_));
  dsc_.header.cf = LV_IMG_CF_TRUE_COLOR;
  dsc_.header.w = w;
  dsc_.header.h = h;
  dsc_.data_size = w * h * sizeof(lv_color_t);
  dsc_.data = (const uint8_t*)buf_;

  root_ = lv_img_create(parent);
  if (buf_) lv_img_set_src(root_, &dsc_);
  lv_obj_set_size(root_, w, h);
  lv_obj_set_style_bg_color(root_, bg, 0);
  lv_obj_set_style_bg_opa(root_, opa, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < cols_; ++i) resetCol(col_[i], true);
  timer_ = lv_timer_create(timerCb, 35, this);
  lv_obj_add_event_cb(root_, deleteCb, LV_EVENT_DELETE, this);
}

MatrixRain::~MatrixRain() {
  lv_timer_del(timer_);
  free(buf_);
  free(chars_);
  free(shade_);
  delete[] col_;
}

void MatrixRain::deleteCb(lv_event_t* e) { delete static_cast<MatrixRain*>(lv_event_get_user_data(e)); }

void MatrixRain::setRunning(bool r) {
  if (r) lv_timer_resume(timer_);
  else lv_timer_pause(timer_);
}

void MatrixRain::resetCol(Col& c, bool initial) {
  c.len = 6 + rand() % 16;
  c.every = 1 + rand() % 3;
  c.tick = rand() % c.every;
  c.head = initial ? (rand() % (rows_ + c.len)) - c.len : -(rand() % rows_);
}

// Render one glyph (any bpp) into the bitmap and invalidate just that cell.
void MatrixRain::drawCell(int col, int row, char ch, uint8_t shade) {
  if (!buf_ || col < 0 || col >= cols_ || row < 0 || row >= rows_) return;
  int idx = row * cols_ + col;
  chars_[idx] = ch;
  shade_[idx] = shade;
  int x0 = col * CELL, y0 = row * CELL;
  lv_color_t bg = lv_color_hex(theme::BG);
  for (int y = 0; y < CELL; ++y)
    for (int x = 0; x < CELL; ++x) buf_[(y0 + y) * w_ + x0 + x] = bg;
  if (shade != S_EMPTY && ch) {
    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(FONT, &g, (uint8_t)ch, 0)) {
      const uint8_t* bmp = lv_font_get_glyph_bitmap(FONT, (uint8_t)ch);
      lv_color_t fg = lv_color_hex(SHADES[shade]);
      int bpp = g.bpp, max = (1 << bpp) - 1;
      int gy0 = y0 + (FONT->line_height - FONT->base_line) - g.box_h - g.ofs_y;
      int gx0 = x0 + g.ofs_x;
      for (int y = 0; bmp && y < g.box_h; ++y)
        for (int x = 0; x < g.box_w; ++x) {
          int bit = (y * g.box_w + x) * bpp;
          int v = (bmp[bit >> 3] >> (8 - bpp - (bit & 7))) & max;
          int px = gx0 + x, py = gy0 + y;
          if (v * 2 >= max && px >= x0 && px < x0 + CELL && py >= y0 && py < y0 + CELL) buf_[py * w_ + px] = fg;
        }
    }
  }
  lv_area_t a;
  lv_obj_get_coords(root_, &a);
  a.x1 += x0;
  a.y1 += y0;
  a.x2 = a.x1 + CELL - 1;
  a.y2 = a.y1 + CELL - 1;
  lv_obj_invalidate_area(root_, &a);
}

void MatrixRain::shadeCell(int col, int row, uint8_t shade) {
  if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return;
  int idx = row * cols_ + col;
  if (shade_[idx] == shade) return;
  drawCell(col, row, chars_[idx] ? chars_[idx] : glyph(), shade);
}

void MatrixRain::step() {
  for (int i = 0; i < cols_; ++i) {
    Col& c = col_[i];
    if (++c.tick < c.every) continue;
    c.tick = 0;
    int h = c.head;
    // fade the trail behind the old head, then draw the new head
    shadeCell(i, h, S_GREEN);
    shadeCell(i, h - 3, S_MID);
    shadeCell(i, h - c.len / 2, S_LOW);
    shadeCell(i, h - c.len + 3, S_DIM);
    shadeCell(i, h - c.len + 1, S_DARK);
    shadeCell(i, h - c.len, S_EMPTY);
    c.head = h + 1;
    drawCell(i, c.head, glyph(), S_HEAD);
    if (c.head - c.len > rows_) resetCol(c, false);
  }
  // a few random glyph flickers inside visible trails
  for (int k = 0; k < 4; ++k) {
    int col = rand() % cols_, row = rand() % rows_, idx = row * cols_ + col;
    if (shade_[idx] > S_DARK && shade_[idx] < S_HEAD) drawCell(col, row, glyph(), shade_[idx]);
  }
}

void MatrixRain::timerCb(lv_timer_t* t) { static_cast<MatrixRain*>(t->user_data)->step(); }
