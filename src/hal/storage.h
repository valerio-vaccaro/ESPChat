// micro SD storage (WT32-SC01 Plus). All calls are no-ops when no card is present.
#pragma once
#include <string>
#include <vector>
#include "../proto/bytes.h"
struct Media;

bool storageBegin();
bool storageAvailable();
// Image files available for sending: /bitchat/send/*.jpg|*.jpeg|*.png and /*.jpg
std::vector<std::string> storageListImages();
bool storageRead(const std::string& path, Bytes& out, size_t maxBytes);
// Received media is saved under /bitchat/rx/
void storageSaveMedia(const Media& m);
bool storageWrite(const std::string& path, const std::string& text);
// Identity key file used by the KEYS page (hex text)
#define IDENTITY_KEY_FILE "/bitchat/identity.key"
