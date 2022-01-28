#ifndef _EMU_MAIN_H_
#define _EMU_MAIN_H_

#include <stdint.h>
#include <stdbool.h>
#include "display.h"
#include "btn.h"
#include "led_util.h"

void emuSetPxTft(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t emuGetPxTft(int16_t x, int16_t y);
void emuClearPxTft(void);
void emuDrawDisplayTft(bool drawDiff);

void emuSetPxOled(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t emuGetPxOled(int16_t x, int16_t y);
void emuClearPxOled(void);
void emuDrawDisplayOled(bool drawDiff);

void onTaskYield(void);

void setInputKeys(uint8_t numButtons, char * keyOrder);
bool checkInputKeys(buttonEvt_t * evt);

void initRawdrawLeds(uint8_t numLeds);
void setRawdrawLeds(led_t * leds, uint8_t numLeds);

#endif