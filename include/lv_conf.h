// Minimal LVGL 8.3 configuration for ESPChat. Anything not set here falls
// back to the defaults in lv_conf_internal.h.
#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1          // matches LovyanGFX swap565 / esp32-camera RGB565 order

#define LV_MEM_CUSTOM 1             // use malloc (large blocks land in PSRAM)
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

#ifdef ESPCHAT_NATIVE              // PC screenshot renderer drives lv_tick_inc() itself
#define LV_TICK_CUSTOM 0
#else
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INV_BUF_SIZE 256         // many small invalidated cells (digital rain) without full-screen redraws
#define LV_INDEV_DEF_READ_PERIOD 20
#define LV_DPI_DEF 160

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_UNSCII_8 1
#define LV_FONT_UNSCII_16 1
#define LV_FONT_DEFAULT &lv_font_unscii_16

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_USE_SPAN 1
#define LV_USE_CANVAS 1
#define LV_USE_KEYBOARD 1
#define LV_USE_TABVIEW 1
#define LV_USE_LIST 1
#define LV_USE_MSGBOX 1
#define LV_USE_SPINBOX 1
#define LV_USE_QRCODE 1
#define LV_USE_LED 1
#define LV_USE_SNAPSHOT 0
#define LV_USE_PNG 0
#define LV_USE_SJPG 0

#endif
