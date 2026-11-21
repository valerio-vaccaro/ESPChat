// Local identity: Noise static X25519 key + Ed25519 signing key, persisted in NVS.
#pragma once
#include <string>
#include "../proto/bytes.h"

struct Identity {
  uint8_t noisePriv[32];
  uint8_t noisePub[32];
  uint8_t signPriv[32];
  uint8_t signPub[32];
  uint8_t peerId[8];          // first 8 bytes of SHA256(noisePub)
  std::string peerIdHex;      // 16 lowercase hex chars
  std::string fingerprint;    // full SHA256(noisePub) hex

  void loadOrCreate();
  void regenerate();          // panic: new keys
  // Replace the identity with external keys. signPriv may be null (a new signing key is generated).
  void importKeys(const uint8_t noisePriv[32], const uint8_t* signPriv);
  void sign(const Bytes& msg, uint8_t sig[64]) const;
  static bool verify(const uint8_t pub[32], const Bytes& msg, const uint8_t sig[64]);
  static void peerIdFromNoiseKey(const uint8_t pub[32], uint8_t out[8]);

 private:
  void derive();
  void save();
};

extern Identity gIdentity;
