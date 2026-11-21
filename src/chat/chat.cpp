// ChatService firmware implementation: bitchat protocol handling over the BLE mesh.
#include "chat.h"
#include <Arduino.h>
#include <time.h>
#include <string.h>
#include <algorithm>
#include <map>
#include "../../include/board_config.h"
#include "../crypto/crypto_util.h"
#include "../crypto/identity.h"
#include "../crypto/noise.h"
#include "../hal/settings.h"
#include "../hal/storage.h"
#include "../mesh/ble_mesh.h"
#include "../mesh/fragments.h"
#include "../proto/packet.h"
#include "../proto/payloads.h"

ChatService gChat;

using namespace bc;

namespace {

struct PendingPlain {
  Bytes plain;
  bool isFile;
  std::string msgId;
};

struct Peer {
  std::string id;
  uint8_t idb[8] = {0};
  std::string nick;
  uint8_t noiseKey[32] = {0};
  uint8_t signKey[32] = {0};
  bool hasKeys = false;
  uint32_t lastSeen = 0;
  uint64_t caps = 0;
  NoiseSession noise;
  std::vector<PendingPlain> pending;
};

constexpr uint32_t PEER_TIMEOUT_MS = 90000;
constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 8000;
constexpr size_t SEEN_SLOTS = 512;

uint64_t compileEpochMs() {
#ifdef BUILD_EPOCH
  // UTC build time injected by PlatformIO ($UNIX_TIME); phones drop packets whose timestamp is far
  // off, so this is only a starting point until a peer's announce syncs the clock.
  return (uint64_t)BUILD_EPOCH * 1000ull;
#else
  static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char m[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0};
  struct tm t = {};
  t.tm_mon = (strstr(months, m) - months) / 3;
  t.tm_mday = atoi(__DATE__ + 4);
  t.tm_year = atoi(__DATE__ + 7) - 1900;
  t.tm_hour = atoi(__TIME__);
  t.tm_min = atoi(__TIME__ + 3);
  t.tm_sec = atoi(__TIME__ + 6);
  setenv("TZ", "UTC0", 1);
  tzset();
  return (uint64_t)mktime(&t) * 1000ull;
#endif
}

bool isBroadcast(const Packet& p) {
  if (!p.hasRecipient) return true;
  for (int i = 0; i < 8; ++i)
    if (p.recipient[i] != 0xFF) return false;
  return true;
}

}  // namespace

struct ChatService::Impl {
  ChatService* svc;
  std::map<std::string, Peer> peers;
  FragmentAssembler frags;
  uint64_t seen[SEEN_SLOTS] = {0};
  size_t seenIdx = 0;
  uint64_t epochBase = 0;
  uint32_t nextAnnounce = 0;
  uint32_t lastHousekeeping = 0;
  uint32_t lastPeerSignature = 0;
  // diagnostics
  uint32_t nRaw = 0, nDup = 0, nDecodeFail = 0, nAnnounceOk = 0, nAnnounceUnverified = 0, nAnnounceBad = 0;
  uint32_t nPublic = 0, nHandshake = 0, nEncrypted = 0, nFragments = 0, nSigVariantUnpadded = 0;
  std::map<uint8_t, uint32_t> perType;
  uint32_t lastLog = 0;

  const uint8_t* me() const { return gIdentity.peerId; }

  // ---------------- helpers ----------------
  bool checkSeen(const Bytes& raw) {
    Bytes tmp(raw);
    tmp[2] = 0;  // ttl changes along the relay path
    uint8_t h[32];
    sha256(tmp.data(), tmp.size(), h);
    uint64_t k;
    memcpy(&k, h, 8);
    for (size_t i = 0; i < SEEN_SLOTS; ++i)
      if (seen[i] == k) return true;
    seen[seenIdx] = k;
    seenIdx = (seenIdx + 1) % SEEN_SLOTS;
    return false;
  }

  Peer& peerFor(const uint8_t id[8]) {
    std::string hex = toHex(id, 8);
    auto it = peers.find(hex);
    if (it == peers.end()) {
      Peer p;
      p.id = hex;
      memcpy(p.idb, id, 8);
      p.nick = "anon" + hex.substr(0, 4);
      it = peers.emplace(hex, std::move(p)).first;
    }
    return it->second;
  }

