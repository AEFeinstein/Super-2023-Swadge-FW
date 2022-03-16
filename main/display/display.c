//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include <string.h>
#include <esp_log.h>

#include "display.h"

#define QOI_NO_STDIO
#define QOI_IMPLEMENTATION
#include "qoi.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

//==============================================================================
// Defines
//==============================================================================

#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))
#define WRAP(x,u) (((x < 0) ? x + (u + 1) * ((-(x)) / (u + 1) + 1) : x) % (u + 1))

//==============================================================================
// Functions
//==============================================================================

int wrap(int kX, int const kLowerBound, int const kUpperBound)
{
    int range_size = kUpperBound - kLowerBound + 1;

    if (kX < kLowerBound)
        kX += range_size * ((kLowerBound - kX) / range_size + 1);

    return kLowerBound + (kX - kLowerBound) % range_size;
}

int wrapZero(int x, int const u)
{
    return ((x < 0) ? x + (u + 1) * ((-x) / (u + 1) + 1) : x) % (u + 1);
}

/**
 * @brief Fill a rectangular area on a display with a single color
 * 
 * @param disp The display to fill an area on
 * @param x1 The x coordinate to start the fill (top left)
 * @param y1 The y coordinate to start the fill (top left)
 * @param x2 The x coordinate to stop the fill (bottom right)
 * @param y2 The y coordinate to stop the fill (bottom right)
 * @param c  The color to fill
 */
void fillDisplayArea(display_t * disp, int16_t x1, int16_t y1, int16_t x2,
    int16_t y2, rgba_pixel_t c)
{
    // Only draw on the display
    int16_t xMin = CLAMP(x1, 0, disp->w);
    int16_t xMax = CLAMP(x2, 0, disp->w);
    int16_t yMin = CLAMP(y1, 0, disp->h);
    int16_t yMax = CLAMP(y2, 0, disp->h);
    
    // Set each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            disp->setPx(x, y, c);
        }
    }
}

/**
 * @brief Load a qoi from ROM to RAM. QOIs placed in the spiffs_image folder
 * before compilation will be automatically flashed to ROM 
 * 
 * @param name The filename of the QOI to load
 * @param qoi  A handle to load the qoi to
 * @return true if the qoi was loaded successfully,
 *         false if the qoi load failed and should not be used
 */
bool loadQoi(char * name, qoi_t * qoi)
{
    // Read QOI from file
    uint8_t * buf = NULL;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz))
    {
        ESP_LOGE("QOI", "Failed to read %s", name);
        return false;
    }

    // // Decode the QOI
    qoi_desc qd;
    qoi_rgba_t* pixels = (qoi_rgba_t*)qoi_decode(buf, sz, &qd, 4);
    free(buf);
    if(NULL == pixels)
    {
        ESP_LOGE("QOI", "QOI decode fail (%s)", name);
        return false;
    }

    // Save the image data in the arg
    qoi->px = malloc(sizeof(rgba_pixel_t) * qd.width * qd.height);
    if(NULL == qoi->px)
    {
        ESP_LOGE("QOI", "QOI malloc fail (%s)", name);
        free(pixels);
        return false;
    }
    qoi->h = qd.height;
    qoi->w = qd.width;

    // Copy each pixel
    for(uint16_t y = 0; y < qd.height; y++)
    {
        for(uint16_t x = 0; x < qd.width; x++)
        {
            qoi->px[(y * qd.width) + x].r = (pixels[(y * qd.width) + x].rgba.r * 0x1F) / 0xFF;
            qoi->px[(y * qd.width) + x].g = (pixels[(y * qd.width) + x].rgba.g * 0x1F) / 0xFF;
            qoi->px[(y * qd.width) + x].b = (pixels[(y * qd.width) + x].rgba.b * 0x1F) / 0xFF;
            qoi->px[(y * qd.width) + x].a =  pixels[(y * qd.width) + x].rgba.a ? PX_OPAQUE : PX_TRANSPARENT;
        }
    }

    // Free the decoded pixels
    free(pixels);

    // All done
    return true;
}

/**
 * @brief Allocate space for an empty QOI
 * 
 * @param name The filename of the QOI to load
 * @param qoi  A handle to load the qoi to
 * @return true if the qoi was loaded successfully,
 *         false if the qoi load failed and should not be used
 */
