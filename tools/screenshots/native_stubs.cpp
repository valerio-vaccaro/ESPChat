// PC stand-ins for the hardware layer + a demo ChatService, used only to render screenshots.
#include <string.h>
#include <chrono>
#include "../../src/chat/chat.h"
#include "../../src/hal/audio.h"
#include "../../src/hal/display.h"
#include "../../src/hal/settings.h"
#include "../../src/hal/storage.h"
#include "../../src/platform.h"
#include "../../src/ui/media.h"

uint32_t gFakeMillis = 0;
uint32_t sysMillis() { return gFakeMillis; }
uint32_t sysFreeHeap() { return 187 * 1024; }
uint32_t sysFreePsram() { return 1630 * 1024; }

Settings gSettings;
void Settings::load() {}
void Settings::save() {}
void displaySetBrightness(uint8_t) {}

bool storageBegin() { return true; }
bool storageAvailable() { return true; }
std::vector<std::string> storageListImages() {
  return {"/bitchat/send/white_rabbit.jpg", "/bitchat/send/zion_map.png", "/bitchat/send/red_pill.jpg", "/IMG_0042.jpg"};
}
bool storageRead(const std::string&, Bytes&, size_t) { return false; }
void storageSaveMedia(const Media&) {}
bool storageWrite(const std::string&, const std::string&) { return true; }

bool gRecording = false;
void audioBegin() {}
bool audioCanPlay() { return true; }
bool audioCanRecord() { return true; }
bool audioPlay(std::shared_ptr<Media>) { return true; }
void audioStop() {}
bool audioPlaying() { return false; }
void audioSetVolume(uint8_t) {}
bool audioRecordStart() { gRecording = true; return true; }
bool audioRecordStop(Bytes&) { gRecording = false; return false; }
bool audioRecording() { return gRecording; }
float audioRecordSeconds() { return 3.4f; }

// ---- images: demo media carries raw pixels: "RAW565" | w u16 | h u16 | pixels (swapped RGB565)
bool jpegEncode(const uint8_t* px, int w, int h, int, Bytes& out) {
  out.clear();
  putStr(out, "RAW565"); putU16(out, w); putU16(out, h); putBytes(out, px, w * h * 2);
  return true;
}

UiImage* imageDecode(const Bytes& d, int maxW, int maxH) {
  if (d.size() < 10 || memcmp(d.data(), "RAW565", 6)) return nullptr;
  int w = rdU16(&d[6]), h = rdU16(&d[8]);
  float s = std::min(1.0f, std::min((float)maxW / w, (float)maxH / h));
  int ow = std::max(1, (int)(w * s)), oh = std::max(1, (int)(h * s));
  uint8_t* buf = (uint8_t*)malloc(ow * oh * 2);
  for (int y = 0; y < oh; ++y)
    for (int x = 0; x < ow; ++x) {
      int sx = std::min(w - 1, (int)(x / s)), sy = std::min(h - 1, (int)(y / s));
      memcpy(buf + (y * ow + x) * 2, &d[10 + (sy * w + sx) * 2], 2);
    }
  auto* img = new UiImage();
  memset(&img->dsc, 0, sizeof(img->dsc));
  img->dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  img->dsc.header.w = ow;
  img->dsc.header.h = oh;
  img->dsc.data_size = ow * oh * 2;
  img->dsc.data = buf;
  img->handle = buf;
  return img;
}
void imageFree(UiImage* img) { if (img) { free(img->handle); delete img; } }

// ---- demo chat service ----
ChatService gChat;
struct DemoPeer { PeerInfo info; };
static std::vector<PeerInfo> demoPeers;
static const uint64_t BASE_TS = 1763654400000ull + 21 * 3600000ull + 37 * 60000ull;  // 21:37 UTC

void ChatService::begin() { nick_ = "neo"; synced_ = true; conv(""); }
void ChatService::loop() {}
uint64_t ChatService::nowMs() const { return BASE_TS + 13 * 60000ull + gFakeMillis; }
int ChatService::links() const { return 3; }
void ChatService::setCentralRole(bool) {}
std::string ChatService::myPeerId() const { return "3fa94c1e8b07d2a6"; }
std::string ChatService::myFingerprint() const {
  return "3fa94c1e8b07d2a6c55e10b9d7f2a4e19c0b6d3e8f71a2c4b5d6e7f8091a2b3c";
}
void ChatService::setNickname(const std::string& n) { nick_ = n; }
void ChatService::markRead(const std::string& c) { conv(c).unread = 0; }
void ChatService::startSession(const std::string&) {}
void ChatService::panic() {}
KeyInfo ChatService::keys() const {
  return {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
          "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
          "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
          "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb"};
}
std::string ChatService::debugInfo() {
  return "[mesh] adv seen 42 | out-links ok 2 fail 1 | in-links 1 subscribed 1 | now 2/1 (out/in)\n"
         "[mesh] frames rx 318 tx 97 | dup 54 | decode fail 0 | maxframe 509\n"
         "[mesh] announce ok 12 unverified 0 bad 0 | public 9 | noise hs 3 enc 6 | frag 0 | unpadded-sig 0\n"
         "[mesh] types 01:12 02:9 10:3 11:6";
}
bool ChatService::importIdentity(const std::string&, std::string*) { return true; }
std::vector<PeerInfo> ChatService::peerList() {
  for (auto& p : demoPeers) p.unread = conv(p.id).unread;
  return demoPeers;
}
std::string ChatService::peerNick(const std::string& id) {
  for (auto& p : demoPeers) if (p.id == id) return p.nick;
  return id.substr(0, 8);
}
bool ChatService::peerSecure(const std::string& id) {
  for (auto& p : demoPeers) if (p.id == id) return p.secure;
  return false;
}
bool ChatService::sendText(const std::string& c, const std::string& t, std::string*) {
  ChatMsg m; m.id = makeUUID(); m.outgoing = true; m.nick = nick_; m.text = t; m.ts = nowMs();
  m.status = c.empty() ? ST_SENT : ST_SENDING; addMessage(c, std::move(m)); return true;
}
bool ChatService::sendMedia(const std::string& c, const std::string& name, const std::string& mime, Bytes&& d, std::string*) {
  ChatMsg m; m.id = makeUUID(); m.outgoing = true; m.nick = nick_; m.ts = nowMs(); m.status = ST_SENT;
  m.media = std::make_shared<Media>(); m.media->name = name; m.media->mime = mime; m.media->data = std::move(d);
  addMessage(c, std::move(m)); return true;
}

// public helpers for the demo script
void demoAddPeer(const char* id, const char* nick, bool online, bool secure, uint32_t ago) {
  PeerInfo p; p.id = id; p.nick = nick; p.online = online; p.secure = secure; p.lastSeenMs = ago;
  demoPeers.push_back(p);
}
void demoMsg(const std::string& conv, const char* from, const char* fromId, const char* text, int minute,
             bool out = false, uint8_t status = ST_NONE, std::shared_ptr<Media> media = nullptr, bool verified = true) {
  ChatMsg m; m.id = makeUUID(); m.nick = from; m.senderId = fromId; m.text = text ? text : "";
  m.ts = BASE_TS + minute * 60000ull; m.outgoing = out; m.status = status; m.media = media; m.verified = verified;
  gChat.addMessage(conv, std::move(m));
}
void demoSystem(const std::string& conv, const char* text, int minute) {
  ChatMsg m; m.system = true; m.text = text; m.ts = BASE_TS + minute * 60000ull;
  gChat.addMessage(conv, std::move(m));
}
