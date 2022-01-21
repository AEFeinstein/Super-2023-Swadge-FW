#ifndef _EMU_MAIN_H_
#define _EMU_MAIN_H_

#include <stdint.h>
#include "display.h"

void initRawDraw(int w, int h);
void emuSetPx(int16_t x, int16_t y, rgba_pixel_t px);
rgb_pixel_t emuGetPx(int16_t x, int16_t y);
void emuClearPx(void);
void emuDrawDisplay(bool drawDiff);

void onTaskYield(void);

#endif