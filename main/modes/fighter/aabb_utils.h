#ifndef _AABB_UTILS_H_
#define _AABB_UTILS_H_

#include <stdint.h>
#include "display.h"

typedef struct
{
    int32_t x;
    int32_t y;
} vector_t;

typedef struct
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
} box_t;

void drawBox(display_t* disp, box_t box, paletteColor_t color, bool isFilled, int32_t scalingFactor);
bool boxesCollide(box_t box0, box_t box1, int32_t scalingFactor);

#endif