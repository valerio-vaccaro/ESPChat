#include "noise.h"
#include "crypto_util.h"
#include <ChaChaPoly.h>
#include <string.h>

static const char* PROTOCOL_NAME = "Noise_XX_25519_ChaChaPoly_SHA256";  // exactly 32 bytes

// ---------------- CipherState ----------------
static void makeNonce(uint64_t n, uint8_t iv[12]) {
  memset(iv, 0, 4);
  for (int i = 0; i < 8; ++i) iv[4 + i] = (n >> (8 * i)) & 0xFF;
}

void CipherState::initKey(const uint8_t k[32]) { memcpy(k_, k, 32); keyed_ = true; n = 0; }
void CipherState::clear() { memset(k_, 0, 32); keyed_ = false; n = 0; }

void CipherState::seal(uint64_t nonce, const uint8_t* ad, size_t adLen, const uint8_t* pt, size_t len,
                       uint8_t* out) {
  ChaChaPoly c;
  uint8_t iv[12];
  makeNonce(nonce, iv);
  c.setKey(k_, 32);
  c.setIV(iv, 12);
  if (adLen) c.addAuthData(ad, adLen);
  if (len) c.encrypt(out, pt, len);
  c.computeTag(out + len, 16);
  c.clear();
}

bool CipherState::open(uint64_t nonce, const uint8_t* ad, size_t adLen, const uint8_t* ct, size_t len,
                       uint8_t* out) {
  if (len < 16) return false;
  ChaChaPoly c;
  uint8_t iv[12];
  makeNonce(nonce, iv);
  c.setKey(k_, 32);
  c.setIV(iv, 12);
  if (adLen) c.addAuthData(ad, adLen);
  if (len > 16) c.decrypt(out, ct, len - 16);
  bool ok = c.checkTag(ct + len - 16, 16);
  c.clear();
  return ok;
}

void CipherState::encryptWithAd(const uint8_t* ad, size_t adLen, const uint8_t* pt, size_t len, Bytes& out) {
  if (!keyed_) { out.assign(pt, pt + len); return; }
  out.resize(len + 16);
  seal(n++, ad, adLen, pt, len, out.data());
}

bool CipherState::decryptWithAd(const uint8_t* ad, size_t adLen, const uint8_t* ct, size_t len, Bytes& out) {
  if (!keyed_) { out.assign(ct, ct + len); return true; }
  if (len < 16) return false;
  out.resize(len - 16);
  if (!open(n, ad, adLen, ct, len, out.data())) return false;
  n++;
  return true;
}

// ---------------- HandshakeState ----------------
static void hkdf2(const uint8_t ck[32], const uint8_t* ikm, size_t ikmLen, uint8_t o1[32], uint8_t o2[32]) {
  uint8_t temp[32], buf[33];
  hmacSha256(ck, 32, ikm, ikmLen, temp);
  uint8_t one = 1;
  hmacSha256(temp, 32, &one, 1, o1);
  memcpy(buf, o1, 32);
  buf[32] = 2;
  hmacSha256(temp, 32, buf, 33, o2);
  memset(temp, 0, 32);
}

void HandshakeState::init(bool initiator, const uint8_t sPriv[32], const uint8_t sPub[32]) {
  initiator_ = initiator;
  step_ = 0;
  memcpy(h_, PROTOCOL_NAME, 32);
  memcpy(ck_, h_, 32);
  cs_.clear();
  mixHash(nullptr, 0);  // empty prologue
  memcpy(s_, sPriv, 32);
  memcpy(sPub_, sPub, 32);
  memset(rs_, 0, 32);
  memset(re_, 0, 32);
  randomBytes(e_, 32);
  x25519Clamp(e_);
  x25519Public(ePub_, e_);
}

void HandshakeState::mixHash(const uint8_t* d, size_t n) {
  Bytes b(h_, h_ + 32);
  if (n) b.insert(b.end(), d, d + n);
  sha256(b.data(), b.size(), h_);
}

void HandshakeState::mixKey(const uint8_t* ikm) {
  uint8_t k[32];
  hkdf2(ck_, ikm, 32, ck_, k);
  cs_.initKey(k);
  memset(k, 0, 32);
}

bool HandshakeState::dhMix(const uint8_t* priv, const uint8_t* pub) {
  uint8_t shared[32];
  if (!x25519(shared, priv, pub)) return false;
  mixKey(shared);
  memset(shared, 0, 32);
  return true;
}

void HandshakeState::encryptAndHash(const uint8_t* p, size_t n, Bytes& out) {
  cs_.encryptWithAd(h_, 32, p, n, out);
  mixHash(out.data(), out.size());
}

bool HandshakeState::decryptAndHash(const uint8_t* c, size_t n, Bytes& out) {
  uint8_t hPrev[32];
  memcpy(hPrev, h_, 32);
  if (!cs_.decryptWithAd(hPrev, 32, c, n, out)) return false;
  mixHash(c, n);
  return true;
}

