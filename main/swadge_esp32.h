#ifndef _SWADGE_ESP32_H_
#define _SWADGE_ESP32_H_

#include <stdint.h>
#include "swadge_util.h"

#if defined(EMU)
extern int lockMode;
extern int monkeyAround;
extern int64_t fuzzerModeTestTime;
extern int64_t resetToMenuTimer;
extern char keyButtonsP1[8];
extern char keyButtonsP2[8];
extern char keyTouchP1[5];
extern char keyTouchP2[5];
#endif

void app_main(void);
void setFrameRateUs(uint32_t frameRate);
void cleanupOnExit(void);

#endif