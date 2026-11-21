#pragma once
#include "bytes.h"
// Inflate a raw DEFLATE stream (Apple COMPRESSION_ZLIB); falls back to a zlib-wrapped stream.
bool inflateBytes(const uint8_t* src, size_t srcLen, size_t expectedLen, Bytes& out);
