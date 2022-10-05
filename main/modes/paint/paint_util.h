#ifndef _PAINT_UTIL_H_
#define _PAINT_UTIL_H_

#include "palette.h"
#include "swadge_util.h"

#include "paint_type.h"
#include "px_stack.h"

paletteColor_t getContrastingColor(paletteColor_t col);

// Extra drawing functions
void paintDrawWsgTemp(display_t* display, const wsg_t* wsg, pxStack_t* saveTo, uint16_t x, uint16_t y, colorMapFn_t colorSwap);

void paintPlotSquareWave(display_t* disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col);
void plotRectFilledScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);

void setPxScaled(display_t* disp, int x, int y, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);

uint8_t paintGetMaxScale(display_t* disp, uint16_t imgW, uint16_t imgH, uint16_t xMargin, uint16_t yMargin);

void paintConvertPickPoints(const pxStack_t* pxStack, point_t* points);

#endif
