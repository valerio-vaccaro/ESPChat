#include <RNG.h>
#include "stubs/esp_random.h"
RNGClass RNG;
RNGClass::RNGClass() {}
RNGClass::~RNGClass() {}
void RNGClass::rand(uint8_t* data, size_t len) { esp_fill_random(data, len); }
