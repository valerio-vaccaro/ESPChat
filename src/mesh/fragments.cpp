#include "fragments.h"
#include <Arduino.h>

static constexpr uint16_t MAX_FRAGMENTS = 10000;
static constexpr size_t MAX_SET_BYTES = bc::MAX_FRAMED_BYTES + 4096;
static constexpr size_t MAX_SETS = 16;

bool FragmentAssembler::add(const bc::Packet& p, Bytes& frame, uint8_t& originalType) {
  const Bytes& d = p.payload;
  if (d.size() < 13) return false;
  uint16_t idx = rdU16(&d[8]), total = rdU16(&d[10]);
  uint8_t type = d[12];
  if (total == 0 || total > MAX_FRAGMENTS || idx >= total) return false;
  std::string key = toHex(p.sender, 8) + toHex(d.data(), 8);

  auto it = sets_.find(key);
  if (it == sets_.end()) {
    if (sets_.size() >= MAX_SETS) {
      // evict the stalest set
      auto oldest = sets_.begin();
      for (auto i = sets_.begin(); i != sets_.end(); ++i)
        if (i->second.lastMs < oldest->second.lastMs) oldest = i;
      sets_.erase(oldest);
    }
    Set s;
    s.total = total;
    s.type = type;
    s.parts.resize(total);
    it = sets_.emplace(key, std::move(s)).first;
  }
  Set& s = it->second;
  if (s.total != total || s.type != type) return false;
  s.lastMs = millis();
  if (!s.parts[idx].empty()) return false;  // duplicate
  s.parts[idx].assign(d.begin() + 13, d.end());
  s.bytes += d.size() - 13;
  if (s.parts[idx].empty()) s.parts[idx].push_back(0);  // should not happen; keep slot marked
  s.received++;
  if (s.bytes > MAX_SET_BYTES) { sets_.erase(it); return false; }
  if (s.received < s.total) return false;

  frame.clear();
  frame.reserve(s.bytes);
  for (auto& part : s.parts) frame.insert(frame.end(), part.begin(), part.end());
  originalType = s.type;
  sets_.erase(it);
  return true;
}

void FragmentAssembler::prune(uint32_t now) {
  for (auto it = sets_.begin(); it != sets_.end();) {
    if (now - it->second.lastMs > 45000) it = sets_.erase(it);
    else ++it;
  }
}
