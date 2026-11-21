#pragma once
#include <stdint.h>
#include <stddef.h>

void sha256(const void* data, size_t len, uint8_t out[32]);
void hmacSha256(const uint8_t* key, size_t keyLen, const void* data, size_t len, uint8_t out[32]);
void randomBytes(uint8_t* out, size_t n);
// X25519 helpers (keys are clamped before use)
void x25519Clamp(uint8_t priv[32]);
void x25519Public(uint8_t pub[32], const uint8_t priv[32]);
bool x25519(uint8_t out[32], const uint8_t priv[32], const uint8_t pub[32]);
