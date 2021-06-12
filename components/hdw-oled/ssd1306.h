/*
 * ssd1306.h
 *
 *  Created on: Mar 16, 2019
 *      Author: adam, CNLohr
 */

#ifndef _SSD1306_H_
#define _SSD1306_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum __attribute__((__packed__))
{
    BLACK = 0,
    WHITE = 1,
    INVERSE = 2,
    TRANSPARENT_COLOR = 3,
    WHITE_F_TRANSPARENT_B = 4,
}
color;

typedef enum
{
    NOTHING_TO_DO,
    FRAME_DRAWN,
    FRAME_NOT_DRAWN
} oledResult_t;

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

bool initOLED(bool reset);
void drawPixel(int16_t x, int16_t y, color c);
void drawPixelUnsafe( int x, int y );
void drawPixelUnsafeBlack( int x, int y );
void drawPixelUnsafeC( int x, int y, color c );

color getPixel(int16_t x, int16_t y);
bool setOLEDparams(bool turnOnOff);
int updateOLEDScreenRange( uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage );
oledResult_t updateOLED(bool drawDifference);
void clearDisplay(void);
void fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c);

#endif /* _SSD1306_H_ */
