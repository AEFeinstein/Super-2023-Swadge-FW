#ifndef _DISPLAY_H_
#define _DISPLAY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "palette.h"

#if !defined(EMU)
#define DISPLAY_HAS_FB
#endif

#if !defined(DISPLAY_HAS_FB)
// Draw a pixel directly to the framebuffer
#define SET_PIXEL(d, x, y, c)        d->setPx(x, y, c)
// Draw a pixel to the framebuffer with bounds checking
#define SET_PIXEL_BOUNDS(d, x, y, c) d->setPx(x, y, c)
// Get a pixel directly from the framebuffer
#define GET_PIXEL(d, x, y)           d->getPx(x, y)
#else
// A technique to turbo time set pixels (not yet in use)
#define TURBO_SET_PIXEL(x, y, px, val, width, temp, temp2) asm volatile( "mul16u %[temp2], %[width], %[y]\nadd %[temp, %[px], %[x]\nadd %[temp], %[temp2], %[px]\ns8i %[val],%[temp], 0" : [temp]"+a"(temp),[temp2]"+a"(temp2) : [x]"a"(x),[y]"a"(y),[px]"g"(px),[val]"a"(val),[width]"a"(width) : ); // 5/4 cycles
// Draw a pixel directly to the framebuffer
#define SET_PIXEL(d, x, y, c) (d)->pxFb[((y)*((d)->w))+(x)] = (c)
// Draw a pixel to the framebuffer with bounds checking
#define SET_PIXEL_BOUNDS(d, x, y, c) \
    do{ \
        if(0 <= (x) && (x) < (d)->w && 0 <= (y) && (y) < (d)->h) { \
            (d)->pxFb[((y)*((d)->w))+(x)] = (c); \
        } \
    } while(0)
// Get a pixel directly from the framebuffer
#define GET_PIXEL(d, x, y) (d)->pxFb[((y)*((d)->w))+(x)]
#endif

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
typedef paletteColor_t* (*pxFbGetFunc_t)(void);
typedef void (*pxClearFunc_t)(void);
typedef void (*drawDisplayFunc_t)(bool drawDiff);

typedef struct
{
    pxSetFunc_t setPx;
    pxGetFunc_t getPx;
    pxClearFunc_t clearPx;
    drawDisplayFunc_t drawDisplay;
    uint16_t w;
    uint16_t h;
    paletteColor_t * pxFb; // may be null
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
void drawChar(display_t* disp, paletteColor_t color, int h, font_ch_t* ch,
              int16_t xOff, int16_t yOff);
int16_t drawText(display_t* disp, font_t* font, paletteColor_t color,
                 const char* text, int16_t xOff, int16_t yOff);
uint16_t textWidth(font_t* font, const char* text);
void freeFont(font_t* font);

paletteColor_t hsv2rgb( uint8_t hue, uint8_t sat, uint8_t val);

int16_t getSin1024(int16_t degree);
int16_t getCos1024(int16_t degree);
int32_t getTan512(int16_t degree);

#endif