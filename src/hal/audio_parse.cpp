#include "audio_parse.h"
#include "audio.h"
#include "../chat/chat.h"
#include <string.h>
#include <algorithm>

static uint16_t le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool parseWav(const Bytes& b, WavInfo& w) {
  if (b.size() < 44 || memcmp(b.data(), "RIFF", 4) || memcmp(b.data() + 8, "WAVE", 4)) return false;
  size_t o = 12;
  bool fmt = false;
  while (o + 8 <= b.size()) {
    const uint8_t* c = b.data() + o;
    uint32_t len = le32(c + 4);
    if (!memcmp(c, "fmt ", 4) && len >= 16 && o + 8 + 16 <= b.size()) {
      w.format = le16(c + 8);
      w.channels = le16(c + 10);
      w.rate = le32(c + 12);
      w.bits = le16(c + 22);
      fmt = true;
    } else if (!memcmp(c, "data", 4)) {
      w.data = c + 8;
      w.dataLen = std::min<size_t>(len, b.size() - o - 8);
      return fmt && w.channels && w.rate && (w.bits == 8 || w.bits == 16);
    }
    o += 8 + len + (len & 1);
  }
  return false;
}

Bytes makeWav8(const uint8_t* pcm, size_t n, uint32_t rate) {
  Bytes b;
  b.reserve(44 + n);
  auto le32p = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
  auto le16p = [&](uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); };
  putStr(b, "RIFF"); le32p(36 + n); putStr(b, "WAVE");
  putStr(b, "fmt "); le32p(16); le16p(1); le16p(1); le32p(rate); le32p(rate); le16p(1); le16p(8);
  putStr(b, "data"); le32p(n);
  putBytes(b, pcm, n);
  return b;
}

// ---------------- minimal MP4 demuxer ----------------
struct Box {
  const uint8_t* p;  // payload
  size_t n;
};

static bool findBox(const uint8_t* p, size_t n, const char* type, Box& out, size_t skip = 0) {
  size_t o = skip;
  while (o + 8 <= n) {
    uint64_t size = rdU32(p + o);
    size_t hdr = 8;
    if (size == 1 && o + 16 <= n) { size = rdU64(p + o + 8); hdr = 16; }
    else if (size == 0) size = n - o;
    if (size < hdr || o + size > n) return false;
    if (!memcmp(p + o + 4, type, 4)) { out = {p + o + hdr, (size_t)(size - hdr)}; return true; }
    o += size;
  }
  return false;
}

static bool path(Box in, std::initializer_list<const char*> types, Box& out) {
  for (const char* t : types)
    if (!findBox(in.p, in.n, t, in)) return false;
  out = in;
  return true;
}

static size_t descLen(const uint8_t*& p, const uint8_t* end) {
  size_t len = 0;
  for (int i = 0; i < 4 && p < end; ++i) {
    uint8_t b = *p++;
    len = (len << 7) | (b & 0x7F);
    if (!(b & 0x80)) break;
  }
  return len;
}

static bool parseEsds(Box esds, M4aInfo& m) {
  const uint8_t* p = esds.p + 4;  // version/flags
  const uint8_t* end = esds.p + esds.n;
  if (p >= end || *p++ != 0x03) return false;
  descLen(p, end);
  if (p + 3 > end) return false;
  uint8_t flags = p[2];
  p += 3;
  if (flags & 0x80) p += 2;
  if (flags & 0x40) { if (p >= end) return false; p += 1 + *p; }
  if (flags & 0x20) p += 2;
  if (p >= end || *p++ != 0x04) return false;
  descLen(p, end);
  p += 13;
  if (p >= end || *p++ != 0x05) return false;
  size_t l = descLen(p, end);
  if (l < 2 || p + 2 > end) return false;
  m.objectType = p[0] >> 3;
  static const uint32_t rates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                   22050, 16000, 12000, 11025, 8000,  7350};
  uint8_t fi = ((p[0] & 7) << 1) | (p[1] >> 7);
  if (fi < 13) m.sampleRate = rates[fi];
  m.channels = (p[1] >> 3) & 0x0F;
  return true;
}

