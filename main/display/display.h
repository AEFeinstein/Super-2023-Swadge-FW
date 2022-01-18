#ifndef _DISPLAY_H_
#define _DISPLAY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>

//==============================================================================
// Structs
//==============================================================================

typedef union {
    struct /*__attribute__((packed))*/ {
        uint16_t b:5;
        uint16_t g:6;
        uint16_t r:5;
    } c;
    uint16_t val;
} rgb_pixel_t;

typedef struct /*__attribute__((packed))*/ {
    rgb_pixel_t rgb;
    uint8_t a;
} rgba_pixel_t;

typedef struct {
    rgba_pixel_t * px;
    uint16_t w;
    uint16_t h;
} png_t;

typedef void (*pxSetFunc_t)(int16_t x, int16_t y, rgba_pixel_t px);
typedef rgb_pixel_t (*pxGetFunc_t)(int16_t x, int16_t y);
typedef void (*pxClearFunc_t)(void);
typedef void (*drawDisplayFunc_t)(bool drawDiff);

typedef struct {
    pxSetFunc_t setPx;
    pxGetFunc_t getPx;
    pxClearFunc_t clearPx;
    drawDisplayFunc_t drawDisplay;
    uint16_t w;
    uint16_t h;
} display_t;

typedef struct {
    uint8_t w;
    uint8_t * bitmap;
} font_ch_t;

typedef struct {
    uint8_t h;
    font_ch_t chars['~' - ' ' + 1];
} font_t;


//==============================================================================
// Prototypes
//==============================================================================

void fillDisplayArea(display_t * disp, int16_t x1, int16_t y1, int16_t x2,
    int16_t y2, rgba_pixel_t c);

bool loadPng(char * name, png_t *);
void drawPng(display_t * disp, png_t *png, int16_t xOff, int16_t yOff);
void freePng(png_t *);

bool loadFont(const char * name, font_t * font);
void drawChar(display_t * disp, rgba_pixel_t color, uint16_t h, font_ch_t * ch,
    int16_t xOff, int16_t yOff);
void drawText(display_t * disp, font_t * font, rgba_pixel_t color,
    const char * text, int16_t xOff, int16_t yOff);
void freeFont(font_t * font);

rgb_pixel_t hsv2rgb(uint16_t h, float s, float v);

#endif