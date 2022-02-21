#ifndef _AABB_UTILS_H_
#define _AABB_UTILS_H_

#include <stdint.h>

typedef struct {
    int16_t x;
    int16_t y;
} vector_t;

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} box_t;

void drawBox(display_t * disp, box_t box, rgba_pixel_t color, uint8_t fractional_bits);
bool boxesCollide(box_t box0, box_t box1);

#endif