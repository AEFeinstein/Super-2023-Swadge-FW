#include "font.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>

#include "display.h"
#include "palette.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define SLANT(font) ((font->h + 3) / 4)

/**
 * @brief Load a font from ROM to RAM. Fonts are bitmapped image files that have
 * a single height, all ASCII characters, and a width for each character.
 * PNGs placed in the assets folder before compilation will be automatically
 * flashed to ROM
 *
 * @param name The name of the font to load
 * @param font A handle to load the font to
 * @return true if the font was loaded successfully
 *         false if the font failed to load and should not be used
 */
bool loadFont(const char* name, font_t* font)
{
    // Read font from file
    uint8_t* buf = NULL;
    size_t bufIdx = 0;
    uint8_t chIdx = 0;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz, true))
    {
        ESP_LOGE("FONT", "Failed to read %s", name);
        return false;
    }

    // Read the data into a font struct
    font->h = buf[bufIdx++];

    // Read each char
    while(bufIdx < sz)
    {
        // Get an easy refence to this character
        font_ch_t* this = &font->chars[chIdx++];

        // Read the width
        this->w = buf[bufIdx++];

        // Figure out what size the char is
        int pixels = font->h * this->w;
        int bytes = (pixels / 8) + ((pixels % 8 == 0) ? 0 : 1);

        // Allocate space for this char and copy it over
        this->bitmap = (uint8_t*) malloc(sizeof(uint8_t) * bytes);
        memcpy(this->bitmap, &buf[bufIdx], bytes);
        bufIdx += bytes;
    }

    // Zero out any unused chars
    while (chIdx <= '~' - ' ' + 1)
    {
        font->chars[chIdx].bitmap = NULL;
        font->chars[chIdx++].w = 0;
    }

    // Free the SPIFFS data
    free(buf);

    return true;
}

/**
 * @brief Free the memory allocated for a font
 *
 * @param font The font to free memory from
 */
void freeFont(font_t* font)
{
    // using uint8_t instead of char because a char will overflow to -128 after the last char is freed (\x7f)
    for(uint8_t idx = 0; idx <= '~' - ' ' + 1; idx++)
    {
        if (font->chars[idx].bitmap != NULL)
        {
            free(font->chars[idx].bitmap);
        }
    }
}


/**
 * @brief Draw a single character from a font to a display
 *
 * @param disp  The display to draw a character to
 * @param color The color of the character to draw
 * @param h     The height of the character to draw
 * @param ch    The character bitmap to draw (includes the width of the char)
 * @param xOff  The x offset to draw the char at
 * @param yOff  The y offset to draw the char at
 * @param slant The slope of the slant, with 1 being 45 degrees and `h` or 0 being 90 degrees (normal)
 */
void drawCharItalic(display_t* disp, paletteColor_t color, int h, const font_ch_t* ch, int16_t xOff, int16_t yOff, int8_t slant)
{
    //  This function has been micro optimized by cnlohr on 2022-09-07, using gcc version 8.4.0 (crosstool-NG esp-2021r2-patch3)
    int bitIdx = 0;
    const uint8_t* bitmap = ch->bitmap;
    int wch = ch->w;

    // Get a pointer to the end of the bitmap
    const uint8_t* endOfBitmap = &bitmap[((wch * h) + 7) >> 3] - 1;
    int origH = h;

    // Don't draw off the bottom of the screen.
    if( yOff + h > disp->h )
    {
        h = disp->h - yOff;
    }

    // Check Y bounds
    if(yOff < 0)
    {
        // Above the display, do wacky math with -yOff
        bitIdx -= yOff * wch;
        bitmap += bitIdx >> 3;
        bitIdx &= 7;
        h += yOff;
        origH += yOff;
        yOff = 0;
    }

    paletteColor_t* pxOutput = disp->pxFb + yOff * disp->w;

    for (int y = 0; y < h; y++)
    {
        // Figure out where to draw
        int truncate = 0;
        // for every (slant) lines we draw, we shift by one less
        int xSlant = slant ? (origH - y) / slant : 0;

        int startX = xOff + xSlant;
        if( xOff < 0 )
        {
            // Track how many groups of pixels we are skipping over
            // that weren't displayed on the left of the screen.
            startX = 0;
            bitIdx += -xOff;
            bitmap += bitIdx >> 3;
            bitIdx &= 7;
        }
        int endX = xOff + wch + xSlant;
        if( endX > disp->w )
        {
            // Track how many groups of pixels we are skipping over,
            // if the letter falls off the end of the screen.
            truncate = endX - disp->w;
            endX = disp->w;
        }

        uint8_t thisByte = *bitmap;
        for (int drawX = startX; drawX < endX; drawX++)
        {
            // Figure out where to draw
            // Check X bounds
            if(thisByte & (1 << bitIdx))
            {
                // Draw the pixel
                pxOutput[drawX] = color;
            }

            // Iterate over the bit data
            if( 8 == ++bitIdx )
            {
                bitIdx = 0;
                // Make sure not to read past the bitmap
                if(bitmap < endOfBitmap)
                {
                    thisByte = *(++bitmap);
                }
                else
                {
                    // No more bitmap, so return
                    return;
                }
            }
        }

        // Handle any remaining bits if we have ended off the end of the display.
        bitIdx += truncate;
        bitmap += bitIdx >> 3;
        bitIdx &= 7;
        pxOutput += disp->w;
    }
}

