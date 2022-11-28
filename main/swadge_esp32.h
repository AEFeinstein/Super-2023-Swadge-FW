#ifndef _SWADGE_ESP32_H_
#define _SWADGE_ESP32_H_

#include <stdint.h>
#include "swadge_util.h"

#if defined(EMU)
// TODO probably time for a struct...
extern int lockMode;
extern int monkeyAround;
extern int fullscreen;
extern int64_t fuzzerModeTestTime;
extern int64_t resetToMenuTimer;
extern int64_t fuzzButtonDelay;
extern int8_t fuzzButtonProbability;

extern char keyButtonsP1[8];
extern char keyButtonsP2[8];
extern char keyTouchP1[5];
extern char keyTouchP2[5];

extern char fuzzKeysP1[8];
extern char fuzzKeysP2[8];
extern char fuzzTouchP1[5];
extern char fuzzTouchP2[5];
extern uint8_t fuzzKeyCount;
extern uint8_t fuzzTouchCount;
#endif

void app_main(void);
void setFrameRateUs(uint32_t frameRate);
void cleanupOnExit(void);

#endif