#ifndef _EMU_MAIN_H_
#define _EMU_MAIN_H_

#include <stdint.h>
#include "display.h"

void emuSetPxTft(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t emuGetPxTft(int16_t x, int16_t y);
void emuClearPxTft(void);
void emuDrawDisplayTft(bool drawDiff);

void emuSetPxOled(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t emuGetPxOled(int16_t x, int16_t y);
void emuClearPxOled(void);
void emuDrawDisplayOled(bool drawDiff);

void onTaskYield(void);

#endif