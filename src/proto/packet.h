// bitchat binary packet format (BitFoundation/BinaryProtocol.swift compatible).
//
// header v1 (14B): version u8 | type u8 | ttl u8 | timestamp u64 BE (ms) | flags u8 | payloadLen u16 BE
// header v2 (16B): same, payloadLen u32 BE
// then: senderID[8] | recipientID[8] (flag 0x01) | route (v2 + flag 0x08: count u8 + count*8)
//       | payload (flag 0x04: original-size prefix u16/u32 + raw DEFLATE) | signature[64] (flag 0x02)
//       | optional PKCS#7-style padding to 256/512/1024/2048
#pragma once
#include "bytes.h"

namespace bc {

enum MsgType : uint8_t {
  MT_ANNOUNCE = 0x01,
  MT_MESSAGE = 0x02,
  MT_LEAVE = 0x03,
  MT_NOISE_HANDSHAKE = 0x10,
  MT_NOISE_ENCRYPTED = 0x11,
  MT_FRAGMENT = 0x20,
  MT_REQUEST_SYNC = 0x21,
  MT_FILE_TRANSFER = 0x22,
  MT_PING = 0x26,
  MT_PONG = 0x27,
  MT_VOICE_FRAME = 0x29,
};

enum NoisePayloadType : uint8_t {
  NP_PRIVATE_MESSAGE = 0x01,
  NP_READ_RECEIPT = 0x02,
  NP_DELIVERED = 0x03,
  NP_PRIVATE_FILE_LEGACY = 0x09,
  NP_PRIVATE_FILE = 0x20,
};

enum Flags : uint8_t {
  F_HAS_RECIPIENT = 0x01,
  F_HAS_SIGNATURE = 0x02,
  F_COMPRESSED = 0x04,
  F_HAS_ROUTE = 0x08,
  F_RSR = 0x10,
};

constexpr uint8_t DEFAULT_TTL = 7;
constexpr size_t MAX_FRAMED_BYTES = 1179760;

struct Packet {
  uint8_t version = 1;
  uint8_t type = 0;
  uint8_t ttl = DEFAULT_TTL;
  uint64_t timestamp = 0;
  uint8_t flags = 0;            // flags as received (decode) - recomputed by encode
  uint8_t sender[8] = {0};
  bool hasRecipient = false;
  uint8_t recipient[8] = {0};
  Bytes route;                  // hop IDs, 8 bytes each (v2 only)
  Bytes payload;                // plain (decompressed) payload
  Bytes wirePayload;            // payload field exactly as on the wire (decode only)
  bool hasSignature = false;
  uint8_t signature[64] = {0};
};

bool decode(const uint8_t* data, size_t len, Packet& out);
// Encodes p (payload sent uncompressed). Padding applied when pad=true.
Bytes encode(const Packet& p, bool pad);
// Bytes covered by the Ed25519 signature: unsigned encoding with ttl=0, RSR cleared, padded.
Bytes signingBytes(const Packet& p, bool pad = true);

size_t optimalBlockSize(size_t dataSize);
void applyPadding(Bytes& b, size_t target);

}  // namespace bc