  Peer* findPeer(const std::string& id) {
    auto it = peers.find(id);
    return it == peers.end() ? nullptr : &it->second;
  }

  bool verify(const Packet& p, const uint8_t key[32]) {
    if (!p.hasSignature) return false;
    if (Identity::verify(key, signingBytes(p, true), p.signature)) return true;
    // tolerate implementations that sign the unpadded encoding
    if (Identity::verify(key, signingBytes(p, false), p.signature)) { nSigVariantUnpadded++; return true; }
    return false;
  }

  void syncClock(uint64_t ts) {
    int64_t diff = (int64_t)ts - (int64_t)svc->nowMs();
    if (!svc->synced_ || llabs(diff) > 5000) {
      svc->clockOffset_ += diff;
      svc->synced_ = true;
      log_i("clock synced from mesh (diff %lld ms)", diff);
    }
  }

  Packet basePacket(uint8_t type) {
    Packet p;
    p.version = 1;
    p.type = type;
    p.ttl = DEFAULT_TTL;
    p.timestamp = svc->nowMs();
    memcpy(p.sender, me(), 8);
    return p;
  }

  void signAndSend(Packet& p, uint16_t delay = 0) {
    p.hasSignature = false;
    Bytes sb = signingBytes(p);
    gIdentity.sign(sb, p.signature);
    p.hasSignature = true;
    mesh::send(encode(p, true), delay);
  }

  // ---------------- outgoing ----------------
  void sendAnnounce() {
    Packet p = basePacket(MT_ANNOUNCE);
    // <= 99 bytes: receivers re-encode the packet to check the signature and would compress a
    // larger payload, breaking the signature (same limit as the T-Deck client)
    p.payload = encodeAnnounce(bc::utf8Truncate(svc->nick_, 29), gIdentity.noisePub, gIdentity.signPub, 0);
    signAndSend(p);
  }

  void sendHandshake(Peer& peer, const Bytes& msg) {
    Packet p = basePacket(MT_NOISE_HANDSHAKE);
    p.hasRecipient = true;
    memcpy(p.recipient, peer.idb, 8);
    p.payload = msg;
    mesh::send(encode(p, true));
  }

  void startHandshake(Peer& peer) {
    NoiseSession& s = peer.noise;
    uint8_t retries = s.retries;
    s.reset();
    s.retries = retries;
    s.hs.init(true, gIdentity.noisePriv, gIdentity.noisePub);
    Bytes m1;
    if (!s.hs.writeMessage(m1)) { s.reset(); return; }
    s.state = NoiseSession::AWAIT_MSG2;
    s.startedMs = millis();
    sendHandshake(peer, m1);
    if (svc->onPeersChanged) svc->onPeersChanged();
  }

  bool sendEncrypted(Peer& peer, const Bytes& plain, bool isFile) {
    Bytes ct;
    if (!peer.noise.encrypt(plain.data(), plain.size(), ct)) return false;
    Packet p = basePacket(MT_NOISE_ENCRYPTED);
    p.version = isFile ? 2 : 1;
    p.hasRecipient = true;
    memcpy(p.recipient, peer.idb, 8);
    p.payload = std::move(ct);
    mesh::send(encode(p, true));
    return true;
  }

  // Returns true when sent immediately, false when queued behind a handshake.
  bool sendNoisePayload(Peer& peer, Bytes&& plain, bool isFile, const std::string& msgId) {
    if (peer.noise.state == NoiseSession::ESTABLISHED && sendEncrypted(peer, plain, isFile)) return true;
    peer.pending.push_back({std::move(plain), isFile, msgId});
    if (peer.noise.state == NoiseSession::NONE) {
      peer.noise.retries = 0;
      startHandshake(peer);
    }
    return false;
  }

  void flushPending(Peer& peer) {
    auto list = std::move(peer.pending);
    peer.pending.clear();
    for (auto& pp : list) {
      if (sendEncrypted(peer, pp.plain, pp.isFile)) {
        if (!pp.msgId.empty()) svc->setStatus(peer.id, pp.msgId, ST_SENT);
      } else {
        peer.pending.push_back(std::move(pp));
      }
    }
  }

