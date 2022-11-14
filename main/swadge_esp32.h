#ifndef _SWADGE_ESP32_H_
#define _SWADGE_ESP32_H_

#include <stdint.h>
#include "swadge_util.h"

#if defined(EMU)
extern bool lockMode;
extern bool monkeyAround;
extern int64_t fuzzerModeTestTime;
extern int64_t resetToMenuTimer;
#endif

void app_main(void);
void setFrameRateUs(uint32_t frameRate);
void cleanupOnExit(void);

#endif