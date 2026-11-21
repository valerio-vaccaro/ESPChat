// Image helpers for the UI (decode to LVGL image descriptors, JPEG encode for doodles).
#pragma once
#include <lvgl.h>
#include <string>
#include "../proto/bytes.h"

struct UiImage {
  lv_img_dsc_t dsc;
  void* handle = nullptr;  // platform specific pixel owner
};

// Parse dimensions; kind = "jpeg" | "png". progressive JPEGs are reported via `progressive`.
bool imageProbe(const Bytes& d, int& w, int& h, std::string& kind, bool& progressive);
// Decode, scaled to fit maxW x maxH (never upscaled). nullptr on failure.
UiImage* imageDecode(const Bytes& d, int maxW, int maxH);
void imageFree(UiImage* img);
// Encode an RGB565 (LV_COLOR_16_SWAP byte order) buffer to JPEG.
bool jpegEncode(const uint8_t* rgb565, int w, int h, int quality, Bytes& out);
