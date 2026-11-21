#include "ble_mesh.h"
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <vector>
#include <string>
#include <map>
#include "../proto/packet.h"
#include "../crypto/crypto_util.h"

namespace mesh {

static constexpr int MAX_LINKS = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
static constexpr int MAX_CENTRAL_LINKS = MAX_LINKS > 1 ? MAX_LINKS - 1 : 1;
static constexpr size_t FRAGMENT_HEADER = 13;
static constexpr size_t DEFAULT_CHUNK = 469;

struct RxItem { uint8_t* data; uint16_t len; };
struct TxItem { Bytes* frame; uint16_t delayMs; };

struct CentralLink {
  NimBLEClient* client = nullptr;
  NimBLERemoteCharacteristic* chr = nullptr;
  std::string addr;
  bool withResponse = false;
};

static QueueHandle_t rxQueue, txQueue, connectQueue;
static SemaphoreHandle_t linkMutex;
static NimBLEServer* server = nullptr;
static NimBLECharacteristic* characteristic = nullptr;
static std::vector<CentralLink> centrals;
static std::map<uint16_t, bool> subscribers;   // conn handle -> subscribed
static std::map<std::string, uint32_t> failedAt;
static volatile bool linkUp = false;
static volatile uint32_t txPending = 0;
static volatile bool central = false;
static Stats st;
const Stats& stats() {
  st.advertising = NimBLEDevice::getAdvertising()->isAdvertising();
  return st;
}

static void pushRx(const uint8_t* d, size_t n) {
  if (!n || n > 4096) return;
  st.rxFrames++;
  RxItem it{(uint8_t*)malloc(n), (uint16_t)n};
  if (!it.data) return;
  memcpy(it.data, d, n);
  if (xQueueSend(rxQueue, &it, 0) != pdTRUE) free(it.data);
}

// ---------------- peripheral role ----------------
class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, ble_gap_conn_desc* desc) override {
    st.centralsIn++;
    log_i("[ble] central connected to us: %s", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    if (s->getConnectedCount() < (size_t)MAX_LINKS) NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer* s, ble_gap_conn_desc* desc) override {
    xSemaphoreTake(linkMutex, portMAX_DELAY);
    subscribers.erase(desc->conn_handle);
    xSemaphoreGive(linkMutex);
    NimBLEDevice::startAdvertising();
  }
};

class CharCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, ble_gap_conn_desc* desc) override {
    std::string v = c->getValue();
    pushRx((const uint8_t*)v.data(), v.size());
  }
  void onSubscribe(NimBLECharacteristic* c, ble_gap_conn_desc* desc, uint16_t sub) override {
    xSemaphoreTake(linkMutex, portMAX_DELAY);
    if (sub) subscribers[desc->conn_handle] = true;
    else subscribers.erase(desc->conn_handle);
    xSemaphoreGive(linkMutex);
    if (sub) { linkUp = true; st.subscribed++; }
    log_i("[ble] remote central %s notifications (conn %u)", sub ? "subscribed to" : "unsubscribed from",
          desc->conn_handle);
  }
};

// ---------------- central role ----------------
class ScanCB : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    static const NimBLEUUID main(SERVICE_UUID), test(SERVICE_UUID_TESTNET);
    if (!dev->isAdvertisingService(main) && !dev->isAdvertisingService(test)) return;
    st.advSeen++;
    NimBLEAddress* a = new NimBLEAddress(dev->getAddress());
    if (xQueueSend(connectQueue, &a, 0) != pdTRUE) delete a;
  }
};

class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* c) override {}
};

static ScanCB scanCB;
static ClientCB clientCB;

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) { pushRx(data, len); }

static std::string props(NimBLERemoteCharacteristic* c) {
  std::string p;
  if (c->canRead()) p += "R";
  if (c->canWrite()) p += "W";
  if (c->canWriteNoResponse()) p += "w";
  if (c->canNotify()) p += "N";
  if (c->canIndicate()) p += "I";
  return p;
}

