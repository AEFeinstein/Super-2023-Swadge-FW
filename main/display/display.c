#include <string.h>
#include <esp_log.h>

#include "display.h"
#include "upng.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

/**
 * @brief TODO
 * 
 * @param name 
 * @param png 
 * @return true 
 * @return false 
 */
bool loadPng(char * name, png_t * png)
{
    // Read font from file
    uint8_t * buf = NULL;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz))
    {
        ESP_LOGE("PNG", "Failed to read %s", name);
        return false;
    }

    // Decode PNG
    upng_t * upng = upng_new_from_bytes(buf, sz);
    if(UPNG_EOK != upng_decode(upng))
    {
        ESP_LOGE("PNG", "Failed to decode png %s", name);
        return false;
    }

    // Variables for PNG metadata
    png->w  = upng_get_width(upng);
    png->h = upng_get_height(upng);
    unsigned int depth  = upng_get_bpp(upng) / 8;
    upng_format format  = upng_get_format(upng);

    // Validate the format
    if(UPNG_RGB8 != format && UPNG_RGBA8 != format)
    {
        ESP_LOGE("PNG", "Invalid PNG format %s (%d)", name, format);
        return false;
    }

    // Allocate space for pixels
    png->px = (rgba_pixel_t*)malloc(sizeof(rgba_pixel_t) * png->w * png->h);

    // Convert the PNG to 565 color, fill up pixels
    for(int y = 0; y < png->h; y++)
    {
        for(int x = 0; x < png->w; x++)
        {
            (png->px)[(y * png->w) + x].rgb.c.r = (127 + (upng_get_buffer(upng)[depth * ((y * png->w) + x) + 0]) * 31) / 255;
            (png->px)[(y * png->w) + x].rgb.c.g = (127 + (upng_get_buffer(upng)[depth * ((y * png->w) + x) + 1]) * 63) / 255;
            (png->px)[(y * png->w) + x].rgb.c.b = (127 + (upng_get_buffer(upng)[depth * ((y * png->w) + x) + 2]) * 31) / 255;
            if(UPNG_RGBA8 == format)
            {
                (png->px)[(y * png->w) + x].a = upng_get_buffer(upng)[depth * ((y * png->w) + x) + 3];
            }
        }
    }

    // Free the upng data
    upng_free(upng);

    // All done
    return true;
}

/**
 * @brief TODO
 * 
 * @param png 
 */
void freePng(png_t * png)
{
    free(png->px);
}


/**
 * @brief TODO
 * 
 * @param disp 
 * @param png 
 * @param xOff 
 * @param yOff 
 */
void drawPng(display_t * disp, png_t *png, uint16_t xOff, uint16_t yOff)
{
    for (int y = 0; y < png->h; y++)
    {
        for (int x = 0; x < png->w; x++)
        {
            if (png->px[(y * png->w) + x].a)
            {
                disp->drawPx(x + xOff, y + yOff, png->px[(y * png->w) + x]);
            }
        }
    }
}

/**
 * @brief TODO
 *
 * @param name
 * @param font
 * @return true
 * @return false
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
 * @brief TODO
 *
 * @param font
 */
void freeFont(font_t * font)
{
    for(char ch = ' '; ch <= '~'; ch++)
    {
        free(font->chars[ch - ' '].bitmap);
    }
}


/**
 * @brief TODO
 * 
 * @param disp 
 * @param color 
 * @param h 
 * @param ch 
 * @param xOff 
 * @param yOff 
 */
void drawChar(display_t * disp, rgba_pixel_t color, uint16_t h, font_ch_t * ch, uint16_t xOff, uint16_t yOff)
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
                disp->drawPx(drawX, drawY, color);
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
 * @brief TODO
 * 
 * TODO stop drawing when off screen right
 * 
 * @param disp 
 * @param font 
 * @param color 
 * @param text 
 * @param xOff 
 * @param yOff 
 */
void drawText(display_t * disp, font_t * font, rgba_pixel_t color, const char * text, uint16_t xOff, uint16_t yOff)
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
    }
}
