#include "platform.h"
#include <Arduino.h>
uint32_t sysMillis() { return millis(); }
uint32_t sysFreeHeap() { return ESP.getFreeHeap(); }
uint32_t sysFreePsram() { return ESP.getFreePsram(); }
