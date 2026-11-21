#include "compress.h"
#include <stdlib.h>
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/miniz.h"
#else
#include "rom/miniz.h"
#endif

static bool tryInflate(const uint8_t* src, size_t srcLen, size_t expectedLen, Bytes& out, int flags) {
  tinfl_decompressor* d = (tinfl_decompressor*)malloc(sizeof(tinfl_decompressor));
  if (!d) return false;
  tinfl_init(d);
  out.assign(expectedLen, 0);
  size_t inBytes = srcLen, outBytes = expectedLen;
  tinfl_status st = tinfl_decompress(d, src, &inBytes, out.data(), out.data(), &outBytes,
                                     flags | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  free(d);
  return st == TINFL_STATUS_DONE && outBytes == expectedLen;
}

bool inflateBytes(const uint8_t* src, size_t srcLen, size_t expectedLen, Bytes& out) {
  if (expectedLen == 0) { out.clear(); return true; }
  if (tryInflate(src, srcLen, expectedLen, out, 0)) return true;
  return tryInflate(src, srcLen, expectedLen, out, TINFL_FLAG_PARSE_ZLIB_HEADER);
}
