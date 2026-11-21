// Container parsing for voice notes (hardware independent).
#pragma once
#include <vector>
#include "../proto/bytes.h"

struct WavInfo {
  uint16_t format = 0, channels = 0, bits = 0;
  uint32_t rate = 0;
  const uint8_t* data = nullptr;
  size_t dataLen = 0;
};
bool parseWav(const Bytes& b, WavInfo& w);

struct M4aInfo {
  uint32_t sampleRate = 0;
  uint8_t channels = 0;
  uint8_t objectType = 0;
  uint32_t timescale = 0;
  uint64_t duration = 0;
  std::vector<uint32_t> sampleOffsets;
  std::vector<uint32_t> sampleSizes;
};
bool parseM4a(const Bytes& b, M4aInfo& m);
