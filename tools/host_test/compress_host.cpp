#include "../../src/proto/compress.h"
#include <zlib.h>
static bool inf(const uint8_t* s, size_t n, size_t exp, Bytes& out, int wbits) {
  z_stream z = {}; if (inflateInit2(&z, wbits) != Z_OK) return false;
  out.assign(exp, 0); z.next_in = (Bytef*)s; z.avail_in = n; z.next_out = out.data(); z.avail_out = exp;
  int r = inflate(&z, Z_FINISH); size_t got = exp - z.avail_out; inflateEnd(&z);
  return r == Z_STREAM_END && got == exp;
}
bool inflateBytes(const uint8_t* s, size_t n, size_t exp, Bytes& out) {
  return inf(s, n, exp, out, -15) || inf(s, n, exp, out, 15);
}