  void failPending(Peer& peer) {
    for (auto& pp : peer.pending)
      if (!pp.msgId.empty()) svc->setStatus(peer.id, pp.msgId, ST_FAILED);
    peer.pending.clear();
  }

  void sendReceipt(Peer& peer, uint8_t type, const std::string& msgId) {
    Bytes plain;
    putU8(plain, type);
    putStr(plain, msgId);
    sendNoisePayload(peer, std::move(plain), false, "");
  }

  // ---------------- incoming ----------------
  void handleRaw(const Bytes& raw, bool reassembled, int expectedType) {
    if (raw.size() < 22) return;
    nRaw++;
    if (!reassembled && checkSeen(raw)) { nDup++; return; }
    Packet p;
    if (!decode(raw.data(), raw.size(), p)) {
      nDecodeFail++;
      log_d("[rx] cannot decode %u-byte frame: %s...", (unsigned)raw.size(),
            toHex(raw.data(), std::min<size_t>(raw.size(), 24)).c_str());
      return;
    }
    perType[p.type]++;
    if (expectedType >= 0 && p.type != expectedType) return;  // forged fragment set
    if (memcmp(p.sender, me(), 8) == 0) return;               // own echo

    bool forMe = p.hasRecipient && memcmp(p.recipient, me(), 8) == 0;
    bool broadcast = isBroadcast(p);

    if (!reassembled && svc->relay_ && p.ttl > 1 && !forMe) {
      Bytes r(raw);
      r[2] = p.ttl - 1;
      mesh::send(std::move(r), 15 + (esp_random() % 80));
    }

    switch (p.type) {
      case MT_ANNOUNCE: handleAnnounce(p); break;
      case MT_MESSAGE:
        if (broadcast) handlePublic(p);
        break;
      case MT_LEAVE: {
        Peer* peer = findPeer(toHex(p.sender, 8));
        if (peer && peer->hasKeys && verify(p, peer->signKey)) {
          peer->lastSeen = 0;
          peer->noise.reset();
          if (svc->onPeersChanged) svc->onPeersChanged();
        }
      } break;
      case MT_NOISE_HANDSHAKE:
        nHandshake++;
        if (forMe) handleHandshake(p);
        break;
      case MT_NOISE_ENCRYPTED:
        nEncrypted++;
        if (forMe) handleEncrypted(p);
        break;
      case MT_FRAGMENT:
        nFragments++;
        if (forMe || broadcast) {
          Bytes frame;
          uint8_t origType;
          if (frags.add(p, frame, origType)) handleRaw(frame, true, origType);
        }
        break;
      case MT_FILE_TRANSFER:
        if (broadcast) handleFile(p, "");
        else if (forMe) handleFile(p, toHex(p.sender, 8));
        break;
      default: break;  // requestSync, ping/pong, voiceFrame, ... are ignored
    }
  }

  void touch(Peer& peer) {
    bool wasOnline = peer.lastSeen && millis() - peer.lastSeen < PEER_TIMEOUT_MS;
    peer.lastSeen = millis();
    if (!wasOnline && svc->onPeersChanged) svc->onPeersChanged();
  }

