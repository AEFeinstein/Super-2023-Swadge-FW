//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include <string.h>
#include <esp_log.h>
#ifdef _TEST_USE_SPIRAM_
    #include <esp_heap_caps.h>
#endif

#include "display.h"
#include "heatshrink_decoder.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

//==============================================================================
// Defines
//==============================================================================

#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))

//==============================================================================
// Constant data
//==============================================================================

const uint32_t sin1024[] =
{
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213, 230, 248, 265,
    282, 299, 316, 333, 350, 367, 384, 400, 416, 433, 449, 465, 481, 496, 512,
    527, 543, 558, 573, 587, 602, 616, 630, 644, 658, 672, 685, 698, 711, 724,
    737, 749, 761, 773, 784, 796, 807, 818, 828, 839, 849, 859, 868, 878, 887,
    896, 904, 912, 920, 928, 935, 943, 949, 956, 962, 968, 974, 979, 984, 989,
    994, 998, 1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023,
    1024, 1024, 1024, 1023, 1023, 1022, 1020, 1018, 1016, 1014, 1011, 1008,
    1005, 1002, 998, 994, 989, 984, 979, 974, 968, 962, 956, 949, 943, 935, 928,
    920, 912, 904, 896, 887, 878, 868, 859, 849, 839, 828, 818, 807, 796, 784,
    773, 761, 749, 737, 724, 711, 698, 685, 672, 658, 644, 630, 616, 602, 587,
    573, 558, 543, 527, 512, 496, 481, 465, 449, 433, 416, 400, 384, 367, 350,
    333, 316, 299, 282, 265, 248, 230, 213, 195, 178, 160, 143, 125, 107, 89,
    71, 54, 36, 18, 0, -18, -36, -54, -71, -89, -107, -125, -143, -160, -178,
    -195, -213, -230, -248, -265, -282, -299, -316, -333, -350, -367, -384,
    -400, -416, -433, -449, -465, -481, -496, -512, -527, -543, -558, -573,
    -587, -602, -616, -630, -644, -658, -672, -685, -698, -711, -724, -737,
    -749, -761, -773, -784, -796, -807, -818, -828, -839, -849, -859, -868,
    -878, -887, -896, -904, -912, -920, -928, -935, -943, -949, -956, -962,
    -968, -974, -979, -984, -989, -994, -998, -1002, -1005, -1008, -1011, -1014,
    -1016, -1018, -1020, -1022, -1023, -1023, -1024, -1024, -1024, -1023, -1023,
    -1022, -1020, -1018, -1016, -1014, -1011, -1008, -1005, -1002, -998, -994,
    -989, -984, -979, -974, -968, -962, -956, -949, -943, -935, -928, -920,
    -912, -904, -896, -887, -878, -868, -859, -849, -839, -828, -818, -807,
    -796, -784, -773, -761, -749, -737, -724, -711, -698, -685, -672, -658,
    -644, -630, -616, -602, -587, -573, -558, -543, -527, -512, -496, -481,
    -465, -449, -433, -416, -400, -384, -367, -350, -333, -316, -299, -282,
    -265, -248, -230, -213, -195, -178, -160, -143, -125, -107, -89, -71, -54,
    -36, -18
};

