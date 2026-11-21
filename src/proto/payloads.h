// TLV payloads carried inside bitchat packets.
#pragma once
#include "bytes.h"

namespace bc {

// ---- Announce (MT_ANNOUNCE): [type u8][len u8][value] ----
struct Announce {
  std::string nickname;
  uint8_t noiseKey[32] = {0};
  uint8_t signKey[32] = {0};
  bool hasNoise = false, hasSign = false;
  Bytes neighbors;          // n * 8-byte peer IDs
  uint64_t capabilities = 0;
};
// capability bits (PeerCapabilities)
constexpr uint64_t CAP_PRIVATE_MEDIA = 1ull << 8;

Bytes encodeAnnounce(const std::string& nick, const uint8_t noiseKey[32], const uint8_t signKey[32],
                     uint64_t caps);
bool decodeAnnounce(const Bytes& b, Announce& a);

// ---- Private message (inside Noise, after the 0x01 type byte) ----
//   0x00 messageID (UTF-8, <=255) | 0x01 content (UTF-8, <=255)
struct PrivateMessage {
  std::string id;
  std::string content;
};
Bytes encodePrivateMessage(const PrivateMessage& m);
bool decodePrivateMessage(const uint8_t* p, size_t n, PrivateMessage& m);

// ---- File transfer (BitchatFilePacket) ----
//   0x01 fileName (u16 len) | 0x02 fileSize (u16 len = 4, u32 BE)
//   0x03 mimeType (u16 len) | 0x04 content (u32 len)
struct FilePacket {
  std::string name;
  std::string mime;
  uint64_t size = 0;
  Bytes content;
};
Bytes encodeFile(const FilePacket& f);
bool decodeFile(const uint8_t* p, size_t n, FilePacket& f);

// Truncate a UTF-8 string to at most maxBytes without splitting a code point.
std::string utf8Truncate(const std::string& s, size_t maxBytes);

}  // namespace bc
