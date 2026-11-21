#include "storage.h"
#include "../../include/board_config.h"
#include "../chat/chat.h"

#if HAS_SD
#include <SD.h>
#include <SPI.h>

static bool sdOk = false;
static SPIClass sdSpi(HSPI);

bool storageBegin() {
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOk = SD.begin(SD_CS, sdSpi, 20000000);
  if (sdOk) {
    SD.mkdir("/bitchat");
    SD.mkdir("/bitchat/rx");
    SD.mkdir("/bitchat/send");
  }
  log_i("SD card %s", sdOk ? "mounted" : "not found");
  return sdOk;
}
bool storageAvailable() { return sdOk; }

static bool isImageName(const String& n) {
  String l = n;
  l.toLowerCase();
  return l.endsWith(".jpg") || l.endsWith(".jpeg") || l.endsWith(".png");
}

std::vector<std::string> storageListImages() {
  std::vector<std::string> out;
  if (!sdOk) return out;
  for (const char* dir : {"/bitchat/send", "/"}) {
    File d = SD.open(dir);
    if (!d) continue;
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      if (!f.isDirectory() && isImageName(f.name())) {
        String p = String(dir) + (strcmp(dir, "/") ? "/" : "") + f.name();
        out.push_back(p.c_str());
      }
      if (out.size() >= 64) break;
    }
  }
  return out;
}

bool storageRead(const std::string& path, Bytes& out, size_t maxBytes) {
  if (!sdOk) return false;
  File f = SD.open(path.c_str());
  if (!f || f.size() > maxBytes) return false;
  out.resize(f.size());
  size_t n = f.read(out.data(), out.size());
  return n == out.size();
}

void storageSaveMedia(const Media& m) {
  if (!sdOk || m.data.empty()) return;
  std::string name = m.name.empty() ? "file_" + makeUUID().substr(0, 8) : m.name;
  for (auto& c : name)
    if (c == '/' || c == '\\') c = '_';
  File f = SD.open(("/bitchat/rx/" + name).c_str(), FILE_WRITE);
  if (!f) return;
  f.write(m.data.data(), m.data.size());
  f.close();
}

bool storageWrite(const std::string& path, const std::string& text) {
  if (!sdOk) return false;
  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) return false;
  size_t n = f.write((const uint8_t*)text.data(), text.size());
  f.close();
  return n == text.size();
}

#else
bool storageWrite(const std::string&, const std::string&) { return false; }
bool storageBegin() { return false; }
bool storageAvailable() { return false; }
std::vector<std::string> storageListImages() { return {}; }
bool storageRead(const std::string&, Bytes&, size_t) { return false; }
void storageSaveMedia(const Media&) {}
#endif
