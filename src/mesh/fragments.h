// Reassembly of bitchat fragment packets (type 0x20).
// payload: fragmentID[8] | index u16 BE | total u16 BE | originalType u8 | chunk
#pragma once
#include <map>
#include <string>
#include <vector>
#include "../proto/packet.h"

class FragmentAssembler {
 public:
  // Returns true and fills `frame` when the set is complete.
  bool add(const bc::Packet& p, Bytes& frame, uint8_t& originalType);
  void prune(uint32_t nowMs);
  size_t pending() const { return sets_.size(); }

 private:
  struct Set {
    uint16_t total = 0;
    uint16_t received = 0;
    uint8_t type = 0;
    size_t bytes = 0;
    uint32_t lastMs = 0;
    std::vector<Bytes> parts;
  };
  std::map<std::string, Set> sets_;
};
