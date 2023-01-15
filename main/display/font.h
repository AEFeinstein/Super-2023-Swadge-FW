#ifndef _FONT_H
#define _FONT_H

#include <stdint.h>
#include <stdbool.h>

#include "display.h"
#include "palette.h"

typedef struct
{
    uint8_t w;
    uint8_t* bitmap;
} font_ch_t;

typedef struct
{
    uint8_t h;
    font_ch_t chars['~' - ' ' + 2]; // enough space for all printed ascii chars, and pi
} font_t;

typedef enum
{
    TEXT_NORMAL = 0,
    TEXT_ITALIC = 1,
    TEXT_BOLD = 2,
    TEXT_UNDERLINE = 4,
    TEXT_STRIKE = 8,
} textAttr_t;

bool loadFont(const char* name, font_t* font);
void drawChar(display_t* disp, paletteColor_t color, int h, const font_ch_t* ch,
              int16_t xOff, int16_t yOff);
void drawCharItalic(display_t* disp, paletteColor_t color, int h, const font_ch_t* ch,
              int16_t xOff, int16_t yOff, int8_t slant);
int16_t drawText(display_t* disp, const font_t* font, paletteColor_t color,
                 const char* text, int16_t xOff, int16_t yOff);
int16_t drawTextAttrs(display_t* disp, const font_t* font, paletteColor_t color,
                      const char* text, int16_t xOff, int16_t yOff, uint8_t textAttrs);
const char* drawTextWordWrap(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                             int16_t *xOff, int16_t *yOff, int16_t xMax, int16_t yMax);
const char* drawTextWordWrapExtra(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                                  int16_t *xOff, int16_t *yOff, int16_t xMin, int16_t yMin,int16_t xMax, int16_t yMax,
                                  uint8_t textAttrs, const char* textEnd);
uint16_t textWidth(const font_t* font, const char* text);
uint16_t textLineHeight(const font_t* font, uint8_t textAttrs);
uint16_t textHeight(const font_t* font, const char* text, int16_t width, int16_t maxHeight);
uint16_t textHeightAttrs(const font_t* font, const char* text, int16_t startX, int16_t startY, int16_t width, int16_t maxHeight, uint8_t textAttrs);
uint16_t textWidthAttrs(const font_t* font, const char* text, uint8_t textAttrs);
uint16_t textWidthExtra(const font_t* font, const char* text, uint8_t textAttrs, const char* textEnd);
void freeFont(font_t* font);



#endif