  void handleAnnounce(const Packet& p) {
    Announce a;
    if (!decodeAnnounce(p.payload, a)) {
      nAnnounceBad++;
      log_w("[rx] announce from %s: bad TLV payload", toHex(p.sender, 8).c_str());
      return;
    }
    uint8_t derived[8];
    Identity::peerIdFromNoiseKey(a.noiseKey, derived);
    if (memcmp(derived, p.sender, 8) != 0) {
      nAnnounceBad++;
      log_w("[rx] announce from %s: sender id does not match its noise key", toHex(p.sender, 8).c_str());
      return;
    }
    // The peer id is bound to the noise key above. A signature mismatch most likely means a
    // different signing-bytes convention, so accept the peer but log it.
    bool sigOk = verify(p, a.signKey);
    if (sigOk) nAnnounceOk++;
    else {
      nAnnounceUnverified++;
      log_d("[rx] announce from %s (%s): signature %s - accepted unverified", toHex(p.sender, 8).c_str(),
            a.nickname.c_str(), p.hasSignature ? "MISMATCH" : "missing");
    }
    Peer& peer = peerFor(p.sender);
    bool isNew = !peer.hasKeys;
    if (peer.hasKeys && memcmp(peer.noiseKey, a.noiseKey, 32) != 0) peer.noise.reset();
    bool nickChanged = peer.nick != a.nickname;
    memcpy(peer.noiseKey, a.noiseKey, 32);
    memcpy(peer.signKey, a.signKey, 32);
    peer.hasKeys = true;
    peer.nick = a.nickname;
    peer.caps = a.capabilities;
    if (p.ttl == DEFAULT_TTL) syncClock(p.timestamp);
    if (isNew) {
      log_i("[peer] new node @%s %s (sig %s)", a.nickname.c_str(), peer.id.c_str(), sigOk ? "ok" : "unverified");
      // let the newcomer learn about us quickly
      uint32_t soon = millis() + 600 + (esp_random() % 800);
      if ((int32_t)(nextAnnounce - soon) > 0) nextAnnounce = soon;
    }
    touch(peer);
    if ((isNew || nickChanged) && svc->onPeersChanged) svc->onPeersChanged();
  }

  void handlePublic(const Packet& p) {
    std::string hex = toHex(p.sender, 8);
    Peer* peer = findPeer(hex);
    bool verified = false;
    if (peer && peer->hasKeys) {
      verified = verify(p, peer->signKey);
      if (!verified) log_d("[rx] public message from %s: signature not verified", hex.c_str());
      touch(*peer);
    }
    nPublic++;
    ChatMsg m;
    uint8_t h[32];
    sha256(p.payload.data(), p.payload.size(), h);
    m.id = hex + toHex(h, 4);
    m.senderId = hex;
    m.nick = peer ? peer->nick : "anon" + hex.substr(0, 4);
    m.text.assign((const char*)p.payload.data(), p.payload.size());
    m.ts = p.timestamp;
    m.verified = verified;
    svc->addMessage("", std::move(m));
  }

  void handleHandshake(const Packet& p) {
    Peer& peer = peerFor(p.sender);
    NoiseSession& s = peer.noise;
    const uint8_t* m = p.payload.data();
    size_t n = p.payload.size();
    if (n == 32) {  // msg1: -> e
      if (s.state == NoiseSession::AWAIT_MSG2 && gIdentity.peerIdHex < peer.id) return;  // we keep initiator
      s.reset();
      s.hs.init(false, gIdentity.noisePriv, gIdentity.noisePub);
      Bytes m2;
      if (!s.hs.readMessage(m, n) || !s.hs.writeMessage(m2)) { s.reset(); return; }
      s.state = NoiseSession::AWAIT_MSG3;
      s.startedMs = millis();
      sendHandshake(peer, m2);
    } else if (s.state == NoiseSession::AWAIT_MSG2) {
      Bytes m3;
      if (!s.hs.readMessage(m, n) || !s.hs.writeMessage(m3)) { s.reset(); return; }
      sendHandshake(peer, m3);
      finishHandshake(peer);
    } else if (s.state == NoiseSession::AWAIT_MSG3) {
      if (!s.hs.readMessage(m, n)) { s.reset(); return; }
      finishHandshake(peer);
    }
    touch(peer);
  }

  void finishHandshake(Peer& peer) {
    NoiseSession& s = peer.noise;
    s.establish();
    uint8_t derived[8];
    Identity::peerIdFromNoiseKey(s.remoteStatic, derived);
    if (memcmp(derived, peer.idb, 8) != 0) {
      log_w("noise static key does not match peer id %s", peer.id.c_str());
      s.reset();
      return;
    }
    if (!peer.hasKeys) memcpy(peer.noiseKey, s.remoteStatic, 32);
    log_i("noise session established with %s", peer.id.c_str());
    flushPending(peer);
    if (svc->onPeersChanged) svc->onPeersChanged();
  }

