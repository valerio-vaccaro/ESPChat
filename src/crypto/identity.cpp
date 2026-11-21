#include "identity.h"
#include "crypto_util.h"
#include <Ed25519.h>
#include <Preferences.h>
#include <string.h>

Identity gIdentity;

void Identity::peerIdFromNoiseKey(const uint8_t pub[32], uint8_t out[8]) {
  uint8_t h[32];
  sha256(pub, 32, h);
  memcpy(out, h, 8);
}

void Identity::derive() {
  x25519Clamp(noisePriv);
  x25519Public(noisePub, noisePriv);
  Ed25519::derivePublicKey(signPub, signPriv);
  uint8_t h[32];
  sha256(noisePub, 32, h);
  memcpy(peerId, h, 8);
  peerIdHex = toHex(peerId, 8);
  fingerprint = toHex(h, 32);
}

void Identity::save() {
  Preferences p;
  p.begin("bc_id", false);
  p.putBytes("noise", noisePriv, 32);
  p.putBytes("sign", signPriv, 32);
  p.end();
}

void Identity::loadOrCreate() {
  Preferences p;
  p.begin("bc_id", true);
  bool ok = p.getBytes("noise", noisePriv, 32) == 32 && p.getBytes("sign", signPriv, 32) == 32;
  p.end();
  if (!ok) { regenerate(); return; }
  derive();
}

void Identity::regenerate() {
  randomBytes(noisePriv, 32);
  randomBytes(signPriv, 32);  // Ed25519 seed from the hardware RNG
  derive();
  save();
}

void Identity::sign(const Bytes& msg, uint8_t sig[64]) const {
  Ed25519::sign(sig, signPriv, signPub, msg.data(), msg.size());
}

bool Identity::verify(const uint8_t pub[32], const Bytes& msg, const uint8_t sig[64]) {
  return Ed25519::verify(sig, pub, msg.data(), msg.size());
}

void Identity::importKeys(const uint8_t np[32], const uint8_t* sp) {
  memcpy(noisePriv, np, 32);
  if (sp) memcpy(signPriv, sp, 32);
  else randomBytes(signPriv, 32);
  derive();
  save();
}
