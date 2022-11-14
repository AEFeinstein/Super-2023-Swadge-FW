#ifndef _SWADGE_ESP32_H_
#define _SWADGE_ESP32_H_

#include <stdint.h>
#include "swadge_util.h"

#if defined(EMU)
extern bool lockMode;
#endif

void app_main(void);
void setFrameRateUs(uint32_t frameRate);
void cleanupOnExit(void);

#endif