// Find the bitchat characteristic, logging the remote GATT table when it is not where expected.
static NimBLERemoteCharacteristic* findBitchatCharacteristic(NimBLEClient* c, const char** why) {
  static const NimBLEUUID svcMain(SERVICE_UUID), svcTest(SERVICE_UUID_TESTNET), chrUuid(CHAR_UUID);
  vTaskDelay(pdMS_TO_TICKS(150));  // let the phone finish its own post-connect GATT traffic
  NimBLERemoteService* svc = nullptr;
  for (int attempt = 0; attempt < 2 && !svc && c->isConnected(); ++attempt) {
    std::vector<NimBLERemoteService*>* all = c->getServices(true);  // full primary service discovery
    if (all)
      for (auto* s : *all)
        if (s->getUUID() == svcMain || s->getUUID() == svcTest) svc = s;
    if (!svc) {
      log_d("[ble]   attempt %d: bitchat service not in %u services:", attempt + 1, all ? (unsigned)all->size() : 0);
      if (all)
        for (auto* s : *all) log_d("[ble]     service %s", s->getUUID().toString().c_str());
      vTaskDelay(pdMS_TO_TICKS(400));
    }
  }
  if (!svc) { *why = c->isConnected() ? "bitchat service not found" : "phone dropped the link during discovery"; return nullptr; }
  std::vector<NimBLERemoteCharacteristic*>* chars = svc->getCharacteristics(true);
  NimBLERemoteCharacteristic* exact = nullptr, *fallback = nullptr;
  if (chars)
    for (auto* ch : *chars) {
      log_d("[ble]     char %s [%s]", ch->getUUID().toString().c_str(), props(ch).c_str());
      if (ch->getUUID() == chrUuid) exact = ch;
      else if (!fallback && (ch->canNotify() || ch->canIndicate()) && (ch->canWrite() || ch->canWriteNoResponse()))
        fallback = ch;
    }
  if (!exact && fallback) log_i("[ble]   using fallback characteristic %s", fallback->getUUID().toString().c_str());
  NimBLERemoteCharacteristic* r = exact ? exact : fallback;
  if (!r) *why = "bitchat characteristic not found in service";
  else if (!r->canNotify() && !r->canIndicate()) { *why = "characteristic has no notify"; return nullptr; }
  return r;
}

static bool isCentralConnected(const std::string& addr) {
  for (auto& l : centrals)
    if (l.addr == addr) return true;
  return false;
}

static void connectTo(const NimBLEAddress& addr) {
  std::string a = addr.toString();
  uint32_t now = millis();
  auto f = failedAt.find(a);
  if (f != failedAt.end() && now - f->second < 30000) return;
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  bool skip = isCentralConnected(a) || (int)centrals.size() >= MAX_CENTRAL_LINKS ||
              NimBLEDevice::getClientListSize() >= (size_t)MAX_CENTRAL_LINKS;
  xSemaphoreGive(linkMutex);
  if (skip) return;
  log_i("[ble] bitchat node found: %s, connecting", a.c_str());

  NimBLEDevice::getScan()->stop();
  NimBLEClient* c = NimBLEDevice::createClient();
  c->setClientCallbacks(&clientCB, false);
  c->setConnectionParams(12, 24, 0, 200);
  c->setConnectTimeout(5);
  bool ok = c->connect(addr, true);
  NimBLERemoteCharacteristic* chr = nullptr;
  const char* why = ok ? nullptr : "link not established";
  if (ok) {
    chr = findBitchatCharacteristic(c, &why);
    if (chr) {
      bool sub = chr->canNotify() ? chr->subscribe(true, onNotify) : chr->subscribe(false, onNotify);
      if (!sub) why = "subscribe to notifications failed";
      ok = sub;
    } else {
      ok = false;
    }
  }
  if (!ok) {
    st.connectFail++;
    log_i("[ble] connect to %s failed: %s (connected=%d)", a.c_str(), why ? why : "?", c->isConnected());
    failedAt[a] = now;
    if (c->isConnected()) c->disconnect();
    NimBLEDevice::deleteClient(c);
    return;
  }
  CentralLink l;
  l.client = c;
  l.chr = chr;
  l.addr = a;
  l.withResponse = !chr->canWriteNoResponse();
  log_i("[ble]   bitchat characteristic ok [%s], subscribed", props(chr).c_str());
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  centrals.push_back(l);
  xSemaphoreGive(linkMutex);
  linkUp = true;
  st.connectOk++;
  log_i("[ble] central link up %s mtu=%u", a.c_str(), c->getMTU());
}

