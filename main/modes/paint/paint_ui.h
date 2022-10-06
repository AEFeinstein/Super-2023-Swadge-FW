#ifndef _PAINT_UI_H_
#define _PAINT_UI_H_

#include <stdint.h>
#include <stdbool.h>

#include "palette.h"
#include "display.h"

#include "paint_type.h"
#include "paint_common.h"

void restoreCursorPixels(void);

void plotCursor(void);
void paintRenderCursor(void);

void drawColorBox(uint16_t xOffset, uint16_t yOffset, uint16_t w, uint16_t h, paletteColor_t col, bool selected, paletteColor_t topBorder, paletteColor_t bottomBorder);
void paintRenderToolbar(paintArtist_t* artist, paintCanvas_t* canvas);
void paintRenderAll(void);

void paintClearCanvas(const paintCanvas_t* canvas, paletteColor_t bgColor);
void initCursor(paintCursor_t* cursor, paintCanvas_t* canvas, const wsg_t* sprite);
void deinitCursor(paintCursor_t* cursor);
void setCursorSprite(paintCursor_t* cursor, paintCanvas_t* canvas, const wsg_t* sprite);
void undrawCursor(paintCursor_t* cursor, paintCanvas_t* canvas);
void hideCursor(paintCursor_t* cursor, paintCanvas_t* canvas);
void showCursor(paintCursor_t* cursor, paintCanvas_t* canvas);
void drawCursor(paintCursor_t* cursor, paintCanvas_t* canvas);
void moveCursorRelative(paintCursor_t* cursor, paintCanvas_t* canvas, int16_t xDiff, int16_t yDiff);

#endif
