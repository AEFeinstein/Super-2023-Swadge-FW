#ifndef _DISPLAY_H_
#define _DISPLAY_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * Structs
 ******************************************************************************/

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

typedef void (*pxDrawFunc_t)(int16_t x, int16_t y, rgba_pixel_t px);

typedef struct {
    pxDrawFunc_t drawPx;
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


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

bool loadPng(char * name, png_t *);
void drawPng(display_t * disp, png_t *png, uint16_t xOff, uint16_t yOff);
void freePng(png_t *);

bool loadFont(const char * name, font_t * font);
void drawChar(display_t * disp, rgba_pixel_t color, uint16_t h, font_ch_t * ch, uint16_t xOff, uint16_t yOff);
void drawText(display_t * disp, font_t * font, rgba_pixel_t color, const char * text, uint16_t xOff, uint16_t yOff);
void freeFont(font_t * font);

#endif