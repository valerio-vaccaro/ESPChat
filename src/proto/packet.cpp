#include "packet.h"
#include "compress.h"
#include <string.h>

namespace bc {

size_t optimalBlockSize(size_t dataSize) {
  static const size_t blocks[] = {256, 512, 1024, 2048};
  size_t total = dataSize + 16;
  for (size_t b : blocks)
    if (total <= b) return b;
  return dataSize;
}

void applyPadding(Bytes& b, size_t target) {
  if (target <= b.size()) return;
  size_t n = target - b.size();
  if (n < 1 || n > 255) return;
  b.insert(b.end(), n, (uint8_t)n);
}

bool decode(const uint8_t* d, size_t len, Packet& p) {
  if (len < 22) return false;
  size_t o = 0;
  p.version = d[o++];
  if (p.version != 1 && p.version != 2) return false;
  size_t hdr = p.version == 1 ? 14 : 16;
  if (len < hdr + 8) return false;
  p.type = d[o++];
  p.ttl = d[o++];
  p.timestamp = rdU64(d + o); o += 8;
  p.flags = d[o++];
  uint32_t plen;
  if (p.version == 1) { plen = rdU16(d + o); o += 2; }
  else { plen = rdU32(d + o); o += 4; }
  if (plen > MAX_FRAMED_BYTES) return false;

  memcpy(p.sender, d + o, 8); o += 8;
  p.hasRecipient = p.flags & F_HAS_RECIPIENT;
  if (p.hasRecipient) {
    if (o + 8 > len) return false;
    memcpy(p.recipient, d + o, 8); o += 8;
  }
  p.route.clear();
  if (p.version == 2 && (p.flags & F_HAS_ROUTE)) {
    if (o + 1 > len) return false;
    uint8_t cnt = d[o++];
    if (o + cnt * 8u > len) return false;
    p.route.assign(d + o, d + o + cnt * 8u); o += cnt * 8u;
  }
  if (o + plen > len) return false;
  p.wirePayload.assign(d + o, d + o + plen);
  o += plen;

  p.hasSignature = p.flags & F_HAS_SIGNATURE;
  if (p.hasSignature) {
    if (o + 64 > len) return false;
    memcpy(p.signature, d + o, 64); o += 64;
  }

  if (p.flags & F_COMPRESSED) {
    size_t pre = p.version == 1 ? 2 : 4;
    if (p.wirePayload.size() < pre) return false;
    uint32_t orig = pre == 2 ? rdU16(p.wirePayload.data()) : rdU32(p.wirePayload.data());
    if (orig > MAX_FRAMED_BYTES) return false;
    if (!inflateBytes(p.wirePayload.data() + pre, p.wirePayload.size() - pre, orig, p.payload))
      return false;
  } else {
    p.payload = p.wirePayload;
  }
  return true;
}

static void writeHeaderAndBody(Bytes& out, const Packet& p, uint8_t ttl, uint8_t flags,
                               const Bytes& payloadField) {
  out.reserve(32 + payloadField.size() + 64 + p.route.size());
  putU8(out, p.version);
  putU8(out, p.type);
  putU8(out, ttl);
  putU64(out, p.timestamp);
  putU8(out, flags);
  if (p.version == 1) putU16(out, (uint16_t)payloadField.size());
  else putU32(out, (uint32_t)payloadField.size());
  putBytes(out, p.sender, 8);
  if (flags & F_HAS_RECIPIENT) putBytes(out, p.recipient, 8);
  if (flags & F_HAS_ROUTE) {
    putU8(out, (uint8_t)(p.route.size() / 8));
    putBytes(out, p.route.data(), p.route.size());
  }
  putBytes(out, payloadField.data(), payloadField.size());
}

Bytes encode(const Packet& p, bool pad) {
  uint8_t flags = 0;
  if (p.hasRecipient) flags |= F_HAS_RECIPIENT;
  if (p.hasSignature) flags |= F_HAS_SIGNATURE;
  if (p.version == 2 && !p.route.empty()) flags |= F_HAS_ROUTE;
  Bytes out;
  writeHeaderAndBody(out, p, p.ttl, flags, p.payload);
  if (p.hasSignature) putBytes(out, p.signature, 64);
  if (pad) applyPadding(out, optimalBlockSize(out.size()));
  return out;
}

Bytes signingBytes(const Packet& p, bool pad) {
  // When verifying a received packet, the payload field must be the exact
  // (possibly compressed) bytes that were signed.
  bool received = !p.wirePayload.empty();
  const Bytes& field = received ? p.wirePayload : p.payload;
  uint8_t flags = 0;
  if (p.hasRecipient) flags |= F_HAS_RECIPIENT;
  if (received && (p.flags & F_COMPRESSED)) flags |= F_COMPRESSED;
  if (p.version == 2 && !p.route.empty()) flags |= F_HAS_ROUTE;
  Bytes out;
  writeHeaderAndBody(out, p, 0, flags, field);
  if (pad) applyPadding(out, optimalBlockSize(out.size()));
  return out;
}

}  // namespace bc
