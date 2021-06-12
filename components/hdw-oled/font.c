/*
 * font.c
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#include <stdint.h>

#include "ssd1306.h"
#include "sprite.h"
#include "font.h"

/**
 * @brief Draw a single character to the OLED display
 *
 * @param x The x position where to draw the character
 * @param y The y position where to draw the character
 * @param character The character to print
 * @param table A table of character sprites, in ASCII order
 * @param col WHITE, BLACK or INVERSE
 * @return The x position of the end of the character drawn
 */
int16_t plotChar(int16_t x, int16_t y,
                 char character, const sprite_t* table, color col)
{
    if ('a' <= character && character <= 'z')
    {
        character = (char) (character - 'a' + 'A');
    }
    return plotSprite(x, y, &table[character - ' '], col);
}

/**
 * @brief Draw a string to the display
 *
 * @param x The x position where to draw the string
 * @param y The y position where to draw the string
 * @param text The string to draw
 * @param font The font to draw the string in
 * @param col WHITE, BLACK or INVERSE
 * @return The x position of the end of the string drawn
 */
int16_t plotText(int16_t x, int16_t y, char* text, fonts font, color col)
{
    while (0 != *text)
    {
        switch (font)
        {
            case TOM_THUMB:
            {
                x = plotChar(x, y, *text, font_TomThumb, col);
                break;
            }
            case IBM_VGA_8:
            {
                x = plotChar(x, y, *text, font_IbmVga8, col);
                break;
            }
            case RADIOSTARS:
            {
                x = plotChar(x, y, *text, font_Radiostars, col);
                break;
            }
            default:
            {
                break;
            }
        }
        text++;
    }
    return x;
}
