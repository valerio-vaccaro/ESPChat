// "Digital rain" rendered into a bitmap on a 16x16 glyph grid. Each frame only the few cells that
// change are redrawn and invalidated, so the effect stays fluid on slow display buses.
#pragma once
#include <lvgl.h>
#include <stdint.h>

class MatrixRain {
 public:
  // The object lives as long as its root lv_obj: delete the root (or its parent) to free it.
  MatrixRain(lv_obj_t* parent, int w, int h, uint8_t opa = LV_OPA_COVER);
  lv_obj_t* root() const { return root_; }
  void step();  // advance one animation frame (also driven by its own timer)
  void setRunning(bool r);

 private:
  struct Col {
    int head;      // row of the bright leading glyph (may be negative = not yet visible)
    int len;       // trail length in cells
    int every;     // advance every N frames
    int tick;
  };
  static void timerCb(lv_timer_t* t);
  static void deleteCb(lv_event_t* e);
  ~MatrixRain();
  void resetCol(Col& c, bool initial);
  void drawCell(int col, int row, char ch, uint8_t shade);  // shade 0 = empty
  void shadeCell(int col, int row, uint8_t shade);

  lv_obj_t* root_;
  lv_timer_t* timer_;
  lv_img_dsc_t dsc_;
  lv_color_t* buf_;
  int w_, h_, cols_, rows_;
  Col* col_;
  char* chars_;     // glyph per cell
  uint8_t* shade_;  // shade per cell
};