const uint32_t tan1024[] =
{
    0, 18, 36, 54, 72, 90, 108, 126, 144, 162, 181, 199, 218, 236, 255, 274, 294,
    313, 333, 353, 373, 393, 414, 435, 456, 477, 499, 522, 544, 568, 591, 615,
    640, 665, 691, 717, 744, 772, 800, 829, 859, 890, 922, 955, 989, 1024, 1060,
    1098, 1137, 1178, 1220, 1265, 1311, 1359, 1409, 1462, 1518, 1577, 1639, 1704,
    1774, 1847, 1926, 2010, 2100, 2196, 2300, 2412, 2534, 2668, 2813, 2974,
    3152, 3349, 3571, 3822, 4107, 4435, 4818, 5268, 5807, 6465, 7286, 8340, 9743,
    11704, 14644, 19539, 29324, 58665, 67108863, -58665, -29324, -19539, -14644,
    -11704, -9743, -8340, -7286, -6465, -5807, -5268, -4818, -4435, -4107, -3822,
    -3571, -3349, -3152, -2974, -2813, -2668, -2534, -2412, -2300, -2196, -2100,
    -2010, -1926, -1847, -1774, -1704, -1639, -1577, -1518, -1462, -1409,
    -1359, -1311, -1265, -1220, -1178, -1137, -1098, -1060, -1024, -989, -955,
    -922, -890, -859, -829, -800, -772, -744, -717, -691, -665, -640, -615, -591,
    -568, -544, -522, -499, -477, -456, -435, -414, -393, -373, -353, -333, -313,
    -294, -274, -255, -236, -218, -199, -181, -162, -144, -126, -108, -90, -72,
    -54, -36, -18, 0, 18, 36, 54, 72, 90, 108, 126, 144, 162, 181, 199, 218,
    236, 255, 274, 294, 313, 333, 353, 373, 393, 414, 435, 456, 477, 499, 522,
    544, 568, 591, 615, 640, 665, 691, 717, 744, 772, 800, 829, 859, 890, 922, 955,
    989, 1024, 1060, 1098, 1137, 1178, 1220, 1265, 1311, 1359, 1409, 1462,
    1518, 1577, 1639, 1704, 1774, 1847, 1926, 2010, 2100, 2196, 2300, 2412, 2534,
    2668, 2813, 2974, 3152, 3349, 3571, 3822, 4107, 4435, 4818, 5268, 5807, 6465,
    7286, 8340, 9743, 11704, 14644, 19539, 29324, 58665, 67108863, -58665, -29324,
    -19539, -14644, -11704, -9743, -8340, -7286, -6465, -5807, -5268, -4818,
    -4435, -4107, -3822, -3571, -3349, -3152, -2974, -2813, -2668, -2534, -2412,
    -2300, -2196, -2100, -2010, -1926, -1847, -1774, -1704, -1639, -1577, -1518,
    -1462, -1409, -1359, -1311, -1265, -1220, -1178, -1137, -1098, -1060, -1024,
    -989, -955, -922, -890, -859, -829, -800, -772, -744, -717, -691, -665,
    -640, -615, -591, -568, -544, -522, -499, -477, -456, -435, -414, -393, -373,
    -353, -333, -313, -294, -274, -255, -236, -218, -199, -181, -162, -144, -126,
    -108, -90, -72, -54, -36, -18
};

//==============================================================================
// Function Prototypes
//==============================================================================

void transformPixel(int16_t* x, int16_t* y, int16_t transX,
                    int16_t transY, bool flipLR, bool flipUD,
                    int16_t rotateDeg, int16_t width, int16_t height);

//==============================================================================
// Functions
//==============================================================================

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
void fillDisplayArea(display_t* disp, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, paletteColor_t c)
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
 * @brief Load a WSG from ROM to RAM. WSGs placed in the spiffs_image folder
 * before compilation will be automatically flashed to ROM
 *
 * @param name The filename of the WSG to load
 * @param wsg  A handle to load the WSG to
 * @return true if the WSG was loaded successfully,
 *         false if the WSG load failed and should not be used
 */
bool loadWsg(char* name, wsg_t* wsg)
{
    // Read WSG from file
    uint8_t* buf = NULL;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz))
    {
        ESP_LOGE("WSG", "Failed to read %s", name);
        return false;
    }

    // Pick out the decompresed size and create a space for it
    uint16_t decompressedSize = (buf[0] << 8) | buf[1];
    uint8_t decompressedBuf[decompressedSize];

    // Create the decoder
    size_t copied = 0;
    heatshrink_decoder* hsd = heatshrink_decoder_alloc(256, 8, 4);
    heatshrink_decoder_reset(hsd);

    // Decode the file in chunks
    uint32_t inputIdx = 0;
    uint32_t outputIdx = 0;
    while(inputIdx < (sz - 2))
    {
        // Decode some data
        copied = 0;
        heatshrink_decoder_sink(hsd, &buf[2 + inputIdx], sz - 2 - inputIdx, &copied);
        inputIdx += copied;

        // Save it to the output array
        copied = 0;
        heatshrink_decoder_poll(hsd, &decompressedBuf[outputIdx], sizeof(decompressedBuf) - outputIdx, &copied);
        outputIdx += copied;
    }

    // Note that it's all done
    heatshrink_decoder_finish(hsd);

    // Flush any final output
    copied = 0;
    heatshrink_decoder_poll(hsd, &decompressedBuf[outputIdx], sizeof(decompressedBuf) - outputIdx, &copied);
    outputIdx += copied;

    // All done decoding
    heatshrink_decoder_finish(hsd);
    heatshrink_decoder_free(hsd);
    // Free the bytes read from the file
    free(buf);

    // Save the decompressed info to the wsg. The first four bytes are dimension
    wsg->w = (decompressedBuf[0] << 8) | decompressedBuf[1];
    wsg->h = (decompressedBuf[2] << 8) | decompressedBuf[3];
    // The rest of the bytes are pixels