void drawChar(display_t* disp, paletteColor_t color, int h, const font_ch_t* ch, int16_t xOff, int16_t yOff)
{
    drawCharItalic(disp, color, h, ch, xOff, yOff, 0);
}

/**
 * @brief Draw text to a display with the given color and font
 *
 * @param disp  The display to draw a character to
 * @param font  The font to use for the text
 * @param color The color of the character to draw
 * @param text  The text to draw to the display
 * @param xOff  The x offset to draw the text at
 * @param yOff  The y offset to draw the text at
 * @param textAttrs A bitfield of textAttr_t indicating which options
 * @return The x offset at the end of the drawn string
 */
int16_t drawTextAttrs(display_t* disp, const font_t* font, paletteColor_t color, const char* text, int16_t xOff, int16_t yOff, uint8_t textAttrs)
{
    // this is the extra space needed to account for the end of where the text will be drawn!
    // this is not the extra space we need to add between each character, because that should
    // not include any extra space for italics
    uint8_t extraW = textWidthAttrs(font, "", textAttrs);
    int16_t xStart = xOff;

    while(*text >= ' ')
    {
        // Only draw if the char is on the screen
        if (xOff + font->chars[(*text) - ' '].w + extraW >= 0)
        {
            if (textAttrs & TEXT_BOLD)
            {
                for (uint8_t n = 0; n < 4; n++)
                {
                    if (textAttrs & TEXT_ITALIC)
                    {
                        drawCharItalic(disp, color, font->h, &font->chars[(*text) - ' '], xOff + (n & 1), yOff + (n >> 1), SLANT(font));
                    }
                    else
                    {
                        drawChar(disp, color, font->h, &font->chars[(*text) - ' '], xOff + (n & 1), yOff + (n >> 1));
                    }
                }

                // Account for the extra space
                xOff++;
            }
            else if (textAttrs & TEXT_ITALIC)
            {
                // Draw char
                drawCharItalic(disp, color, font->h, &font->chars[(*text) - ' '], xOff, yOff, SLANT(font));
            }
            else
            {
                drawChar(disp, color, font->h, &font->chars[(*text) - ' '], xOff, yOff);
            }
        }

        // Move to the next char
        xOff += (font->chars[(*text) - ' '].w + 1);

        // If this char is offscreen, finish drawing
        if(xOff >= disp->w)
        {
            break;
        }

        // Increment the text after we break so it's easier to check what the last char drawn was
        text++;
    }

    // TODO: Do we need to do something to make sure we don't draw the line farther than makes sense?
    // Like, if we reached the end of the line (so *text != '\0')

    if (textAttrs & TEXT_STRIKE)
    {
        int16_t x1 = MAX(xStart, xOff);
        SETUP_FOR_TURBO( disp );
        for (int16_t x = MIN(xStart, xOff); x < x1; x++)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, x, yOff + font->h / 2, color);
        }
    }

    if (textAttrs & TEXT_UNDERLINE)
    {
        int16_t x1 = MAX(xStart, xOff);
        SETUP_FOR_TURBO( disp );
        for (int16_t x = MIN(xStart, xOff); x < x1; x++)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, x, yOff + font->h + 1, color);
        }
    }

    return xOff;
}