bool loadBlankQoi(qoi_t * qoi, unsigned int width, unsigned int height)
{
    // Save the image data in the arg
    qoi->px = malloc(sizeof(rgba_pixel_t) * width * height);
    if(NULL == qoi->px)
    {
        ESP_LOGE("QOI", "QOI malloc fail");
        return false;
    }
    qoi->h = height;
    qoi->w = width;

    // All done
    return true;
}


/**
 * @brief Free the memory for a loaded QOI
 * 
 * @param qoi The qoi to free memory from
 */
void freeQoi(qoi_t * qoi)
{
    free(qoi->px);
}

/**
 * @brief Draw a QOI to the display
 * 
 * @param disp The display to draw the QOI to
 * @param qoi  The QOI to draw to the display
 * @param xOff The x offset to draw the QOI at
 * @param yOff The y offset to draw the QOI at
 */
void drawQoi(display_t * disp, qoi_t *qoi, int16_t xOff, int16_t yOff)
{
    if(NULL == qoi->px)
    {
        return;
    }

    // Only draw in bounds
    int16_t xMin = CLAMP(xOff, 0, disp->w);
    int16_t xMax = CLAMP(xOff + qoi->w, 0, disp->w);
    int16_t yMin = CLAMP(yOff, 0, disp->h);
    int16_t yMax = CLAMP(yOff + qoi->h, 0, disp->h);
    
    // Draw each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            int16_t qoiX = x - xOff;
            int16_t qoiY = y - yOff;
            if (PX_OPAQUE == qoi->px[(qoiY * qoi->w) + qoiX].a)
            {
                disp->setPx(x, y, qoi->px[(qoiY * qoi->w) + qoiX]);
            }
        }
    }
}

/**
 * @brief Draw a QOI to the display
 * 
 * @param disp The display to draw the QOI to
 * @param qoi  The QOI to draw to the display
 * @param xOff The x offset to draw the QOI at
 * @param yOff The y offset to draw the QOI at
 */
void drawQoiTiled(display_t * disp, qoi_t *qoi, int16_t xOff, int16_t yOff)
{
    if(NULL == qoi->px)
    {
        return;
    }

    // Only draw in bounds
    int16_t xMin = 0;
    int16_t xMax = disp->w;
    int16_t yMin = 0;
    int16_t yMax = disp->h;
    
    // Draw each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            int16_t qoiX = WRAP(x - xOff, (qoi->w - 1));
            int16_t qoiY = WRAP(y - yOff, (qoi->h - 1));

            rgba_pixel_t * currentPixel = &qoi->px[(qoiY * qoi->w) + qoiX];

            if (PX_OPAQUE == currentPixel->a)
            {
                disp->setPx(x, y, *currentPixel);
            }
        }
    }
}

/**
 * @brief Draw an allocated QOI into another allocated QOI
 * 
 * @param source Pointer to the QOI to be drawn
 * @param destination Pointer to the QOI that will be drawn onto
 * @param xOff The x offset to draw the QOI at
 * @param yOff The y offset to draw the QOI at
 */