#ifdef _TEST_USE_SPIRAM_
    wsg->px = (paletteColor_t*)heap_caps_malloc(sizeof(paletteColor_t) * wsg->w * wsg->h, MALLOC_CAP_SPIRAM);
#else
    wsg->px = (paletteColor_t*)malloc(sizeof(paletteColor_t) * wsg->w * wsg->h);
#endif
    if(NULL != wsg->px)
    {
        memcpy(wsg->px, &decompressedBuf[4], outputIdx - 4);
        return true;
    }

    // all done
    return false;
}

/**
 * @brief Free the memory for a loaded WSG
 *
 * @param wsg The WSG to free memory from
 */
void freeWsg(wsg_t* wsg)
{
    free(wsg->px);
}

/**
 * Transform a pixel's coordinates by rotation around the sprite's center point,
 * then reflection over Y axis, then reflection over X axis, then translation
 *
 * This intentionally does not have ICACHE_FLASH_ATTR because it may be called often
 *
 * @param x The x coordinate of the pixel location to transform
 * @param y The y coordinate of the pixel location to trasform
 * @param transX The number of pixels to translate X by
 * @param transY The number of pixels to translate Y by
 * @param flipLR true to flip over the Y axis, false to do nothing
 * @param flipUD true to flip over the X axis, false to do nothing
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 * @param width  The width of the image
 * @param height The height of the image
 */
void transformPixel(int16_t* x, int16_t* y, int16_t transX,
                    int16_t transY, bool flipLR, bool flipUD,
                    int16_t rotateDeg, int16_t width, int16_t height)
{
    // First rotate the sprite around the sprite's center point
    if (0 < rotateDeg && rotateDeg < 360)
    {
        // This solves the aliasing problem, but because of tan() it's only safe
        // to rotate by 0 to 90 degrees. So rotate by a multiple of 90 degrees
        // first, which doesn't need trig, then rotate the rest with shears
        // See http://datagenetics.com/blog/august32013/index.html
        // See https://graphicsinterface.org/wp-content/uploads/gi1986-15.pdf

        // Center around (0, 0)
        (*x) -= (width / 2);
        (*y) -= (height / 2);

        // First rotate to the nearest 90 degree boundary, which is trivial
        if(rotateDeg >= 270)
        {
            // (x, y) -> (y, -x)
            int16_t tmp = (*x);
            (*x) = (*y);
            (*y) = -tmp;
        }
        else if(rotateDeg >= 180)
        {
            // (x, y) -> (-x, -y)
            (*x) = -(*x);
            (*y) = -(*y);
        }
        else if(rotateDeg >= 90)
        {
            // (x, y) -> (-y, x)
            int16_t tmp = (*x);
            (*x) = -(*y);
            (*y) = tmp;
        }
        // Now that it's rotated to a 90 degree boundary, find out how much more
        // there is to rotate by shearing
        rotateDeg = rotateDeg % 90;

        // If there's any more to rotate, apply three shear matrices in order
        // if(rotateDeg > 1 && rotateDeg < 89)
        if(rotateDeg > 0)
        {
            // 1st shear
            (*x) = (*x) - (((*y) * tan1024[rotateDeg / 2]) + 512) / 1024;
            // 2nd shear
            (*y) = (((*x) * sin1024[rotateDeg]) + 512) / 1024 + (*y);
            // 3rd shear
            (*x) = (*x) - (((*y) * tan1024[rotateDeg / 2]) + 512) / 1024;
        }

        // Return pixel to original position
        (*x) = (*x) + (width / 2);
        (*y) = (*y) + (height / 2);
    }

    // Then reflect over Y axis
    if (flipLR)
    {
        (*x) = width - 1 - (*x);
    }

    // Then reflect over X axis
    if(flipUD)
    {
        (*y) = height - 1 - (*y);
    }

    // Then translate
    (*x) += transX;
    (*y) += transY;
}