int16_t drawText(display_t* disp, const font_t* font, paletteColor_t color, const char* text, int16_t xOff, int16_t yOff)
{
    return drawTextAttrs(disp, font, color, text, xOff, yOff, TEXT_NORMAL);
}

/**
 * @brief Return the pixel width of some text in a given font
 *
 * @param font The font to use
 * @param text The text to measure
 * @return The width of the text rendered in the font
 */
uint16_t textWidth(const font_t* font, const char* text)
{
    uint16_t width = 0;
    while(*text != 0)
    {
        if((*text) >= ' ')
        {
            width += (font->chars[(*text) - ' '].w + 1);
        }
        text++;
    }
    // Delete trailing space
    if(0 < width)
    {
        width--;
    }
    return width;
}

uint16_t textWidthAttrs(const font_t* font, const char* text, uint8_t textAttrs)
{
    return textWidth(font, text) + ((textAttrs & TEXT_ITALIC) ? font->h / abs(SLANT(font)) : 0) + ((textAttrs & TEXT_BOLD) ? 1 : 0);
}

uint16_t textWidthExtra(const font_t* font, const char* text, uint8_t textAttrs, const char* textEnd)
{
    uint16_t width = 0;
    while(text != textEnd)
    {
        if((*text) >= ' ')
        {
            width += (font->chars[(*text) - ' '].w + 1);
        }
        text++;
    }
    // Delete trailing space
    if(0 < width)
    {
        width--;
    }
    return width + ((textAttrs & TEXT_ITALIC) ? font->h / abs(SLANT(font)) : 0) + ((textAttrs & TEXT_BOLD) ? 1 : 0);
}

static const char* drawTextWordWrapInner(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                             int16_t *xOff, int16_t *yOff, int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                             uint8_t textAttrs, const char* textEnd)
{
    const char* textPtr = text;
    int16_t textX = *xOff, textY = *yOff;
    const char* breakPtr = NULL;
    size_t bufStrlen;
    char buf[64];

    // don't dereference that null pointer
    if (text == NULL)
    {
        return NULL;
    }

    const int16_t lineHeight = textLineHeight(font, textAttrs);

    if (textEnd == NULL)
    {
        textEnd = text + strlen(text);
    }

    // while there is text left to print, and the text would not exceed the Y-bounds...
    // Subtract 1 from the line height to account for the space we don't care about
    while (textPtr < textEnd && (textY + lineHeight - 1 <= yMax))
    {
        *yOff = textY;

        // skip leading spaces if we're at the start of the line
        for (; textX == xMin && *textPtr == ' ' && textPtr < textEnd; textPtr++);

        // handle newlines
        if (textPtr < textEnd && *textPtr == '\n')
        {
            textX = xMin;
            textY += lineHeight;
            textPtr++;
            continue;
        }

        // copy as much text as will fit into the buffer
        // leaving room for a null-terminator in case the string is longer
        strncpy(buf, textPtr, sizeof(buf) - 1);

        // ensure there is always a null-terminator even if
        buf[sizeof(buf) - 1] = '\0';

        breakPtr = strpbrk(textPtr, " -\n");

        // if strpbrk() returns NULL, we didn't find a char
        // otherwise, breakPtr will point to the first breakable char in textPtr
        bufStrlen = strlen(buf);

        if (breakPtr == NULL || breakPtr > textEnd)
        {
            breakPtr = textEnd;
        }

        if (breakPtr - textPtr > bufStrlen)
        {
            breakPtr = textPtr + bufStrlen;
        }

        switch (*breakPtr)
        {
            case ' ':
            case '-':
            breakPtr++;
            break;

            case '\n':
            default:
            break;
        }

        // end the string at the break
        buf[(breakPtr - textPtr)] = '\0';

        // The text is longer than an entire line, so we must shorten it
        if (xMin + textWidthAttrs(font, buf, textAttrs) > xMax)
        {
            // shorten the text until it fits
            while (textX + textWidthAttrs(font, buf, textAttrs) > xMax && breakPtr > textPtr)
            {
                buf[(--breakPtr - textPtr)] = '\0';
            }
        }

        // The text is longer than will fit on the rest of the current line
        // Or we shortened it down to nothing. Either way, move to next line.
        // Also, go back to the start of the loop so we don't
        // accidentally overrun the yMax
        if (textX + textWidthAttrs(font, buf, textAttrs) > xMax || breakPtr == textPtr)
        {
            // The line won't fit
            textY += lineHeight;
            textX = xMin;
            continue;
        }

        // the line must have enough space for the rest of the buffer
        // print the line, and advance the text pointer and offset
        if (disp != NULL && textY + lineHeight - 1 >= 0 && textY <= disp->h)
        {
            textX = drawTextAttrs(disp, font, color, buf, textX, textY, textAttrs);
        }
        else
        {
            // drawText returns the next text position, which is 1px past the last char
            // textWidth returns, well, the text width, so add 1 to account for the last pixel

            // We can't use textWidthAttrs() here! That will include the extra width from
            // italic text, and put way too much space between words and characters.
            // Instead, manually add another 1px if the text is bold
            textX += textWidth(font, buf) + 1;
        }
        textPtr = breakPtr;
    }

    // Return NULL if we've printed everything
    // Otherwise, return the remaining text
    *xOff = textX;
    return (textPtr == textEnd) ? NULL : textPtr;
}