  void handleEncrypted(const Packet& p) {
    Peer& peer = peerFor(p.sender);
    touch(peer);
    if (peer.noise.state != NoiseSession::ESTABLISHED) {
      if (peer.noise.state == NoiseSession::NONE) { peer.noise.retries = 0; startHandshake(peer); }
      return;
    }
    Bytes plain;
    if (!peer.noise.decrypt(p.payload.data(), p.payload.size(), plain) || plain.empty()) {
      log_i("decrypt failed from %s, re-handshaking", peer.id.c_str());
      peer.noise.retries = 0;
      startHandshake(peer);
      return;
    }
    uint8_t t = plain[0];
    const uint8_t* body = plain.data() + 1;
    size_t bl = plain.size() - 1;
    switch (t) {
      case NP_PRIVATE_MESSAGE: {
        PrivateMessage pm;
        if (!decodePrivateMessage(body, bl, pm)) return;
        sendReceipt(peer, NP_DELIVERED, pm.id);
        ChatMsg m;
        m.id = pm.id;
        m.senderId = peer.id;
        m.nick = peer.nick;
        m.text = pm.content;
        m.ts = p.timestamp;
        svc->addMessage(peer.id, std::move(m));
      } break;
      case NP_DELIVERED:
        svc->setStatus(peer.id, std::string((const char*)body, bl), ST_DELIVERED);
        break;
      case NP_READ_RECEIPT:
        svc->setStatus(peer.id, std::string((const char*)body, bl), ST_READ);
        break;
      case NP_PRIVATE_FILE:
      case NP_PRIVATE_FILE_LEGACY: {
        FilePacket f;
        if (decodeFile(body, bl, f)) addFile(peer.id, peer, f, p.timestamp, true);
      } break;
      default: break;
    }
  }

  void handleFile(const Packet& p, const std::string& convId) {
    std::string hex = toHex(p.sender, 8);
    Peer* peer = findPeer(hex);
    if (!peer) {
      log_i("[rx] file from unknown node %s dropped", hex.c_str());
      return;
    }
    bool ok = peer->hasKeys && verify(p, peer->signKey);
    FilePacket f;
    if (!decodeFile(p.payload.data(), p.payload.size(), f)) return;
    addFile(convId, *peer, f, p.timestamp, ok);
  }

  void addFile(const std::string& convId, Peer& peer, FilePacket& f, uint64_t ts, bool verified) {
    auto media = std::make_shared<Media>();
    media->name = f.name;
    media->mime = f.mime;
    if (media->mime.empty()) {
      const std::string& n = f.name;
      auto ends = [&](const char* e) { size_t l = strlen(e); return n.size() >= l && n.compare(n.size() - l, l, e) == 0; };
      if (ends(".jpg") || ends(".jpeg")) media->mime = "image/jpeg";
      else if (ends(".png")) media->mime = "image/png";
      else if (ends(".m4a")) media->mime = "audio/mp4";
      else if (ends(".wav")) media->mime = "audio/wav";
      else media->mime = "application/octet-stream";
    }
    media->data = std::move(f.content);
    storageSaveMedia(*media);
    ChatMsg m;
    m.id = makeUUID();
    m.senderId = peer.id;
    m.nick = peer.nick;
    m.ts = ts;
    m.verified = verified;
    m.media = media;
    svc->addMessage(convId, std::move(m));
  }

  void housekeeping() {
    uint32_t now = millis();
    frags.prune(now);
    uint32_t sig = 0;
    for (auto& kv : peers) {
      Peer& peer = kv.second;
      NoiseSession& s = peer.noise;
      if ((s.state == NoiseSession::AWAIT_MSG2 || s.state == NoiseSession::AWAIT_MSG3) &&
          now - s.startedMs > HANDSHAKE_TIMEOUT_MS) {
        if (s.state == NoiseSession::AWAIT_MSG2 && s.retries < 3 && !peer.pending.empty()) {
          s.retries++;
          startHandshake(peer);
        } else {
          s.reset();
          failPending(peer);
          if (svc->onPeersChanged) svc->onPeersChanged();
        }
      }
      bool online = peer.lastSeen && now - peer.lastSeen < PEER_TIMEOUT_MS;
      sig = sig * 31 + (online ? 1 : 0) + s.state * 7;
    }
    if (sig != lastPeerSignature) {
      lastPeerSignature = sig;
      if (svc->onPeersChanged) svc->onPeersChanged();
    }
  }
};