bool HandshakeState::writeMessage(Bytes& out) {
  out.clear();
  Bytes tmp;
  if (initiator_ && step_ == 0) {          // -> e
    putBytes(out, ePub_, 32);
    mixHash(ePub_, 32);
    encryptAndHash(nullptr, 0, tmp);
    putBytes(out, tmp.data(), tmp.size());
  } else if (!initiator_ && step_ == 1) {  // <- e, ee, s, es
    putBytes(out, ePub_, 32);
    mixHash(ePub_, 32);
    if (!dhMix(e_, re_)) return false;
    encryptAndHash(sPub_, 32, tmp);
    putBytes(out, tmp.data(), tmp.size());
    if (!dhMix(s_, re_)) return false;
    encryptAndHash(nullptr, 0, tmp);
    putBytes(out, tmp.data(), tmp.size());
  } else if (initiator_ && step_ == 2) {   // -> s, se
    encryptAndHash(sPub_, 32, tmp);
    putBytes(out, tmp.data(), tmp.size());
    if (!dhMix(s_, re_)) return false;
    encryptAndHash(nullptr, 0, tmp);
    putBytes(out, tmp.data(), tmp.size());
  } else {
    return false;
  }
  step_++;
  return true;
}

bool HandshakeState::readMessage(const uint8_t* m, size_t n) {
  Bytes tmp;
  if (!initiator_ && step_ == 0) {         // -> e
    if (n < 32) return false;
    memcpy(re_, m, 32);
    mixHash(re_, 32);
    if (!decryptAndHash(m + 32, n - 32, tmp)) return false;
  } else if (initiator_ && step_ == 1) {   // <- e, ee, s, es
    if (n < 32 + 48 + 16) return false;
    memcpy(re_, m, 32);
    mixHash(re_, 32);
    if (!dhMix(e_, re_)) return false;
    if (!decryptAndHash(m + 32, 48, tmp) || tmp.size() != 32) return false;
    memcpy(rs_, tmp.data(), 32);
    if (!dhMix(e_, rs_)) return false;
    if (!decryptAndHash(m + 80, n - 80, tmp)) return false;
  } else if (!initiator_ && step_ == 2) {  // -> s, se
    if (n < 48 + 16) return false;
    if (!decryptAndHash(m, 48, tmp) || tmp.size() != 32) return false;
    memcpy(rs_, tmp.data(), 32);
    if (!dhMix(e_, rs_)) return false;
    if (!decryptAndHash(m + 48, n - 48, tmp)) return false;
  } else {
    return false;
  }
  step_++;
  return true;
}

void HandshakeState::split(CipherState& send, CipherState& recv) {
  uint8_t k1[32], k2[32];
  hkdf2(ck_, nullptr, 0, k1, k2);
  if (initiator_) { send.initKey(k1); recv.initKey(k2); }
  else { send.initKey(k2); recv.initKey(k1); }
  memset(k1, 0, 32); memset(k2, 0, 32);
  memset(e_, 0, 32);
}

// ---------------- NoiseSession ----------------
void NoiseSession::reset() {
  state = NONE;
  send_.clear();
  recv_.clear();
  sendNonce_ = 0;
  highest_ = 0;
  anyRecv_ = false;
  window_ = 0;
  retries = 0;
}

void NoiseSession::establish() {
  hs.split(send_, recv_);
  memcpy(remoteStatic, hs.remoteStatic(), 32);
  sendNonce_ = 0;
  highest_ = 0;
  anyRecv_ = false;
  window_ = 0;
  state = ESTABLISHED;
}

bool NoiseSession::encrypt(const uint8_t* pt, size_t len, Bytes& out) {
  if (state != ESTABLISHED || sendNonce_ == 0xFFFFFFFF) return false;
  uint32_t nonce = sendNonce_++;
  out.resize(4 + len + 16);
  out[0] = nonce >> 24; out[1] = nonce >> 16; out[2] = nonce >> 8; out[3] = nonce;
  send_.seal(nonce, nullptr, 0, pt, len, out.data() + 4);
  return true;
}

bool NoiseSession::decrypt(const uint8_t* in, size_t len, Bytes& out) {
  if (state != ESTABLISHED || len < 4 + 16) return false;
  uint64_t nonce = rdU32(in);
  // replay protection: sliding window of 64 nonces (bit 0 == highest_)
  if (anyRecv_ && nonce <= highest_) {
    uint64_t d = highest_ - nonce;
    if (d >= 64 || ((window_ >> d) & 1)) return false;
  }
  out.resize(len - 4 - 16);
  if (!recv_.open(nonce, nullptr, 0, in + 4, len - 4, out.data())) return false;
  if (!anyRecv_) { highest_ = nonce; window_ = 1; anyRecv_ = true; }
  else if (nonce > highest_) {
    uint64_t shift = nonce - highest_;
    window_ = shift >= 64 ? 1 : (window_ << shift) | 1;
    highest_ = nonce;
  } else {
    window_ |= 1ull << (highest_ - nonce);
  }
  return true;
}
