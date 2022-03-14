#ifndef _DISPLAY_H_
#define _DISPLAY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>

//==============================================================================
// Defines
//==============================================================================

#define PX_TRANSPARENT 1
#define PX_OPAQUE      0

//==============================================================================
// Structs
//==============================================================================

typedef struct /*__attribute__((packed))*/ {
    uint16_t b:5;
    uint16_t a:1; // This is actually the LSB for green when transferring the framebuffer to the TFT
    uint16_t g:5;
    uint16_t r:5;
} rgba_pixel_t;

typedef union {
    rgba_pixel_t px;
    uint16_t val;
} rgba_pixel_disp_t;

typedef struct {
    rgba_pixel_t * px;
    uint16_t w;
    uint16_t h;
} qoi_t;

typedef void (*pxSetFunc_t)(int16_t x, int16_t y, rgba_pixel_t px);
typedef rgba_pixel_t (*pxGetFunc_t)(int16_t x, int16_t y);
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

bool loadQoi(char * name, qoi_t * qoi);
bool loadBlankQoi(qoi_t * qoi, unsigned int width, unsigned int height);
void drawQoi(display_t * disp, qoi_t *qoi, int16_t xOff, int16_t yOff);
void drawQoiTiled(display_t * disp, qoi_t *qoi, int16_t xOff, int16_t yOff);
void drawQoiIntoQoi(qoi_t *source, qoi_t *destination, int16_t xOff, int16_t yOff);
void freeQoi(qoi_t * qoi);

bool loadFont(const char * name, font_t * font);
void drawChar(display_t * disp, rgba_pixel_t color, uint16_t h, font_ch_t * ch,
    int16_t xOff, int16_t yOff);
int16_t drawText(display_t * disp, font_t * font, rgba_pixel_t color,
    const char * text, int16_t xOff, int16_t yOff);
uint16_t textWidth(font_t * font, const char * text);
void freeFont(font_t * font);

rgba_pixel_t hsv2rgb(uint16_t h, float s, float v);

#endif