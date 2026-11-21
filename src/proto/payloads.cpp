#include "payloads.h"
#include <string.h>

namespace bc {

std::string utf8Truncate(const std::string& s, size_t maxBytes) {
  if (s.size() <= maxBytes) return s;
  size_t n = maxBytes;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) --n;
  return s.substr(0, n);
}

static void tlv8(Bytes& b, uint8_t t, const void* v, size_t n) {
  if (n > 255) n = 255;
  putU8(b, t); putU8(b, (uint8_t)n); putBytes(b, v, n);
}

Bytes encodeAnnounce(const std::string& nick, const uint8_t noiseKey[32], const uint8_t signKey[32],
                     uint64_t caps) {
  Bytes b;
  std::string n = utf8Truncate(nick, 255);
  tlv8(b, 0x01, n.data(), n.size());
  tlv8(b, 0x02, noiseKey, 32);
  tlv8(b, 0x03, signKey, 32);
  if (caps) {
    uint8_t c[8]; size_t len = 0;
    uint64_t v = caps;
    while (v) { c[len++] = v & 0xFF; v >>= 8; }
    tlv8(b, 0x05, c, len);
  }
  return b;
}

bool decodeAnnounce(const Bytes& b, Announce& a) {
  size_t o = 0;
  bool hasNick = false;
  while (o + 2 <= b.size()) {
    uint8_t t = b[o], l = b[o + 1];
    o += 2;
    if (o + l > b.size()) return false;
    const uint8_t* v = b.data() + o;
    switch (t) {
      case 0x01: a.nickname.assign((const char*)v, l); hasNick = true; break;
      case 0x02: if (l == 32) { memcpy(a.noiseKey, v, 32); a.hasNoise = true; } break;
      case 0x03: if (l == 32) { memcpy(a.signKey, v, 32); a.hasSign = true; } break;
      case 0x04: if (l && l % 8 == 0) a.neighbors.assign(v, v + l); break;
      case 0x05: {
        uint64_t c = 0;
        for (int i = 0; i < l && i < 8; ++i) c |= uint64_t(v[i]) << (8 * i);
        a.capabilities = c;
      } break;
      default: break;
    }
    o += l;
  }
  return hasNick && a.hasNoise && a.hasSign;
}

Bytes encodePrivateMessage(const PrivateMessage& m) {
  Bytes b;
  std::string id = utf8Truncate(m.id, 255), c = utf8Truncate(m.content, 255);
  tlv8(b, 0x00, id.data(), id.size());
  tlv8(b, 0x01, c.data(), c.size());
  return b;
}

bool decodePrivateMessage(const uint8_t* p, size_t n, PrivateMessage& m) {
  size_t o = 0;
  bool hasId = false, hasContent = false;
  while (o + 2 <= n) {
    uint8_t t = p[o], l = p[o + 1];
    o += 2;
    if (o + l > n) return false;
    if (t == 0x00) { m.id.assign((const char*)p + o, l); hasId = true; }
    else if (t == 0x01) { m.content.assign((const char*)p + o, l); hasContent = true; }
    else return false;
    o += l;
  }
  return hasId && hasContent;
}

Bytes encodeFile(const FilePacket& f) {
  Bytes b;
  b.reserve(f.content.size() + f.name.size() + f.mime.size() + 24);
  if (!f.name.empty()) { putU8(b, 0x01); putU16(b, f.name.size()); putStr(b, f.name); }
  putU8(b, 0x02); putU16(b, 4); putU32(b, (uint32_t)f.content.size());
  if (!f.mime.empty()) { putU8(b, 0x03); putU16(b, f.mime.size()); putStr(b, f.mime); }
  putU8(b, 0x04); putU32(b, (uint32_t)f.content.size());
  putBytes(b, f.content.data(), f.content.size());
  return b;
}

bool decodeFile(const uint8_t* p, size_t n, FilePacket& f) {
  size_t o = 0;
  f.content.clear();
  while (o < n) {
    uint8_t t = p[o++];
    if (t == 0x04) {
      // content: u32 length (legacy: u16)
      size_t len = 0, hl = 0;
      if (o + 4 <= n && o + 4 + rdU32(p + o) <= n) { len = rdU32(p + o); hl = 4; }
      else if (o + 2 <= n && o + 2 + rdU16(p + o) <= n) { len = rdU16(p + o); hl = 2; }
      else return false;
      o += hl;
      f.content.insert(f.content.end(), p + o, p + o + len);
      o += len;
      continue;
    }
    if (o + 2 > n) return false;
    uint16_t l = rdU16(p + o); o += 2;
    if (o + l > n) return false;
    const uint8_t* v = p + o;
    if (t == 0x01) f.name.assign((const char*)v, l);
    else if (t == 0x02) { if (l == 4) f.size = rdU32(v); else if (l == 8) f.size = rdU64(v); }
    else if (t == 0x03) f.mime.assign((const char*)v, l);
    o += l;
  }
  if (f.size == 0) f.size = f.content.size();
  return !f.content.empty();
}

}  // namespace bc
