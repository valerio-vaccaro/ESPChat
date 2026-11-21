// Noise_XX_25519_ChaChaPoly_SHA256 (empty prologue, empty handshake payloads)
// Transport frames: [nonce u32 BE][ciphertext][tag 16], AEAD nonce = 0x00000000 || LE64(nonce).
#pragma once
#include "../proto/bytes.h"

class CipherState {
 public:
  void initKey(const uint8_t k[32]);
  bool hasKey() const { return keyed_; }
  void clear();
  // Handshake-style (implicit nonce) encryption with associated data.
  void encryptWithAd(const uint8_t* ad, size_t adLen, const uint8_t* pt, size_t len, Bytes& out);
  bool decryptWithAd(const uint8_t* ad, size_t adLen, const uint8_t* ct, size_t len, Bytes& out);
  // Explicit-nonce AEAD primitives.
  void seal(uint64_t nonce, const uint8_t* ad, size_t adLen, const uint8_t* pt, size_t len, uint8_t* out);
  bool open(uint64_t nonce, const uint8_t* ad, size_t adLen, const uint8_t* ct, size_t len, uint8_t* out);
  uint64_t n = 0;

 private:
  uint8_t k_[32] = {0};
  bool keyed_ = false;
};

class HandshakeState {
 public:
  void init(bool initiator, const uint8_t sPriv[32], const uint8_t sPub[32]);
  bool writeMessage(Bytes& out);                 // next outgoing handshake message
  bool readMessage(const uint8_t* m, size_t n);  // next incoming handshake message
  bool complete() const { return step_ >= 3; }
  bool isInitiator() const { return initiator_; }
  int step() const { return step_; }
  void split(CipherState& send, CipherState& recv);
  const uint8_t* remoteStatic() const { return rs_; }

 private:
  void mixHash(const uint8_t* d, size_t n);
  void mixKey(const uint8_t* ikm);
  void encryptAndHash(const uint8_t* p, size_t n, Bytes& out);
  bool decryptAndHash(const uint8_t* c, size_t n, Bytes& out);
  bool dhMix(const uint8_t* priv, const uint8_t* pub);

  bool initiator_ = false;
  int step_ = 0;  // number of messages processed (0..3)
  uint8_t ck_[32], h_[32];
  CipherState cs_;
  uint8_t s_[32], sPub_[32], e_[32], ePub_[32], rs_[32], re_[32];
};

class NoiseSession {
 public:
  enum State { NONE, AWAIT_MSG2, AWAIT_MSG3, ESTABLISHED };
  State state = NONE;
  uint32_t startedMs = 0;
  uint8_t retries = 0;
  HandshakeState hs;
  uint8_t remoteStatic[32] = {0};

  void reset();
  void establish();
  bool encrypt(const uint8_t* pt, size_t len, Bytes& out);
  bool decrypt(const uint8_t* in, size_t len, Bytes& out);

 private:
  CipherState send_, recv_;
  uint32_t sendNonce_ = 0;
  uint64_t highest_ = 0;
  bool anyRecv_ = false;
  uint64_t window_ = 0;   // bitmap of the last 64 nonces below highest_
};
