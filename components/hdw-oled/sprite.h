#ifndef _SPRITE_H_
#define _SPRITE_H_

#include <stdint.h>

#include "ssd1306.h"

typedef struct
{
    uint8_t  width;
    uint8_t  height;
    uint16_t data[16];
    uint16_t dummy; // round size to multiple of 32 bits
} sprite_t;

int16_t plotSprite(int16_t x, int16_t y, const sprite_t* sprite, color col);

#endif