static void pruneCentrals() {
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  for (size_t i = 0; i < centrals.size();) {
    if (!centrals[i].client->isConnected()) {
      NimBLEClient* c = centrals[i].client;
      centrals.erase(centrals.begin() + i);
      xSemaphoreGive(linkMutex);
      NimBLEDevice::deleteClient(c);
      xSemaphoreTake(linkMutex, portMAX_DELAY);
    } else {
      ++i;
    }
  }
  xSemaphoreGive(linkMutex);
}

static void managerTask(void*) {
  NimBLEScan* scan = NimBLEDevice::getScan();
  for (;;) {
    NimBLEAddress* a = nullptr;
    if (xQueueReceive(connectQueue, &a, pdMS_TO_TICKS(1000)) == pdTRUE && a) {
      connectTo(*a);
      delete a;
    }
    pruneCentrals();
    // keep advertising so phones can connect to us as central
    if (!NimBLEDevice::getAdvertising()->isAdvertising() && server->getConnectedCount() < (size_t)MAX_LINKS) {
      st.advRestarts++;
      NimBLEDevice::startAdvertising();
    }
    // Scan in 5 s windows: at the end of each window NimBLE reports devices whose scan
    // response never arrived, and the next window starts with a clean cache.
    if (central && !scan->isScanning()) scan->start(5, nullptr, false);
    if (!central && scan->isScanning()) scan->stop();
  }
}

// ---------------- link utilities ----------------
size_t maxFrame() {
  size_t m = 512;
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  for (auto& s : subscribers) {
    uint16_t mtu = server->getPeerMTU(s.first);
    if (mtu > 3 && mtu - 3u < m) m = mtu - 3;
  }
  for (auto& l : centrals) {
    uint16_t mtu = l.client->getMTU();
    if (mtu > 3 && mtu - 3u < m) m = mtu - 3;
  }
  xSemaphoreGive(linkMutex);
  return m < 23 ? 20 : m;
}

int peripheralLinks() {
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  int n = subscribers.size();
  xSemaphoreGive(linkMutex);
  return n;
}
int centralLinks() {
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  int n = centrals.size();
  xSemaphoreGive(linkMutex);
  return n;
}
int linkCount() { return peripheralLinks() + centralLinks(); }

bool takeLinkUpEvent() {
  if (!linkUp) return false;
  linkUp = false;
  return true;
}

uint32_t txQueued() { return txPending; }

static void writeAll(const uint8_t* d, size_t n) {
  xSemaphoreTake(linkMutex, portMAX_DELAY);
  bool anySub = !subscribers.empty();
  std::vector<CentralLink> links = centrals;
  xSemaphoreGive(linkMutex);
  st.txFrames++;
  if (anySub) characteristic->notify(d, n, true);
  for (auto& l : links) {
    if (!l.client->isConnected()) continue;
    for (int attempt = 0; attempt < 3; ++attempt) {
      if (l.chr->writeValue(d, n, l.withResponse)) break;
      vTaskDelay(pdMS_TO_TICKS(15));
    }
  }
}

static size_t chunkFor(size_t mf) {
  size_t overhead = 14 + 8 + 8 + FRAGMENT_HEADER;
  size_t c = mf > overhead + 64 ? mf - overhead : 64;
  return c > DEFAULT_CHUNK ? DEFAULT_CHUNK : c;
}

size_t estimateFragments(size_t size) {
  size_t mf = maxFrame();
  if (size <= mf) return 1;
  size_t c = chunkFor(mf);
  return (size + c - 1) / c;
}

