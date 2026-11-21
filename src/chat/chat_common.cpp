// Parts of ChatService shared by the firmware and the native screenshot build.
#include "chat.h"
#include "../../include/board_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

Conversation& ChatService::conv(const std::string& convId) {
  auto it = convs_.find(convId);
  if (it == convs_.end()) {
    Conversation c;
    c.peerId = convId;
    it = convs_.emplace(convId, std::move(c)).first;
  }
  return it->second;
}

void ChatService::addMessage(const std::string& convId, ChatMsg&& m) {
  Conversation& c = conv(convId);
  if (!m.outgoing && !m.system) c.unread++;
  c.msgs.push_back(std::move(m));
  while (c.msgs.size() > MAX_MESSAGES_PER_CONV) c.msgs.pop_front();
  if (onMessage) onMessage(convId, c.msgs.back());
}

void ChatService::setStatus(const std::string& convId, const std::string& msgId, uint8_t st) {
  Conversation& c = conv(convId);
  for (auto it = c.msgs.rbegin(); it != c.msgs.rend(); ++it) {
    if (it->id == msgId) {
      if (st > it->status || st == ST_FAILED) {
        it->status = st;
        if (onStatus) onStatus(convId, msgId, st);
      }
      return;
    }
  }
}

int ChatService::totalUnreadPrivate() {
  int n = 0;
  for (auto& kv : convs_)
    if (!kv.first.empty()) n += kv.second.unread;
  return n;
}

std::string makeUUID() {
  uint8_t b[16];
  for (int i = 0; i < 16; ++i) b[i] = rand() & 0xFF;
  b[6] = (b[6] & 0x0F) | 0x40;
  b[8] = (b[8] & 0x3F) | 0x80;
  char s[40];
  snprintf(s, sizeof(s), "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", b[0], b[1], b[2],
           b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
  return s;
}

int ChatService::totalUnread() { return totalUnreadPrivate() + conv("").unread; }

// Parses "hex" into 32 or 64 bytes; returns number of bytes or 0.
size_t parseKeyHex(const std::string& in, uint8_t out[64]) {
  std::string h;
  for (char c : in)
    if (isxdigit((unsigned char)c)) h += c;
  if (h.size() != 64 && h.size() != 128) return 0;
  if (!fromHex(h, out, h.size() / 2)) return 0;
  return h.size() / 2;
}
