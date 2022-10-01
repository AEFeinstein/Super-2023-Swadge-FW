#ifndef _PAINT_UI_H_
#define _PAINT_UI_H_

#include <stdint.h>
#include <stdbool.h>

#include "palette.h"

void restoreCursorPixels(void);

void plotCursor(void);
void paintRenderCursor(void);

void drawColorBox(uint16_t xOffset, uint16_t yOffset, uint16_t w, uint16_t h, paletteColor_t col, bool selected, paletteColor_t topBorder, paletteColor_t bottomBorder);
void paintRenderToolbar(void);
void paintRenderAll(void);

void paintClearCanvas(void);


#endif