static void sendFragmented(const Bytes& f, size_t mf) {
  // parse the header fields we need from the encoded frame
  uint8_t version = f[0], type = f[1], ttl = f[2], flags = f[11];
  uint64_t ts = rdU64(&f[3]);
  size_t hdr = version == 2 ? 16 : 14;
  bc::Packet frag;
  frag.version = 1;
  frag.type = bc::MT_FRAGMENT;
  frag.ttl = ttl;
  frag.timestamp = ts;
  memcpy(frag.sender, &f[hdr], 8);
  frag.hasRecipient = flags & bc::F_HAS_RECIPIENT;
  if (frag.hasRecipient) memcpy(frag.recipient, &f[hdr + 8], 8);
  bool directed = frag.hasRecipient;

  size_t chunk = chunkFor(mf);
  size_t total = (f.size() + chunk - 1) / chunk;
  uint8_t fid[8];
  randomBytes(fid, 8);
  for (size_t i = 0; i < total; ++i) {
    size_t off = i * chunk, len = std::min(chunk, f.size() - off);
    frag.payload.clear();
    putBytes(frag.payload, fid, 8);
    putU16(frag.payload, (uint16_t)i);
    putU16(frag.payload, (uint16_t)total);
    putU8(frag.payload, type);
    putBytes(frag.payload, &f[off], len);
    Bytes out = bc::encode(frag, false);
    writeAll(out.data(), out.size());
    vTaskDelay(pdMS_TO_TICKS(directed ? 25 : 30));
  }
}

static void txTask(void*) {
  for (;;) {
    TxItem it;
    if (xQueueReceive(txQueue, &it, portMAX_DELAY) != pdTRUE) continue;
    if (it.delayMs) vTaskDelay(pdMS_TO_TICKS(it.delayMs));
    if (linkCount() > 0 && it.frame->size() >= 22) {
      size_t mf = maxFrame();
      if (it.frame->size() <= mf) writeAll(it.frame->data(), it.frame->size());
      else sendFragmented(*it.frame, mf);
    }
    delete it.frame;
    txPending--;
  }
}

void send(Bytes&& frame, uint16_t delayMs) {
  TxItem it{new Bytes(std::move(frame)), delayMs};
  txPending++;
  if (xQueueSend(txQueue, &it, pdMS_TO_TICKS(100)) != pdTRUE) {
    delete it.frame;
    txPending--;
  }
}

bool popRx(Bytes& out) {
  RxItem it;
  if (xQueueReceive(rxQueue, &it, 0) != pdTRUE) return false;
  out.assign(it.data, it.data + it.len);
  free(it.data);
  return true;
}

void setCentralRole(bool on) { central = on; }
bool centralRole() { return central; }

void begin(bool centralRole) {
  central = centralRole;
  rxQueue = xQueueCreate(96, sizeof(RxItem));
  txQueue = xQueueCreate(64, sizeof(TxItem));
  connectQueue = xQueueCreate(8, sizeof(NimBLEAddress*));
  linkMutex = xSemaphoreCreateMutex();

  NimBLEDevice::init("ESPChat");
  NimBLEDevice::setMTU(517);
  // bitchat phones only connect to devices heard above about -85 dBm (Android, -75 in background)
#if CONFIG_IDF_TARGET_ESP32S3
  NimBLEDevice::setPower(ESP_PWR_LVL_P18);
#else
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // maximum on the original ESP32
#endif

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  NimBLEService* svc = server->createService(SERVICE_UUID);
  characteristic = svc->createCharacteristic(
      CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR |
                     NIMBLE_PROPERTY::NOTIFY, 512);
  characteristic->setCallbacks(new CharCB());
  svc->start();

  // Advertisement: flags + complete list with the bitchat 128-bit UUID (what the apps' scan filter
  // matches); a short name goes in the scan response.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData ad, sr;
  ad.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  ad.setCompleteServices(NimBLEUUID(SERVICE_UUID));
  sr.setShortName("ESPChat");
  adv->setAdvertisementData(ad);
  adv->setScanResponseData(sr);
  adv->setMinInterval(48);   // 30 ms
  adv->setMaxInterval(96);   // 60 ms
  adv->start();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(&scanCB, false);
  scan->setActiveScan(true);   // the service UUID may be in the scan response
  scan->setInterval(100);
  scan->setWindow(60);
  scan->setDuplicateFilter(false);
  scan->setMaxResults(0);      // do not cache results: onResult fires for every advertisement
  if (central) scan->start(5, nullptr, false);

  xTaskCreatePinnedToCore(managerTask, "ble_mgr", 6144, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(txTask, "ble_tx", 6144, nullptr, 4, nullptr, 0);
}

}  // namespace mesh
