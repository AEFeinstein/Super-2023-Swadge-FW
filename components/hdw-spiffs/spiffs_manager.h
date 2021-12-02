#ifndef _SPIFFS_MANAGER_H_
#define _SPIFFS_MANAGER_H_

#include <stdbool.h>
#include "hdw-tft.h"

bool initSpiffs(void);
bool deinitSpiffs(void);
bool spiffsTest(void);
bool loadPng(char * name, tft_pixel_t ** pxOut, uint16_t * w, uint16_t * h);

#endif
