#ifndef _DISPLAY_H_
#define _DISPLAY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "palette.h"

#if !defined(EMU)

// A technique to turbo time set pixels (not yet in use)
#define SETUP_FOR_TURBO( disp ) \
    register uint32_t dispWidth = disp->w; \
    register uint32_t dispHeight = disp->h; \
    register uint32_t dispPx = (uint32_t)disp->pxFb; \

// 5/4 cycles -- note you can do better if you don't need arbitrary X/Y's.
#define TURBO_SET_PIXEL(disp, opxc, opy, colorVal ) \
    asm volatile( "mul16u a4, %[width], %[y]\nadd a4, a4, %[px]\nadd a4, a4, %[opx]\ns8i %[val],a4, 0" \
                  : : [opx]"a"(opxc),[y]"a"(opy),[px]"a"(dispPx),[val]"a"(colorVal),[width]"a"(dispWidth) : "a4" );

// Very tricky:
//   We do bgeui which checks to make sure 0 <= x < MAX
//   Other than that, it's basically the same as above.
#define TURBO_SET_PIXEL_BOUNDS(disp, opxc, opy, colorVal ) \
    asm volatile( "bgeu %[opx], %[width], failthrough%=\nbgeu %[y], %[height], failthrough%=\nmul16u a4, %[width], %[y]\nadd a4, a4, %[px]\nadd a4, a4, %[opx]\ns8i %[val],a4, 0\nfailthrough%=:\n" \
                  : : [opx]"a"(opxc),[y]"a"(opy),[px]"a"(dispPx),[val]"a"(colorVal),[width]"a"(dispWidth),[height]"a"(dispHeight) : "a4" );

#else
#define SETUP_FOR_TURBO( disp )\
    __attribute__((unused)) uint32_t dispWidth = disp->w; \
    __attribute__((unused)) uint32_t dispHeight = disp->h;

#define TURBO_SET_PIXEL SET_PIXEL
#define TURBO_SET_PIXEL_BOUNDS SET_PIXEL_BOUNDS
#endif

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

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    paletteColor_t* px;
    uint16_t w;
    uint16_t h;
} wsg_t;

struct display;

typedef void (*fnBackgroundDrawCallback_t)(struct display* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up,
        int16_t upNum);

typedef void (*pxSetFunc_t)(int16_t x, int16_t y, paletteColor_t px);
typedef paletteColor_t (*pxGetFunc_t)(int16_t x, int16_t y);
typedef paletteColor_t* (*pxFbGetFunc_t)(void);
typedef void (*pxClearFunc_t)(void);
typedef void (*drawDisplayFunc_t)(struct display* disp, bool drawDiff, fnBackgroundDrawCallback_t cb);

struct display
{
    pxSetFunc_t setPx;
    pxGetFunc_t getPx;
    pxClearFunc_t clearPx;
    drawDisplayFunc_t drawDisplay;
    uint16_t w;
    uint16_t h;
    paletteColor_t* pxFb;  // may be null
};

typedef struct display display_t;

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

//==============================================================================
// Prototypes
//==============================================================================

void fillDisplayArea(display_t* disp, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, paletteColor_t c);

bool loadWsg(char* name, wsg_t* wsg);
bool loadWsgSpiRam(char* name, wsg_t* wsg, bool spiRam);
void drawWsg(display_t* disp, const wsg_t* wsg, int16_t xOff, int16_t yOff,
             bool flipLR, bool flipUD, int16_t rotateDeg);
void drawWsgSimpleFast(display_t* disp, const wsg_t* wsg, int16_t xOff, int16_t yOff);
void drawWsgTile(display_t* disp, const wsg_t* wsg, int32_t xOff, int32_t yOff);
void freeWsg(wsg_t* wsg);

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

// If you want to do your own thing.
extern const int16_t sin1024[360];

int16_t getSin1024(int16_t degree);
int16_t getCos1024(int16_t degree);
int32_t getTan1024(int16_t degree);

#endif