/**
 * @brief Draw a WSG to the display
 *
 * @param disp The display to draw the WSG to
 * @param wsg  The WSG to draw to the display
 * @param xOff The x offset to draw the WSG at
 * @param yOff The y offset to draw the WSG at
 * @param flipLR true to flip the image across the Y axis
 * @param flipUD true to flip the image across the X axis
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 */
void drawWsg(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff,
             bool flipLR, bool flipUD, int16_t rotateDeg)
{
    if(NULL == wsg->px)
    {
        return;
    }

    // Draw the image's pixels
    for(int16_t srcY = 0; srcY < wsg->h; srcY++)
    {
        for(int16_t srcX = 0; srcX < wsg->w; srcX++)
        {
            // Draw if not transparent
            if (cTransparent != wsg->px[(srcY * wsg->w) + srcX])
            {
                // Transform this pixel's draw location as necessary
                int16_t dstX = srcX;
                int16_t dstY = srcY;
                transformPixel(&dstX, &dstY, xOff, yOff, flipLR, flipUD,
                               rotateDeg, wsg->w, wsg->h);
                // Check bounds
                if(0 <= dstX && dstX < disp->w && 0 <= dstY && dstY <= disp->h)
                {
                    // Draw the pixel
                    disp->setPx(dstX, dstY, wsg->px[(srcY * wsg->w) + srcX]);
                }
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
bool loadFont(const char* name, font_t* font)
{
    // Read font from file
    uint8_t* buf = NULL;
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
        font_ch_t* this = &font->chars[ch - ' '];

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
void drawChar(display_t* disp, paletteColor_t color, uint16_t h, font_ch_t* ch, int16_t xOff, int16_t yOff)
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
            if(8 == bitIdx)
            {
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
int16_t drawText(display_t* disp, font_t* font, paletteColor_t color, const char* text, int16_t xOff, int16_t yOff)
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
uint16_t textWidth(font_t* font, const char* text)
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
 * @return paletteColor_t The output RGB
 */
paletteColor_t hsv2rgb(uint16_t h, float s, float v)
{
    // TODO FIX THIS!!!
    return c232;
    // float hh, p, q, t, ff;
    // uint16_t i;
    // paletteColor_t px = {.a=PX_OPAQUE};

    // hh = (h % 360) / 60.0f;
    // i = (uint16_t)hh;
    // ff = hh - i;
    // p = v * (1.0 - s);
    // q = v * (1.0 - (s * ff));
    // t = v * (1.0 - (s * (1.0 - ff)));

    // switch (i)
    // {
    //     case 0:
    //     {
    //         px.r = v * 0x1F;
    //         px.g = t * 0x1F;
    //         px.b = p * 0x1F;
    //         break;
    //     }
    //     case 1:
    //     {
    //         px.r = q * 0x1F;
    //         px.g = v * 0x1F;
    //         px.b = p * 0x1F;
    //         break;
    //     }
    //     case 2:
    //     {
    //         px.r = p * 0x1F;
    //         px.g = v * 0x1F;
    //         px.b = t * 0x1F;
    //         break;
    //     }
    //     case 3:
    //     {
    //         px.r = p * 0x1F;
    //         px.g = q * 0x1F;
    //         px.b = v * 0x1F;
    //         break;
    //     }
    //     case 4:
    //     {
    //         px.r = t * 0x1F;
    //         px.g = p * 0x1F;
    //         px.b = v * 0x1F;
    //         break;
    //     }
    //     case 5:
    //     default:
    //     {
    //         px.r = v * 0x1F;
    //         px.g = p * 0x1F;
    //         px.b = q * 0x1F;
    //         break;
    //     }
    // }
    // return px;
}
