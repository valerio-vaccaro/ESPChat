#include "bytes.h"

std::string toHex(const uint8_t* p, size_t n) {
  static const char* hx = "0123456789abcdef";
  std::string s; s.reserve(n * 2);
  for (size_t i = 0; i < n; ++i) { s += hx[p[i] >> 4]; s += hx[p[i] & 15]; }
  return s;
}

static int nib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool fromHex(const std::string& s, uint8_t* out, size_t n) {
  if (s.size() < n * 2) return false;
  for (size_t i = 0; i < n; ++i) {
    int a = nib(s[2 * i]), b = nib(s[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = (a << 4) | b;
  }
  return true;
}