// ---------------- public API ----------------
void ChatService::begin() {
  srand(esp_random());
  impl_ = new Impl();
  impl_->svc = this;
  impl_->epochBase = compileEpochMs();
  nick_ = gSettings.nickname.empty() ? "esp" + gIdentity.peerIdHex.substr(0, 4) : gSettings.nickname;
  relay_ = gSettings.relay;
  impl_->nextAnnounce = millis() + 1500;
  conv("");  // public channel always exists
  ChatMsg hello;
  hello.system = true;
  hello.ts = nowMs();
  hello.text = "mesh online. you are @" + nick_ + " [" + gIdentity.peerIdHex.substr(0, 8) + "]";
  addMessage("", std::move(hello));
}

void ChatService::loop() {
  Bytes raw;
  for (int i = 0; i < 24 && mesh::popRx(raw); ++i) impl_->handleRaw(raw, false, -1);
  uint32_t now = millis();
  if (mesh::takeLinkUpEvent()) impl_->nextAnnounce = now + 700;
  if ((int32_t)(now - impl_->nextAnnounce) >= 0) {
    impl_->sendAnnounce();
    impl_->nextAnnounce = now + 15000 + (esp_random() % 5000);
  }
  if (now - impl_->lastLog > 10000) {
    impl_->lastLog = now;
    log_d("%s", debugInfo().c_str());
  }
  if (now - impl_->lastHousekeeping > 1000) {
    impl_->lastHousekeeping = now;
    impl_->housekeeping();
  }
}

uint64_t ChatService::nowMs() const {
  uint64_t base = impl_ ? impl_->epochBase : 0;
  return base + clockOffset_ + millis();
}

int ChatService::links() const { return mesh::linkCount(); }
void ChatService::setCentralRole(bool on) { mesh::setCentralRole(on); }
std::string ChatService::myPeerId() const { return gIdentity.peerIdHex; }
std::string ChatService::myFingerprint() const { return gIdentity.fingerprint; }

void ChatService::setNickname(const std::string& n) {
  std::string t = bc::utf8Truncate(n, 32);
  if (t.empty() || t == nick_) return;
  nick_ = t;
  gSettings.nickname = t;
  gSettings.save();
  impl_->nextAnnounce = millis() + 200;
}

bool ChatService::sendText(const std::string& convId, const std::string& text, std::string* err) {
  if (text.empty()) return false;
  ChatMsg m;
  m.id = makeUUID();
  m.senderId = gIdentity.peerIdHex;
  m.nick = nick_;
  m.ts = nowMs();
  m.outgoing = true;

  if (convId.empty()) {
    // Signed payloads must stay below the 100-byte compression threshold (receivers re-encode,
    // compressing, to verify): long texts go out as several messages split on UTF-8 boundaries.
    std::string rest = bc::utf8Truncate(text, 2000);
    bool any = false;
    while (!rest.empty()) {
      std::string part = bc::utf8Truncate(rest, 96);
      rest = rest.substr(part.size());
      ChatMsg pm = m;
      pm.id = makeUUID();
      pm.text = part;
      Packet p = impl_->basePacket(MT_MESSAGE);
      p.timestamp = pm.ts;
      p.payload.assign(part.begin(), part.end());
      impl_->signAndSend(p);
      pm.status = mesh::linkCount() ? ST_SENT : ST_NONE;
      addMessage("", std::move(pm));
      any = true;
    }
    return any;
  }

  Peer* peer = impl_->findPeer(convId);
  if (!peer) { if (err) *err = "unknown peer"; return false; }
  m.text = bc::utf8Truncate(text, 255);
  PrivateMessage pm{m.id, m.text};
  Bytes plain;
  putU8(plain, NP_PRIVATE_MESSAGE);
  Bytes tlv = encodePrivateMessage(pm);
  plain.insert(plain.end(), tlv.begin(), tlv.end());
  std::string id = m.id;
  m.status = ST_SENDING;
  addMessage(convId, std::move(m));
  if (impl_->sendNoisePayload(*peer, std::move(plain), false, id)) setStatus(convId, id, ST_SENT);
  return true;
}

