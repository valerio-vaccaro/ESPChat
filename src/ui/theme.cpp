#include "theme.h"
#include <string.h>

namespace theme {

lv_font_t mono;
lv_font_t monoSmall;
lv_font_t monoLarge;

static lv_theme_t matrixTheme;
static lv_style_t sScreen, sObj, sBtn, sBtnPr, sBtnChk, sTa, sTaFocus, sCursor, sScroll, sKbBg, sKbBtn,
    sKbBtnPr, sIndicator, sKnob, sSwitchKnob, sTrack, sList, sLabel;

static void applyCb(lv_theme_t*, lv_obj_t* o) {
  if (lv_obj_get_parent(o) == nullptr) {
    lv_obj_add_style(o, &sScreen, 0);
    lv_obj_add_style(o, &sScroll, LV_PART_SCROLLBAR);
    return;
  }
  if (lv_obj_check_type(o, &lv_obj_class)) {
    lv_obj_add_style(o, &sObj, 0);
    lv_obj_add_style(o, &sScroll, LV_PART_SCROLLBAR);
  } else if (lv_obj_check_type(o, &lv_btn_class)) {
    lv_obj_add_style(o, &sBtn, 0);
    lv_obj_add_style(o, &sBtnPr, LV_STATE_PRESSED);
    lv_obj_add_style(o, &sBtnChk, LV_STATE_CHECKED);
  } else if (lv_obj_check_type(o, &lv_label_class)) {
    lv_obj_add_style(o, &sLabel, 0);
  } else if (lv_obj_check_type(o, &lv_textarea_class)) {
    lv_obj_add_style(o, &sTa, 0);
    lv_obj_add_style(o, &sTaFocus, LV_STATE_FOCUSED);
    lv_obj_add_style(o, &sCursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_style(o, &sScroll, LV_PART_SCROLLBAR);
  } else if (lv_obj_check_type(o, &lv_keyboard_class)) {
    lv_obj_add_style(o, &sKbBg, 0);
    lv_obj_add_style(o, &sKbBtn, LV_PART_ITEMS);
    lv_obj_add_style(o, &sKbBtnPr, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_style(o, &sBtnChk, LV_PART_ITEMS | LV_STATE_CHECKED);
  } else if (lv_obj_check_type(o, &lv_btnmatrix_class)) {
    lv_obj_add_style(o, &sKbBg, 0);
    lv_obj_add_style(o, &sKbBtn, LV_PART_ITEMS);
    lv_obj_add_style(o, &sKbBtnPr, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_style(o, &sBtnChk, LV_PART_ITEMS | LV_STATE_CHECKED);
  } else if (lv_obj_check_type(o, &lv_slider_class) || lv_obj_check_type(o, &lv_switch_class)) {
    lv_obj_add_style(o, &sTrack, 0);
    lv_obj_add_style(o, &sIndicator, LV_PART_INDICATOR);
    lv_obj_add_style(o, &sIndicator, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(o, &sKnob, LV_PART_KNOB);
    if (lv_obj_check_type(o, &lv_switch_class)) lv_obj_add_style(o, &sSwitchKnob, LV_PART_KNOB);
  } else if (lv_obj_check_type(o, &lv_tabview_class)) {
    lv_obj_add_style(o, &sScreen, 0);
  } else if (lv_obj_check_type(o, &lv_dropdown_class)) {
    lv_obj_add_style(o, &sTa, 0);
  } else if (lv_obj_check_type(o, &lv_dropdownlist_class)) {
    lv_obj_add_style(o, &sList, 0);
    lv_obj_add_style(o, &sBtnPr, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_add_style(o, &sBtnPr, LV_PART_SELECTED | LV_STATE_PRESSED);
  } else if (lv_obj_check_type(o, &lv_msgbox_class)) {
    lv_obj_add_style(o, &sList, 0);
  }
}

lv_color_t nickColor(const char* id) {
  static const uint32_t shades[] = {0x00FF41, 0x00E5FF, 0xFFB300, 0xFF6EC7, 0xB388FF, 0x9CFF3D, 0xFF8A3D, 0x64B5F6};
  uint32_t h = 2166136261u;
  for (const char* p = id; p && *p; ++p) h = (h ^ (uint8_t)*p) * 16777619u;
  return lv_color_hex(shades[h % 8]);
}

void init(lv_disp_t* disp) {
  mono = font_mono_16;
  monoSmall = font_mono_13;
  monoLarge = font_mono_20;

  lv_style_init(&sScreen);
  lv_style_set_bg_color(&sScreen, lv_color_hex(BG));
  lv_style_set_bg_opa(&sScreen, LV_OPA_COVER);
  lv_style_set_text_color(&sScreen, lv_color_hex(GREEN));
  lv_style_set_text_font(&sScreen, &mono);

  lv_style_init(&sObj);
  lv_style_set_bg_color(&sObj, lv_color_hex(BG));
  lv_style_set_bg_opa(&sObj, LV_OPA_TRANSP);
  lv_style_set_border_width(&sObj, 0);
  lv_style_set_radius(&sObj, 0);
  lv_style_set_text_color(&sObj, lv_color_hex(GREEN));
  lv_style_set_pad_all(&sObj, 0);

  lv_style_init(&sLabel);
  lv_style_set_text_color(&sLabel, lv_color_hex(GREEN));

  lv_style_init(&sScroll);
  lv_style_set_bg_color(&sScroll, lv_color_hex(GREEN_DIM));
  lv_style_set_bg_opa(&sScroll, LV_OPA_70);
  lv_style_set_width(&sScroll, 3);
  lv_style_set_radius(&sScroll, 0);

  lv_style_init(&sBtn);
  lv_style_set_bg_color(&sBtn, lv_color_hex(PANEL2));
  lv_style_set_bg_opa(&sBtn, LV_OPA_COVER);
  lv_style_set_border_color(&sBtn, lv_color_hex(GREEN_MID));
  lv_style_set_border_width(&sBtn, 1);
  lv_style_set_radius(&sBtn, 2);
  lv_style_set_shadow_width(&sBtn, 0);
  lv_style_set_text_color(&sBtn, lv_color_hex(GREEN));
  lv_style_set_pad_all(&sBtn, 6);

  lv_style_init(&sBtnPr);
  lv_style_set_bg_color(&sBtnPr, lv_color_hex(GREEN));
  lv_style_set_bg_opa(&sBtnPr, LV_OPA_COVER);
  lv_style_set_text_color(&sBtnPr, lv_color_hex(BG));

  lv_style_init(&sBtnChk);
  lv_style_set_bg_color(&sBtnChk, lv_color_hex(GREEN_DARK));
  lv_style_set_text_color(&sBtnChk, lv_color_hex(GREEN_PALE));
  lv_style_set_border_color(&sBtnChk, lv_color_hex(GREEN));

  lv_style_init(&sTa);
  lv_style_set_bg_color(&sTa, lv_color_hex(BG));
  lv_style_set_bg_opa(&sTa, LV_OPA_COVER);
  lv_style_set_border_color(&sTa, lv_color_hex(GREEN_DIM));
  lv_style_set_border_width(&sTa, 1);
  lv_style_set_radius(&sTa, 0);
  lv_style_set_text_color(&sTa, lv_color_hex(GREEN));
  lv_style_set_pad_all(&sTa, 6);

  lv_style_init(&sTaFocus);
  lv_style_set_border_color(&sTaFocus, lv_color_hex(GREEN));
  lv_style_set_shadow_color(&sTaFocus, lv_color_hex(GREEN));
  lv_style_set_shadow_width(&sTaFocus, 8);
  lv_style_set_shadow_opa(&sTaFocus, LV_OPA_40);

  lv_style_init(&sCursor);
  lv_style_set_bg_color(&sCursor, lv_color_hex(GREEN));
  lv_style_set_bg_opa(&sCursor, LV_OPA_COVER);
  lv_style_set_border_width(&sCursor, 0);

  lv_style_init(&sKbBg);
  lv_style_set_bg_color(&sKbBg, lv_color_hex(PANEL));
  lv_style_set_bg_opa(&sKbBg, LV_OPA_COVER);
  lv_style_set_border_color(&sKbBg, lv_color_hex(GREEN_DIM));
  lv_style_set_border_width(&sKbBg, 1);
  lv_style_set_border_side(&sKbBg, LV_BORDER_SIDE_TOP);
  lv_style_set_pad_all(&sKbBg, 3);
  lv_style_set_pad_gap(&sKbBg, 3);
  lv_style_set_radius(&sKbBg, 0);

  lv_style_init(&sKbBtn);
  lv_style_set_bg_color(&sKbBtn, lv_color_hex(PANEL2));
  lv_style_set_bg_opa(&sKbBtn, LV_OPA_COVER);
  lv_style_set_border_color(&sKbBtn, lv_color_hex(GREEN_DARK));
  lv_style_set_border_width(&sKbBtn, 1);
  lv_style_set_radius(&sKbBtn, 2);
  lv_style_set_text_color(&sKbBtn, lv_color_hex(GREEN));
  lv_style_set_text_font(&sKbBtn, &lv_font_montserrat_16);
  lv_style_set_shadow_width(&sKbBtn, 0);

  lv_style_init(&sKbBtnPr);
  lv_style_set_bg_color(&sKbBtnPr, lv_color_hex(GREEN));
  lv_style_set_text_color(&sKbBtnPr, lv_color_hex(BG));

  lv_style_init(&sTrack);
  lv_style_set_bg_color(&sTrack, lv_color_hex(GREEN_DARK));
  lv_style_set_bg_opa(&sTrack, LV_OPA_COVER);
  lv_style_set_radius(&sTrack, 2);
  lv_style_set_border_width(&sTrack, 0);

  lv_style_init(&sIndicator);
  lv_style_set_bg_color(&sIndicator, lv_color_hex(GREEN));
  lv_style_set_bg_opa(&sIndicator, LV_OPA_COVER);
  lv_style_set_radius(&sIndicator, 2);

  lv_style_init(&sKnob);
  lv_style_set_bg_color(&sKnob, lv_color_hex(GREEN_PALE));
  lv_style_set_bg_opa(&sKnob, LV_OPA_COVER);
  lv_style_set_radius(&sKnob, 2);
  lv_style_set_pad_all(&sKnob, 4);

  lv_style_init(&sSwitchKnob);
  lv_style_set_pad_all(&sSwitchKnob, -3);

  lv_style_init(&sList);
  lv_style_set_bg_color(&sList, lv_color_hex(PANEL));
  lv_style_set_bg_opa(&sList, LV_OPA_COVER);
  lv_style_set_border_color(&sList, lv_color_hex(GREEN));
  lv_style_set_border_width(&sList, 1);
  lv_style_set_radius(&sList, 0);
  lv_style_set_text_color(&sList, lv_color_hex(GREEN));
  lv_style_set_text_font(&sList, &mono);

  lv_theme_t* base = lv_theme_default_init(disp, lv_color_hex(GREEN), lv_color_hex(GREEN_DIM), true, &mono);
  matrixTheme = *base;
  lv_theme_set_parent(&matrixTheme, base);
  lv_theme_set_apply_cb(&matrixTheme, applyCb);
  lv_disp_set_theme(disp, &matrixTheme);
  // layers created before the theme existed
  lv_obj_set_style_text_font(lv_layer_top(), &mono, 0);
  lv_obj_set_style_text_color(lv_layer_top(), lv_color_hex(GREEN), 0);
  lv_obj_set_style_text_font(lv_layer_sys(), &mono, 0);
}

}  // namespace theme