void drawQoiIntoQoi(qoi_t *source, qoi_t *destination, int16_t xOff, int16_t yOff)
{
    if(NULL == source->px || NULL == destination -> px)
    {
        return;
    }

    
    // Only draw in bounds
    int16_t xMin = CLAMP(xOff, 0, destination->w);
    int16_t xMax = CLAMP(xOff + source->w, 0, destination->w);
    int16_t yMin = CLAMP(yOff, 0, destination->h);
    int16_t yMax = CLAMP(yOff + source->h, 0, destination->h);
    
    // Draw each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            int16_t qoiX = x - xOff;
            int16_t qoiY = y - yOff;
            if (PX_OPAQUE == source->px[(qoiY * source->w) + qoiX].a)
            {
                destination->px[(y * destination->w) + x]=source->px[(qoiY * source->w) + qoiX];
            }
        }
    }
}

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
bool loadFont(const char * name, font_t * font)
{
    // Read font from file
    uint8_t * buf = NULL;
    size_t bufIdx = 0;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz))
    {
        ESP_LOGE("FONT", "Failed to read %s", name);
        return false;
    }

    // Read the data into a font struct
    font->h = buf[bufIdx++];

    // Read each char
    for(char ch = ' '; ch <= '~'; ch++)
    {
        // Get an easy refence to this character
        font_ch_t * this = &font->chars[ch - ' '];

        // Read the width
        this->w = buf[bufIdx++];

        // Figure out what size the char is
        int pixels = font->h * this->w;
        int bytes = (pixels / 8) + ((pixels % 8 == 0) ? 0 : 1);

        // Allocate space for this char and copy it over
        this->bitmap = (uint8_t *) malloc(sizeof(uint8_t) * bytes);
        memcpy(this->bitmap, &buf[bufIdx], bytes);
        bufIdx += bytes;
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
void freeFont(font_t * font)
{
    for(char ch = ' '; ch <= '~'; ch++)
    {
        free(font->chars[ch - ' '].bitmap);
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
 */
void drawChar(display_t * disp, rgba_pixel_t color, uint16_t h, font_ch_t * ch, int16_t xOff, int16_t yOff)
{
    uint16_t byteIdx = 0;
    uint8_t bitIdx = 0;
    // Iterate over the character bitmap
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < ch->w; x++)
        {
            // Figure out where to draw
            int drawX = x + xOff;
            int drawY = y + yOff;
            // If there is a pixel
            if (ch->bitmap[byteIdx] & (1 << bitIdx))
            {
                // Draw the pixel
                disp->setPx(drawX, drawY, color);
            }

            // Iterate over the bit data
            bitIdx++;
            if(8 == bitIdx) {
                bitIdx = 0;
                byteIdx++;
            }
        }
    }
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
 * @return The x offset at the end of the drawn string
 */
int16_t drawText(display_t * disp, font_t * font, rgba_pixel_t color, const char * text, int16_t xOff, int16_t yOff)
{
    while(*text != 0)
    {
        // Only draw if the char is on the screen
        if (xOff + font->chars[(*text) - ' '].w >= 0)
        {
            // Draw char
            drawChar(disp, color, font->h, &font->chars[(*text) - ' '], xOff, yOff);
        }

        // Move to the next char
        xOff += (font->chars[(*text) - ' '].w + 1);
        text++;

        // If this char is offscreen, finish drawing
        if(xOff >= disp->w)
        {
            return xOff;
        }
    }
    return xOff;
}

/**
 * @brief Return the pixel width of some text in a given font
 * 
 * @param font The font to use
 * @param text The text to measure
 * @return The width of the text rendered in the font
 */
uint16_t textWidth(font_t * font, const char * text)
{
    uint16_t width = 0;
    while(*text != 0)
    {
        width += (font->chars[(*text) - ' '].w + 1);
        text++;
    }
    // Delete trailing space
    if(0 < width)
    {
        width--;
    }
    return width;
}

/**
 * @brief Convert hue, saturation, and value to 15-bit RGB representation
 *
 * @param h The input hue
 * @param s The input saturation
 * @param v The input value
 * @return rgba_pixel_t The output RGB
 */
rgba_pixel_t hsv2rgb(uint16_t h, float s, float v)
{
    float hh, p, q, t, ff;
    uint16_t i;
    rgba_pixel_t px = {.a=PX_OPAQUE};

    hh = (h % 360) / 60.0f;
    i = (uint16_t)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch (i)
    {
        case 0:
        {
            px.r = v * 0x1F;
            px.g = t * 0x1F;
            px.b = p * 0x1F;
            break;
        }
        case 1:
        {
            px.r = q * 0x1F;
            px.g = v * 0x1F;
            px.b = p * 0x1F;
            break;
        }
        case 2:
        {
            px.r = p * 0x1F;
            px.g = v * 0x1F;
            px.b = t * 0x1F;
            break;
        }
        case 3:
        {
            px.r = p * 0x1F;
            px.g = q * 0x1F;
            px.b = v * 0x1F;
            break;
        }
        case 4:
        {
            px.r = t * 0x1F;
            px.g = p * 0x1F;
            px.b = v * 0x1F;
            break;
        }
        case 5:
        default:
        {
            px.r = v * 0x1F;
            px.g = p * 0x1F;
            px.b = q * 0x1F;
            break;
        }
    }
    return px;
}