bool ChatService::sendMedia(const std::string& convId, const std::string& name, const std::string& mime,
                            Bytes&& data, std::string* err) {
  if (data.empty()) return false;
  auto media = std::make_shared<Media>();
  media->name = name;
  media->mime = mime;
  media->data = data;

  FilePacket f;
  f.name = name;
  f.mime = mime;
  f.size = data.size();
  f.content = std::move(data);
  Bytes filePayload = encodeFile(f);

  ChatMsg m;
  m.id = makeUUID();
  m.senderId = gIdentity.peerIdHex;
  m.nick = nick_;
  m.ts = nowMs();
  m.outgoing = true;
  m.media = media;

  if (convId.empty()) {
    if (filePayload.size() > MAX_PUBLIC_MEDIA_BYTES) { if (err) *err = "file too large"; return false; }
    Packet p = impl_->basePacket(MT_FILE_TRANSFER);
    p.version = 2;
    p.timestamp = m.ts;
    p.payload = std::move(filePayload);
    impl_->signAndSend(p);
    m.status = mesh::linkCount() ? ST_SENT : ST_NONE;
    addMessage("", std::move(m));
    return true;
  }

  Peer* peer = impl_->findPeer(convId);
  if (!peer) { if (err) *err = "unknown peer"; return false; }
  size_t frameEstimate = filePayload.size() + 1 + 20 + 16 + 16 + 64;
  if (mesh::estimateFragments(frameEstimate) > MAX_PRIVATE_MEDIA_FRAGMENTS) {
    if (err) *err = "too large for a private transfer";
    return false;
  }
  Bytes plain;
  plain.reserve(filePayload.size() + 1);
  putU8(plain, NP_PRIVATE_FILE);
  plain.insert(plain.end(), filePayload.begin(), filePayload.end());
  std::string id = m.id;
  m.status = ST_SENDING;
  addMessage(convId, std::move(m));
  if (impl_->sendNoisePayload(*peer, std::move(plain), true, id)) setStatus(convId, id, ST_SENT);
  return true;
}

void ChatService::markRead(const std::string& convId) {
  Conversation& c = conv(convId);
  c.unread = 0;
  if (convId.empty()) return;
  Peer* peer = impl_->findPeer(convId);
  if (!peer) return;
  for (auto& m : c.msgs) {
    if (!m.outgoing && !m.system && !m.readSent && !m.media && !m.id.empty()) {
      m.readSent = true;
      impl_->sendReceipt(*peer, NP_READ_RECEIPT, m.id);
    }
  }
}

void ChatService::startSession(const std::string& peerId) {
  Peer* peer = impl_->findPeer(peerId);
  if (peer && peer->noise.state == NoiseSession::NONE) {
    peer->noise.retries = 0;
    impl_->startHandshake(*peer);
  }
}

std::vector<PeerInfo> ChatService::peerList() {
  std::vector<PeerInfo> out;
  uint32_t now = millis();
  for (auto& kv : impl_->peers) {
    Peer& p = kv.second;
    if (!p.hasKeys && p.noise.state == NoiseSession::NONE && conv(p.id).msgs.empty()) continue;
    PeerInfo i;
    i.id = p.id;
    i.nick = p.nick;
    i.fingerprint = p.hasKeys ? [&] { uint8_t h[32]; sha256(p.noiseKey, 32, h); return toHex(h, 32); }() : "";
    i.online = p.lastSeen && now - p.lastSeen < PEER_TIMEOUT_MS;
    i.secure = p.noise.state == NoiseSession::ESTABLISHED;
    i.handshaking = p.noise.state == NoiseSession::AWAIT_MSG2 || p.noise.state == NoiseSession::AWAIT_MSG3;
    i.lastSeenMs = p.lastSeen ? now - p.lastSeen : UINT32_MAX;
    i.unread = conv(p.id).unread;
    out.push_back(i);
  }
  std::sort(out.begin(), out.end(), [](const PeerInfo& a, const PeerInfo& b) {
    if (a.online != b.online) return a.online;
    return a.nick < b.nick;
  });
  return out;
}

