// High-level bitchat chat service: peers, conversations, public + private messaging, media.
// Header is hardware independent (used by the UI and by the native screenshot renderer).
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "../proto/bytes.h"

struct Media {
  std::string name;
  std::string mime;
  Bytes data;
  bool isImage() const { return mime.rfind("image/", 0) == 0; }
  bool isAudio() const { return mime.rfind("audio/", 0) == 0; }
};

enum MsgStatus : uint8_t { ST_NONE, ST_SENDING, ST_SENT, ST_DELIVERED, ST_READ, ST_FAILED };

struct ChatMsg {
  std::string id;
  std::string senderId;     // 16 hex peer id
  std::string nick;
  std::string text;
  uint64_t ts = 0;          // epoch ms
  bool outgoing = false;
  bool system = false;
  bool verified = true;
  bool readSent = false;
  uint8_t status = ST_NONE;
  std::shared_ptr<Media> media;
};

struct Conversation {
  std::string peerId;       // "" == public mesh channel
  std::deque<ChatMsg> msgs;
  int unread = 0;
};

struct KeyInfo {
  std::string noisePub, signPub;    // hex, 64 chars
  std::string noisePriv, signPriv;  // hex, 64 chars (X25519 scalar, Ed25519 seed)
};

struct PeerInfo {
  std::string id;
  std::string nick;
  std::string fingerprint;
  bool online = false;
  bool secure = false;
  bool handshaking = false;
  uint32_t lastSeenMs = 0;  // ms ago
  int unread = 0;
};

class ChatService {
 public:
  void begin();
  void loop();

  bool sendText(const std::string& convId, const std::string& text, std::string* err = nullptr);
  bool sendMedia(const std::string& convId, const std::string& name, const std::string& mime, Bytes&& data,
                 std::string* err = nullptr);
  void markRead(const std::string& convId);
  void startSession(const std::string& peerId);

  Conversation& conv(const std::string& convId);
  std::vector<PeerInfo> peerList();
  std::string peerNick(const std::string& peerId);
  bool peerSecure(const std::string& peerId);
  int totalUnreadPrivate();

  const std::string& nickname() const { return nick_; }
  void setNickname(const std::string& n);
  bool relayEnabled() const { return relay_; }
  void setRelay(bool r) { relay_ = r; }
  void setCentralRole(bool on);
  std::string myPeerId() const;
  std::string myFingerprint() const;

  uint64_t nowMs() const;
  bool timeSynced() const { return synced_; }
  int links() const;
  void panic();
  KeyInfo keys() const;
  // Import an identity from hex: 64 chars (noise key only, new signing key) or 128 chars
  // (noise || ed25519 seed). Separators/whitespace are ignored. Returns false on bad input.
  bool importIdentity(const std::string& hex, std::string* err = nullptr);
  int totalUnread();
  std::string debugInfo();   // mesh diagnostics (CFG > MESH DEBUG)

  // UI hooks (called from the main loop task)
  std::function<void(const std::string& convId, const ChatMsg& msg)> onMessage;
  std::function<void(const std::string& convId, const std::string& msgId, uint8_t status)> onStatus;
  std::function<void()> onPeersChanged;

  struct Impl;

  // shared helpers (chat_common.cpp)
  void addMessage(const std::string& convId, ChatMsg&& m);
  void setStatus(const std::string& convId, const std::string& msgId, uint8_t st);

 private:
  friend struct Impl;
  Impl* impl_ = nullptr;
  std::string nick_;
  bool relay_ = true;
  bool synced_ = false;
  int64_t clockOffset_ = 0;
  std::map<std::string, Conversation> convs_;
};

extern ChatService gChat;

std::string makeUUID();
