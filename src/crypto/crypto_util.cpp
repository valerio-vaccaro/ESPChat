#include "crypto_util.h"
#include <SHA256.h>
#include <Curve25519.h>
#include <string.h>
#include <esp_random.h>

void sha256(const void* data, size_t len, uint8_t out[32]) {
  SHA256 h;
  h.update(data, len);
  h.finalize(out, 32);
}

void hmacSha256(const uint8_t* key, size_t keyLen, const void* data, size_t len, uint8_t out[32]) {
  SHA256 h;
  h.resetHMAC(key, keyLen);
  h.update(data, len);
  h.finalizeHMAC(key, keyLen, out, 32);
}

void randomBytes(uint8_t* out, size_t n) { esp_fill_random(out, n); }

void x25519Clamp(uint8_t k[32]) {
  k[0] &= 248;
  k[31] &= 127;
  k[31] |= 64;
}

void x25519Public(uint8_t pub[32], const uint8_t priv[32]) {
  uint8_t s[32];
  memcpy(s, priv, 32);
  x25519Clamp(s);
  Curve25519::eval(pub, s, nullptr);
  memset(s, 0, 32);
}

bool x25519(uint8_t out[32], const uint8_t priv[32], const uint8_t pub[32]) {
  uint8_t s[32];
  memcpy(s, priv, 32);
  x25519Clamp(s);
  Curve25519::eval(out, s, pub);
  memset(s, 0, 32);
  uint8_t acc = 0;
  for (int i = 0; i < 32; ++i) acc |= out[i];
  return acc != 0;  // reject all-zero shared secret (low-order point)
}
