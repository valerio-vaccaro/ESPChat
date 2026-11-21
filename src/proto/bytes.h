// Small byte-buffer helpers (big-endian, as used by the bitchat wire format).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;

inline void putU8(Bytes& b, uint8_t v) { b.push_back(v); }
inline void putU16(Bytes& b, uint16_t v) { b.push_back(v >> 8); b.push_back(v & 0xFF); }
inline void putU32(Bytes& b, uint32_t v) { for (int i = 3; i >= 0; --i) b.push_back((v >> (8 * i)) & 0xFF); }
inline void putU64(Bytes& b, uint64_t v) { for (int i = 7; i >= 0; --i) b.push_back((v >> (8 * i)) & 0xFF); }
inline void putBytes(Bytes& b, const void* p, size_t n) {
  const uint8_t* s = (const uint8_t*)p; b.insert(b.end(), s, s + n);
}
inline void putStr(Bytes& b, const std::string& s) { putBytes(b, s.data(), s.size()); }

inline uint16_t rdU16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
inline uint32_t rdU32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline uint64_t rdU64(const uint8_t* p) {
  uint64_t v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | p[i]; return v;
}

std::string toHex(const uint8_t* p, size_t n);
bool fromHex(const std::string& s, uint8_t* out, size_t n);