std::string ChatService::peerNick(const std::string& peerId) {
  Peer* p = impl_->findPeer(peerId);
  return p ? p->nick : peerId.substr(0, 8);
}

bool ChatService::peerSecure(const std::string& peerId) {
  Peer* p = impl_->findPeer(peerId);
  return p && p->noise.state == NoiseSession::ESTABLISHED;
}

size_t parseKeyHex(const std::string& in, uint8_t out[64]);

std::string ChatService::debugInfo() {
  const mesh::Stats& s = mesh::stats();
  Impl& d = *impl_;
  std::string types;
  for (auto& kv : d.perType) {
    char b[16];
    snprintf(b, sizeof(b), "%02x:%u ", kv.first, (unsigned)kv.second);
    types += b;
  }
  char buf[640];
  snprintf(buf, sizeof(buf),
           "[mesh] adv seen %u | out-links ok %u fail %u | in-links %u subscribed %u | now %d/%d (out/in)"
           " | advertising %s (restarts %u)\n"
           "[mesh] frames rx %u tx %u | dup %u | decode fail %u | maxframe %u\n"
           "[mesh] announce ok %u unverified %u bad %u | public %u | noise hs %u enc %u | frag %u | unpadded-sig %u\n"
           "[mesh] types %s",
           (unsigned)s.advSeen, (unsigned)s.connectOk, (unsigned)s.connectFail, (unsigned)s.centralsIn,
           (unsigned)s.subscribed, mesh::centralLinks(), mesh::peripheralLinks(), s.advertising ? "on" : "OFF",
           (unsigned)s.advRestarts, (unsigned)s.rxFrames,
           (unsigned)s.txFrames, (unsigned)d.nDup, (unsigned)d.nDecodeFail, (unsigned)mesh::maxFrame(),
           (unsigned)d.nAnnounceOk, (unsigned)d.nAnnounceUnverified, (unsigned)d.nAnnounceBad, (unsigned)d.nPublic,
           (unsigned)d.nHandshake, (unsigned)d.nEncrypted, (unsigned)d.nFragments, (unsigned)d.nSigVariantUnpadded,
           types.empty() ? "-" : types.c_str());
  return buf;
}

KeyInfo ChatService::keys() const {
  KeyInfo k;
  k.noisePub = toHex(gIdentity.noisePub, 32);
  k.signPub = toHex(gIdentity.signPub, 32);
  k.noisePriv = toHex(gIdentity.noisePriv, 32);
  k.signPriv = toHex(gIdentity.signPriv, 32);
  return k;
}

bool ChatService::importIdentity(const std::string& hex, std::string* err) {
  uint8_t k[64];
  size_t n = parseKeyHex(hex, k);
  if (!n) { if (err) *err = "need 64 or 128 hex chars"; return false; }
  uint8_t test[32];
  memcpy(test, k, 32);
  x25519Clamp(test);
  bool zero = true;
  for (int i = 0; i < 32; ++i) zero &= k[i] == 0;
  if (zero) { if (err) *err = "invalid key"; return false; }
  // drop sessions tied to the old identity, keep the message history
  for (auto& kv : impl_->peers) { kv.second.noise.reset(); kv.second.pending.clear(); }
  gIdentity.importKeys(k, n == 64 ? k + 32 : nullptr);
  impl_->nextAnnounce = millis() + 200;
  ChatMsg m;
  m.system = true;
  m.ts = nowMs();
  m.text = "identity imported. peer id is now " + gIdentity.peerIdHex.substr(0, 8);
  addMessage("", std::move(m));
  if (onPeersChanged) onPeersChanged();
  return true;
}

void ChatService::panic() {
  // Leave the mesh, wipe everything, new identity.
  Packet p = impl_->basePacket(MT_LEAVE);
  impl_->signAndSend(p);
  delay(200);
  impl_->peers.clear();
  convs_.clear();
  gIdentity.regenerate();
  gSettings.nickname = "";
  gSettings.save();
  nick_ = "esp" + gIdentity.peerIdHex.substr(0, 4);
  conv("");
  impl_->nextAnnounce = millis() + 300;
  if (onPeersChanged) onPeersChanged();
}
