// Tiny platform abstraction so that the UI can also be built natively
// (tools/screenshots renders every page headlessly on a PC).
#pragma once
#include <stdint.h>
uint32_t sysMillis();
uint32_t sysFreeHeap();
uint32_t sysFreePsram();