bool parseM4a(const Bytes& b, M4aInfo& m) {
  Box root{b.data(), b.size()}, moov;
  if (!findBox(root.p, root.n, "moov", moov)) return false;
  // pick the first sound track
  size_t skip = 0;
  Box trak;
  while (true) {
    size_t o = skip;
    bool found = false;
    while (o + 8 <= moov.n) {
      uint32_t size = rdU32(moov.p + o);
      if (size < 8 || o + size > moov.n) return false;
      if (!memcmp(moov.p + o + 4, "trak", 4)) { trak = {moov.p + o + 8, size - 8u}; skip = o + size; found = true; break; }
      o += size;
    }
    if (!found) return false;
    Box hdlr;
    if (path(trak, {"mdia", "hdlr"}, hdlr) && hdlr.n >= 12 && !memcmp(hdlr.p + 8, "soun", 4)) break;
  }
  Box mdhd, stbl, stsd, stsz, stsc, stco;
  bool co64 = false;
  if (path(trak, {"mdia", "mdhd"}, mdhd) && mdhd.n >= 24) {
    if (mdhd.p[0] == 1 && mdhd.n >= 36) { m.timescale = rdU32(mdhd.p + 20); m.duration = rdU64(mdhd.p + 24); }
    else { m.timescale = rdU32(mdhd.p + 12); m.duration = rdU32(mdhd.p + 16); }
  }
  if (!path(trak, {"mdia", "minf", "stbl"}, stbl)) return false;
  if (!findBox(stbl.p, stbl.n, "stsd", stsd) || !findBox(stbl.p, stbl.n, "stsz", stsz) ||
      !findBox(stbl.p, stbl.n, "stsc", stsc))
    return false;
  if (!findBox(stbl.p, stbl.n, "stco", stco)) {
    if (!findBox(stbl.p, stbl.n, "co64", stco)) return false;
    co64 = true;
  }
  // stsd -> mp4a -> esds
  Box mp4a;
  if (stsd.n < 8 || !findBox(stsd.p, stsd.n, "mp4a", mp4a, 8)) return false;
  if (mp4a.n < 28) return false;
  uint16_t ver = rdU16(mp4a.p + 8);
  size_t entry = 28 + (ver == 1 ? 16 : ver == 2 ? 36 : 0);
  Box esds;
  if (entry > mp4a.n || !findBox(mp4a.p, mp4a.n, "esds", esds, entry) || !parseEsds(esds, m)) return false;
  if (!m.channels) m.channels = rdU16(mp4a.p + 16);
  if (!m.sampleRate) m.sampleRate = rdU32(mp4a.p + 24) >> 16;

  // sample table
  uint32_t fixed = rdU32(stsz.p + 4), count = rdU32(stsz.p + 8);
  if (count > 200000 || (!fixed && stsz.n < 12 + count * 4ull)) return false;
  m.sampleSizes.resize(count);
  for (uint32_t i = 0; i < count; ++i) m.sampleSizes[i] = fixed ? fixed : rdU32(stsz.p + 12 + i * 4);

  uint32_t chunks = rdU32(stco.p + 4);
  std::vector<uint64_t> chunkOff(chunks);
  for (uint32_t i = 0; i < chunks; ++i) {
    size_t at = 8 + i * (co64 ? 8 : 4);
    if (at + (co64 ? 8 : 4) > stco.n) return false;
    chunkOff[i] = co64 ? rdU64(stco.p + at) : rdU32(stco.p + at);
  }
  uint32_t entries = rdU32(stsc.p + 4);
  if (stsc.n < 8 + entries * 12ull || !entries) return false;
  m.sampleOffsets.resize(count);
  uint32_t s = 0;
  for (uint32_t e = 0; e < entries && s < count; ++e) {
    uint32_t first = rdU32(stsc.p + 8 + e * 12), per = rdU32(stsc.p + 12 + e * 12);
    uint32_t last = e + 1 < entries ? rdU32(stsc.p + 8 + (e + 1) * 12) : chunks + 1;
    for (uint32_t c = first; c < last && c <= chunks && s < count; ++c) {
      uint64_t off = chunkOff[c - 1];
      for (uint32_t k = 0; k < per && s < count; ++k) {
        if (off + m.sampleSizes[s] > b.size()) { m.sampleSizes.resize(s); m.sampleOffsets.resize(s); return s > 0; }
        m.sampleOffsets[s] = off;
        off += m.sampleSizes[s];
        ++s;
      }
    }
  }
  m.sampleOffsets.resize(s);
  m.sampleSizes.resize(s);
  return s > 0 && m.sampleRate;
}

float audioDurationSec(const Media& md) {
  WavInfo w;
  if (parseWav(md.data, w)) return (float)w.dataLen / (w.rate * w.channels * (w.bits / 8));
  M4aInfo m;
  if (parseM4a(md.data, m)) {
    if (m.timescale && m.duration) return (float)m.duration / m.timescale;
    return m.sampleSizes.size() * 1024.0f / m.sampleRate;
  }
  return 0;
}
