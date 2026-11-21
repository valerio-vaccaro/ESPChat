#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
static inline void esp_fill_random(void* buf, size_t n) {
  FILE* f = fopen("/dev/urandom", "rb"); fread(buf, 1, n, f); fclose(f);
}
static inline uint32_t esp_random() { uint32_t v; esp_fill_random(&v, 4); return v; }