/// @brief Draws text, breaking on word boundaries, until the given bounds are filled or all text is drawn.
///
/// Text will be drawn, starting at `(xOff, yOff)`, wrapping to the next line at ' ' or '-' when the next
/// word would exceed `xMax`, or immediately when a newline ('\\n') is encountered. Carriage returns and
/// tabs ('\\r', '\\t') are not supported. When the bottom of the next character would exceed `yMax`, no more
/// text is drawn and a pointer to the next undrawn character within `text` is returned. If all text has
/// been written, NULL is returned.
///
/// @param disp The display on which to draw the text
/// @param font The font to use when drawing the text
/// @param color The color of the text to be drawn
/// @param text The text to be pointed, as a null-terminated string
/// @param xOff The X-coordinate to begin drawing the text at
/// @param yOff The Y-coordinate to begin drawing the text at
/// @param xMax The maximum x-coordinate at which any text may be drawn
/// @param yMax The maximum ycoordinate at which text may be drawn
/// @return A pointer to the first unprinted character within `text`, or NULL if all text has been written
const char* drawTextWordWrap(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                             int16_t *xOff, int16_t *yOff, int16_t xMax, int16_t yMax)
{
    return drawTextWordWrapInner(disp, font, color, text, xOff, yOff, *xOff, *yOff, xMax, yMax, TEXT_NORMAL, NULL);
}

const char* drawTextWordWrapExtra(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                                  int16_t *xOff, int16_t *yOff, int16_t xMin, int16_t yMin,int16_t xMax, int16_t yMax,
                                  uint8_t textAttrs, const char* textEnd)
{
    return drawTextWordWrapInner(disp, font, color, text, xOff, yOff, xMin, yMin, xMax, yMax, textAttrs, textEnd);
}

uint16_t textLineHeight(const font_t* font, uint8_t textAttrs)
{
    return font->h + ((textAttrs & TEXT_BOLD) ? 1 : 0) + ((textAttrs & TEXT_UNDERLINE) ? 2 : 0) + 1;
}

uint16_t textHeightAttrs(const font_t* font, const char* text, int16_t startX, int16_t startY, int16_t width, int16_t maxHeight, uint8_t textAttrs)
{
    int16_t xEnd = startX;
    int16_t yEnd = startY;
    drawTextWordWrapInner(NULL, font, cTransparent, text, &xEnd, &yEnd, 0, 0, width, maxHeight, TEXT_NORMAL, NULL);
    return yEnd + textLineHeight(font, textAttrs);
}

uint16_t textHeight(const font_t* font, const char* text, int16_t width, int16_t maxHeight)
{
    return textHeightAttrs(font, text, 0, 0, width, maxHeight, TEXT_NORMAL);
}
