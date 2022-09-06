#ifndef _DISPLAY_H_
#define _DISPLAY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "palette.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    paletteColor_t* px;
    uint16_t w;
    uint16_t h;
} wsg_t;

typedef void (*pxSetFunc_t)(int16_t x, int16_t y, paletteColor_t px);
typedef paletteColor_t (*pxGetFunc_t)(int16_t x, int16_t y);
typedef paletteColor_t * (*pxFbGetFunc_t)(void);
typedef void (*pxClearFunc_t)(void);
typedef void (*drawDisplayFunc_t)(bool drawDiff);

typedef struct
{
    pxSetFunc_t setPx;
    pxGetFunc_t getPx;
    pxFbGetFunc_t getPxFb;
    pxClearFunc_t clearPx;
    drawDisplayFunc_t drawDisplay;
    uint16_t w;
    uint16_t h;
} display_t;

typedef struct
{
    uint8_t w;
    uint8_t* bitmap;
} font_ch_t;

typedef struct
{
    uint8_t h;
    font_ch_t chars['~' - ' ' + 1];
} font_t;

//==============================================================================
// Prototypes
//==============================================================================

void fillDisplayArea(display_t* disp, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, paletteColor_t c);

bool loadWsg(char* name, wsg_t* wsg);
void drawWsg(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff,
             bool flipLR, bool flipUD, int16_t rotateDeg);
void drawWsgSimple(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff);
void drawWsgTile(display_t* disp, wsg_t* wsg, int32_t xOff, int32_t yOff);
void freeWsg(wsg_t* wsg);

bool loadFont(const char* name, font_t* font);
void drawChar(display_t* disp, paletteColor_t color, uint16_t h, font_ch_t* ch,
              int16_t xOff, int16_t yOff);
int16_t drawText(display_t* disp, font_t* font, paletteColor_t color,
                 const char* text, int16_t xOff, int16_t yOff);
uint16_t textWidth(font_t* font, const char* text);
void freeFont(font_t* font);

paletteColor_t hsv2rgb( uint8_t hue, uint8_t sat, uint8_t val);

int16_t getSin1024(int16_t degree);
int16_t getCos1024(int16_t degree);
int16_t getTan1024(int16_t degree);

#endif