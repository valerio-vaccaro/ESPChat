// Dual-role BLE transport compatible with bitchat:
//  - peripheral: GATT service F47B5E2D-...-4B5C with characteristic A1B2C3D4-... (write/notify)
//  - central: scans for the same service, connects, subscribes and writes without response
// Received frames are queued for the application task; outgoing frames go through a TX task
// that fragments anything larger than the smallest link MTU.
#pragma once
#include <Arduino.h>
#include "../proto/bytes.h"

namespace mesh {

constexpr const char* SERVICE_UUID = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C";
constexpr const char* SERVICE_UUID_TESTNET = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5A";  // bitchat DEBUG builds
constexpr const char* CHAR_UUID = "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D";

void begin(bool centralRole);
// Central role = we also scan for phones and connect to them. Off by default: phones connect to us
// (like the T-Deck client); scanning steals airtime from advertising.
void setCentralRole(bool on);
bool centralRole();
// Pop one received BLE value (a full bitchat frame). Returns false when empty.
bool popRx(Bytes& out);
// Queue a frame for broadcast to all links (fragmented when needed).
void send(Bytes&& frame, uint16_t delayMs = 0);
// Number of fragments a frame of `size` bytes would need with the current MTU.
size_t estimateFragments(size_t size);
int linkCount();
int centralLinks();      // we are central
int peripheralLinks();   // remote centrals connected to us
size_t maxFrame();
// Set once each time a new link becomes ready; cleared by the reader.
bool takeLinkUpEvent();
uint32_t txQueued();

// Diagnostics counters (shown in CFG > MESH DEBUG and on the serial log)
struct Stats {
  uint32_t advSeen = 0;        // advertisements carrying the bitchat service
  uint32_t connectOk = 0, connectFail = 0;
  uint32_t centralsIn = 0;     // remote centrals that connected to us
  uint32_t subscribed = 0;     // remote centrals that subscribed to notifications
  uint32_t rxFrames = 0, txFrames = 0;
  uint32_t advRestarts = 0;
  bool advertising = false;
};
const Stats& stats();

}  // namespace mesh
