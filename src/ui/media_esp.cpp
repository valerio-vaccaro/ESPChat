// Firmware image backend: LovyanGFX (TJpgDec / pngle) decode, esp32-camera JPEG encoder.
#include "media.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "img_converters.h"
#include <algorithm>

UiImage* imageDecode(const Bytes& d, int maxW, int maxH) {
  int w, h;
  std::string kind;
  bool progressive;
  if (!imageProbe(d, w, h, kind, progressive) || progressive) return nullptr;
  float s = std::min(1.0f, std::min((float)maxW / w, (float)maxH / h));
  int ow = std::max(1, (int)(w * s)), oh = std::max(1, (int)(h * s));
  auto* spr = new LGFX_Sprite();
  spr->setColorDepth(16);
  spr->setPsram(true);
  if (!spr->createSprite(ow, oh)) { delete spr; return nullptr; }
  spr->fillScreen(TFT_BLACK);
  bool ok = kind == "jpeg" ? spr->drawJpg(d.data(), d.size(), 0, 0, ow, oh, 0, 0, s, s)
                           : spr->drawPng(d.data(), d.size(), 0, 0, ow, oh, 0, 0, s, s);
  if (!ok) { delete spr; return nullptr; }
  auto* img = new UiImage();
  img->handle = spr;
  memset(&img->dsc, 0, sizeof(img->dsc));
  img->dsc.header.always_zero = 0;
  img->dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  img->dsc.header.w = ow;
  img->dsc.header.h = oh;
  img->dsc.data_size = ow * oh * 2;
  img->dsc.data = (const uint8_t*)spr->getBuffer();
  return img;
}

void imageFree(UiImage* img) {
  if (!img) return;
  delete static_cast<LGFX_Sprite*>(img->handle);
  delete img;
}

bool jpegEncode(const uint8_t* rgb565, int w, int h, int quality, Bytes& out) {
  uint8_t* jpg = nullptr;
  size_t len = 0;
  if (!fmt2jpg((uint8_t*)rgb565, w * h * 2, w, h, PIXFORMAT_RGB565, quality, &jpg, &len)) return false;
  out.assign(jpg, jpg + len);
  free(jpg);
  return true;
}
