#include "media.h"
#include <string.h>

bool imageProbe(const Bytes& d, int& w, int& h, std::string& kind, bool& progressive) {
  progressive = false;
  if (d.size() > 24 && !memcmp(d.data(), "\x89PNG", 4)) {
    kind = "png";
    w = rdU32(&d[16]);
    h = rdU32(&d[20]);
    return w > 0 && h > 0;
  }
  if (d.size() < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
  kind = "jpeg";
  size_t o = 2;
  while (o + 9 < d.size()) {
    if (d[o] != 0xFF) { ++o; continue; }
    uint8_t m = d[o + 1];
    if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7) || m == 0xFF) { o += (m == 0xFF) ? 1 : 2; continue; }
    uint16_t len = rdU16(&d[o + 2]);
    if ((m >= 0xC0 && m <= 0xC3) || (m >= 0xC5 && m <= 0xC7) || (m >= 0xC9 && m <= 0xCB) ||
        (m >= 0xCD && m <= 0xCF)) {
      h = rdU16(&d[o + 5]);
      w = rdU16(&d[o + 7]);
      progressive = (m == 0xC2 || m == 0xC6 || m == 0xCA || m == 0xCE);
      return w > 0 && h > 0;
    }
    o += 2 + len;
  }
  return false;
}
