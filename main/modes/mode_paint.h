#ifndef _MODE_PAINT_H_
#define _MODE_PAINT_H_

#include "swadgeMode.h"

#define PAINT_MAX_COLORS 16
#define PAINT_SAVE_CHUNK_SIZE 1024
const int16_t PAINT_CANVAS_X_OFFSET = 18;
const int16_t PAINT_CANVAS_Y_OFFSET = 24;
//const int16_t PAINT_CANVAS_WIDTH = 140;
//const int16_t PAINT_CANVAS_HEIGHT = 120;
const int16_t PAINT_CANVAS_WIDTH = 256;
const int16_t PAINT_CANVAS_HEIGHT = 192;

const paletteColor_t PAINT_TOOLBAR_BG = c333;

extern swadgeMode modePaint;

void setPaintMainMenu(bool resetPos);

#endif
