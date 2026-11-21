#include "display.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "../../include/board_config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 panel_;
#if defined(BOARD_WT32_SC01_PLUS)
  lgfx::Bus_Parallel8 bus_;
#else
  lgfx::Bus_SPI bus_;
#endif
  lgfx::Light_PWM light_;
  lgfx::Touch_FT5x06 touch_;

 public:
  LGFX() {
    {
      auto cfg = bus_.config();
#if defined(BOARD_WT32_SC01_PLUS)
      cfg.freq_write = 20000000;
      cfg.pin_wr = LCD_WR;
      cfg.pin_rd = -1;
      cfg.pin_rs = LCD_RS;
      cfg.pin_d0 = LCD_D0; cfg.pin_d1 = LCD_D1; cfg.pin_d2 = LCD_D2; cfg.pin_d3 = LCD_D3;
      cfg.pin_d4 = LCD_D4; cfg.pin_d5 = LCD_D5; cfg.pin_d6 = LCD_D6; cfg.pin_d7 = LCD_D7;
#else
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = LCD_SCLK;
      cfg.pin_mosi = LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = LCD_DC;
#endif
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
#if defined(BOARD_WT32_SC01_PLUS)
      cfg.pin_cs = -1;
#else
      cfg.pin_cs = LCD_CS;
#endif
      cfg.pin_rst = LCD_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = PANEL_W;
      cfg.memory_height = PANEL_H;
      cfg.panel_width = PANEL_W;
      cfg.panel_height = PANEL_H;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = LCD_INVERT;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = LCD_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    {
      auto cfg = touch_.config();
      cfg.x_min = 0;
      cfg.x_max = PANEL_W - 1;
      cfg.y_min = 0;
      cfg.y_max = PANEL_H - 1;
      cfg.pin_int = TP_INT;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 1;
      cfg.i2c_addr = 0x38;
      cfg.pin_sda = TP_SDA;
      cfg.pin_scl = TP_SCL;
      cfg.freq = 400000;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};

static LGFX lcd;
static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;
static constexpr int BUF_LINES = 40;

static void flushCb(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* px) {
  uint32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  lcd.startWrite();
  lcd.setAddrWindow(a->x1, a->y1, w, h);
  lcd.writePixels((lgfx::swap565_t*)&px->full, w * h);
  lcd.endWrite();
  lv_disp_flush_ready(drv);
}

static void touchCb(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t x, y;
  if (lcd.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void displayBegin() {
  lcd.init();
  lcd.setRotation(LCD_ROTATION);  // touch coordinates follow the rotation
  lcd.setBrightness(0);
  lcd.fillScreen(TFT_BLACK);

  lv_init();
  size_t n = SCREEN_W * BUF_LINES;
  lv_color_t* b1 = (lv_color_t*)heap_caps_malloc(n * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_color_t* b2 = (lv_color_t*)heap_caps_malloc(n * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_disp_draw_buf_init(&drawBuf, b1, b2, n);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_W;
  dispDrv.ver_res = SCREEN_H;
  dispDrv.flush_cb = flushCb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchCb;
  lv_indev_drv_register(&indevDrv);
}

void displaySetBrightness(uint8_t level) { lcd.setBrightness(level < 8 ? 8 : level); }
