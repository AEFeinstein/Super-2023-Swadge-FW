//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include <string.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include "display.h"
#include "bresenham.h"
#include "heatshrink_decoder.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

//==============================================================================
// Defines
//==============================================================================

#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))
#define SLANT(font) ((font->h + 3) / 4)

//==============================================================================
// Constant data
//==============================================================================

const int16_t sin1024[360] =
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

// Only need 180 degrees because of symmetry.
// One note: Originally, this table was 1024, I saw no ill effects when
// shrinking it so I kept it that way.
const uint16_t tan1024[91] =
{
    0, 18, 36, 54, 72, 90, 108, 126, 144, 162, 181, 199, 218, 236, 255, 274, 294,
    313, 333, 353, 373, 393, 414, 435, 456, 477, 499, 522, 544, 568, 591, 615,
    640, 665, 691, 717, 744, 772, 800, 829, 859, 890, 922, 955, 989, 1024, 1060,
    1098, 1137, 1178, 1220, 1265, 1311, 1359, 1409, 1462, 1518, 1577, 1639, 1704,
    1774, 1847, 1926, 2010, 2100, 2196, 2300, 2412, 2534, 2668, 2813, 2974,
    3152, 3349, 3571, 3822, 4107, 4435, 4818, 5268, 5807, 6465, 7286, 8340, 9743,
    11704, 14644, 19539, 29324, 58665, 65535,
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Integer sine function
 *
 * @param degree The degree, between 0 and 359
 * @return int16_t The sine of the degree, between -1024 and 1024
 */
int16_t getSin1024(int16_t degree)
{
    degree = ( (degree % 360) + 360 ) % 360;
    return sin1024[degree];
}

/**
 * Integer cosine function
 *
 * @param degree The degree, between 0 and 359
 * @return int16_t The cosine of the degree, between -1024 and 1024
 */
int16_t getCos1024(int16_t degree)
{
    // cos is just sin offset by 90 degrees
    degree = ( (degree % 360) + 450 ) % 360;
    return sin1024[degree];
}

/**
 * Integer tangent function
 *
 * @param degree The degree, between 0 and 359
 * @return int16_t The tangent of the degree, between -1024 and 1024
 */
int32_t getTan1024(int16_t degree)
{
    // Force always positive modulus math.
    degree = ( ( degree % 180 ) + 180 ) % 180;
    if( degree < 90 )
    {
        return tan1024[degree];
    }
    else
    {
        return -tan1024[degree - 90];
    }
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
void fillDisplayArea(display_t* disp, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, paletteColor_t c)
{
    // Note: int16_t vs int data types tested for speed.
    //  This function has been micro optimized by cnlohr on 2022-09-07, using gcc version 8.4.0 (crosstool-NG esp-2021r2-patch3)

    // Only draw on the display
    int xMin = CLAMP(x1, 0, disp->w);
    int xMax = CLAMP(x2, 0, disp->w);
    int yMin = CLAMP(y1, 0, disp->h);
    int yMax = CLAMP(y2, 0, disp->h);

    uint32_t dw = disp->w;
    {
        paletteColor_t* pxs = disp->pxFb + yMin * dw + xMin;

        int copyLen = xMax - xMin;

        // Set each pixel
        for (int y = yMin; y < yMax; y++)
        {
            memset( pxs, c, copyLen );
            pxs += dw;
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
    return loadWsgSpiRam(name, wsg, false);
}

/**
 * @brief Load a WSG from ROM to RAM. WSGs placed in the spiffs_image folder
 * before compilation will be automatically flashed to ROM
 *
 * @param name The filename of the WSG to load
 * @param wsg  A handle to load the WSG to
 * @param spiRam true to load to SPI RAM, false to load to normal RAM
 * @return true if the WSG was loaded successfully,
 *         false if the WSG load failed and should not be used
 */
bool loadWsgSpiRam(char* name, wsg_t* wsg, bool spiRam)
{
    // Read WSG from file
    uint8_t* buf = NULL;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz, true))
    {
        ESP_LOGE("WSG", "Failed to read %s", name);
        return false;
    }

    // Pick out the decompresed size and create a space for it
    uint32_t decompressedSize = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
    uint8_t * decompressedBuf = (uint8_t *)heap_caps_malloc(decompressedSize, MALLOC_CAP_SPIRAM);

    // Create the decoder
    size_t copied = 0;
    heatshrink_decoder* hsd = heatshrink_decoder_alloc(256, 8, 4);
    heatshrink_decoder_reset(hsd);

    // Decode the file in chunks
    uint32_t inputIdx = 0;
    uint32_t outputIdx = 0;
    while(inputIdx < (sz - 4))
    {
        // Decode some data
        copied = 0;
        heatshrink_decoder_sink(hsd, &buf[4 + inputIdx], sz - 4 - inputIdx, &copied);
        inputIdx += copied;

        // Save it to the output array
        copied = 0;
        heatshrink_decoder_poll(hsd, &decompressedBuf[outputIdx], decompressedSize - outputIdx, &copied);
        outputIdx += copied;
    }

    // Note that it's all done
    heatshrink_decoder_finish(hsd);

    // Flush any final output
    copied = 0;
    heatshrink_decoder_poll(hsd, &decompressedBuf[outputIdx], decompressedSize - outputIdx, &copied);
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
    if(spiRam)
    {
        wsg->px = (paletteColor_t*)heap_caps_malloc(sizeof(paletteColor_t) * wsg->w * wsg->h, MALLOC_CAP_SPIRAM);
    }
    else
    {
        wsg->px = (paletteColor_t*)malloc(sizeof(paletteColor_t) * wsg->w * wsg->h);
    }

    if(NULL != wsg->px)
    {
        memcpy(wsg->px, &decompressedBuf[4], outputIdx - 4);
        free(decompressedBuf);
        return true;
    }

    // all done
    free(decompressedBuf);
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
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 * @param width  The width of the image
 * @param height The height of the image
 */
static void rotatePixel(int16_t* x, int16_t* y, int16_t rotateDeg, int16_t width, int16_t height)
{
    //  This function has been micro optimized by cnlohr on 2022-09-07, using gcc version 8.4.0 (crosstool-NG esp-2021r2-patch3)

    int32_t wx = *x;
    int32_t wy = *y;
    // First rotate the sprite around the sprite's center point

    // This solves the aliasing problem, but because of tan() it's only safe
    // to rotate by 0 to 90 degrees. So rotate by a multiple of 90 degrees
    // first, which doesn't need trig, then rotate the rest with shears
    // See http://datagenetics.com/blog/august32013/index.html
    // See https://graphicsinterface.org/wp-content/uploads/gi1986-15.pdf

    int32_t tx = -(width / 2);
    int32_t ty = -(height / 2);

    // Center around (0, 0)
    wx += tx;
    wy += ty;

    // First rotate to the nearest 90 degree boundary, which is trivial
    if(rotateDeg >= 270)
    {
        // (x, y) -> (y, -x)
        int16_t tmp = wx;
        wx = wy;
        wy = 2 * tx - tmp + width - 1;
        rotateDeg -= 270;
    }
    else if(rotateDeg >= 180)
    {
        // (x, y) -> (-x, -y)
        wx = 2 * tx - wx + width - 1;
        wy = 2 * ty - wy + height - 1;
        rotateDeg -= 180;
    }
    else if(rotateDeg >= 90)
    {
        // (x, y) -> (-y, x)
        int16_t tmp = wx;
        wx = 2 * ty - wy + height - 1;
        wy = tmp;
        rotateDeg -= 90;
    }
    // Now that it's rotated to a 90 degree boundary, find out how much more
    // there is to rotate by shearing

    // If there's any more to rotate, apply three shear matrices in order
    // if(rotateDeg > 1 && rotateDeg < 89)

    if(rotateDeg > 0)
    {
        // 1st shear
        wx = wx - ((wy * tan1024[rotateDeg / 2]) + 512) / 1024;
        // 2nd shear
        wy = ((wx * sin1024[rotateDeg]) + 512) / 1024 + wy;
        // 3rd shear
        wx = wx - ((wy * tan1024[rotateDeg / 2]) + 512) / 1024;
    }

    // Return pixel to original position
    wx -= tx;
    wy -= ty;

    *x = wx;
    *y = wy;
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
void drawWsg(display_t* disp, const wsg_t* wsg, int16_t xOff, int16_t yOff,
             bool flipLR, bool flipUD, int16_t rotateDeg)
{
    //  This function has been micro optimized by cnlohr on 2022-09-08, using gcc version 8.4.0 (crosstool-NG esp-2021r2-patch3)

    /* Btw quick test code for second case is:
    for( int mode = 0; mode < 8; mode++ )
    {
        drawWsgLocal( disp, &example_sprite, 50+mode*20, (global_i%20)-10, !!(mode&1), !!(mode & 2), (mode & 4)*10);
        drawWsgLocal( disp, &example_sprite, 50+mode*20, (global_i%20)+230, !!(mode&1), !!(mode & 2), (mode & 4)*10);
        drawWsgLocal( disp, &example_sprite, (global_i%20)-10, 50+mode*20, !!(mode&1), !!(mode & 2), (mode & 4)*10);
        drawWsgLocal( disp, &example_sprite, (global_i%20)+270, 50+mode*20, !!(mode&1), !!(mode & 2), (mode & 4)*10);
    }
    */

    if(NULL == wsg->px)
    {
        return;
    }

    if(rotateDeg)
    {
        SETUP_FOR_TURBO( disp );
        uint32_t wsgw = wsg->w;
        uint32_t wsgh = wsg->h;
        for(int32_t srcY = 0; srcY < wsgh; srcY++)
        {
            int32_t usey = srcY;

            // Reflect over X axis?
            if(flipUD)
            {
                usey = wsg->h - 1 - usey;
            }

            const paletteColor_t* linein = &wsg->px[usey * wsgw];

            // Reflect over Y axis?
            uint32_t readX = 0;
            uint32_t advanceX = 1;
            if (flipLR)
            {
                readX = wsgw;
                advanceX = -1;
            }

            int32_t localX = 0;
            for(int32_t srcX = 0; srcX != wsgw; srcX++)
            {
                // Draw if not transparent
                uint8_t color = linein[readX];
                if (cTransparent != color)
                {
                    uint16_t tx = localX;
                    uint16_t ty = srcY;

                    rotatePixel((int16_t*)&tx, (int16_t*)&ty, rotateDeg, wsgw, wsgh );
                    tx += xOff;
                    ty += yOff;
                    TURBO_SET_PIXEL_BOUNDS( disp, tx, ty, color );
                }
                localX++;
                readX += advanceX;
            }
        }
    }
    else
    {
        // Draw the image's pixels (no rotation or transformation)
        uint32_t w = disp->w;
        paletteColor_t* px = disp->pxFb;

        uint16_t wsgw = wsg->w;
        uint16_t wsgh = wsg->h;

        int32_t xstart = 0;
        int16_t xend = wsgw;
        int32_t xinc = 1;

        // Reflect over Y axis?
        if (flipLR)
        {
            xstart = wsgw - 1;
            xend = -1;
            xinc = -1;
        }

        if( xOff < 0 )
        {
            if( xinc > 0 )
            {
                xstart -= xOff;
                if( xstart >= xend )
                {
                    return;
                }
            }
            else
            {
                xstart += xOff;
                if( xend >= xstart )
                {
                    return;
                }
            }
            xOff = 0;
        }

        if( xOff + wsgw > w )
        {
            int32_t peelBack = (xOff + wsgw) - w;
            if( xinc > 0 )
            {
                xend -= peelBack;
                if( xstart >= xend )
                {
                    return;
                }
            }
            else
            {
                xend += peelBack;
                if( xend >= xstart )
                {
                    return;
                }
            }
        }


        for(int16_t srcY = 0; srcY < wsgh; srcY++)
        {
            int32_t usey = srcY;

            // Reflect over X axis?
            if(flipUD)
            {
                usey = wsgh - 1 - usey;
            }

            const paletteColor_t* linein = &wsg->px[usey * wsgw];

            // Transform this pixel's draw location as necessary
            uint32_t dstY = srcY + yOff;

            // It is too complicated to detect both directions and backoff correctly, so we just do this here.
            // It does slow things down a "tiny" bit.  People in the future could optimze out this check.
            if( dstY >= disp->h )
            {
                continue;
            }

            int32_t lineOffset = dstY * w;
            int32_t dstx = xOff + lineOffset;

            for(int32_t srcX = xstart; srcX != xend; srcX += xinc)
            {
                // Draw if not transparent
                uint8_t color = linein[srcX];
                if (cTransparent != color)
                {
                    px[dstx] = color;
                }
                dstx++;
            }
        }
    }
}

/**
 * @brief Draw a WSG to the display
 *
 * @param disp The display to draw the WSG to
 * @param wsg  The WSG to draw to the display
 * @param xOff The x offset to draw the WSG at
 * @param yOff The y offset to draw the WSG at
 */
void drawWsgSimpleFast(display_t* disp, const wsg_t* wsg, int16_t xOff, int16_t yOff)
{
    //  This function has been micro optimized by cnlohr on 2022-09-07, using gcc version 8.4.0 (crosstool-NG esp-2021r2-patch3)

    if(NULL == wsg->px)
    {
        return;
    }

    // Only draw in bounds
    int dWidth = disp->w;
    int wWidth = wsg->w;
    int xMin = CLAMP(xOff, 0, dWidth);
    int xMax = CLAMP(xOff + wWidth, 0, dWidth);
    int yMin = CLAMP(yOff, 0, disp->h);
    int yMax = CLAMP(yOff + wsg->h, 0, disp->h);
    paletteColor_t* px = disp->pxFb;
    int numX = xMax - xMin;
    int wsgY = (yMin - yOff);
    int wsgX = (xMin - xOff);
    paletteColor_t* lineout = &px[(yMin * dWidth) + xMin];
    const paletteColor_t* linein = &wsg->px[wsgY * wWidth + wsgX];

    // Draw each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = 0; x < numX; x++)
        {
            int color = linein[x];
            if( color != cTransparent )
            {
                lineout[x] = color;
            }
        }
        lineout += dWidth;
        linein += wWidth;
        wsgY++;
    }
}

/**
 * Quickly copy bytes into the framebuffer. This ignores transparency
 *
 * @param disp The display to draw the WSG to
 * @param wsg  The WSG to draw to the display
 * @param xOff The x offset to draw the WSG at
 * @param yOff The y offset to draw the WSG at
 */
void drawWsgTile(display_t* disp, const wsg_t* wsg, int32_t xOff, int32_t yOff)
{
    // Check if there is framebuffer access
    {
        if(xOff > disp->w)
        {
            return;
        }

        // Bound in the Y direction
        int32_t yStart = (yOff < 0) ? 0 : yOff;
        int32_t yEnd   = ((yOff + wsg->h) > disp->h) ? disp->h : (yOff + wsg->h);

        int wWidth = wsg->w;
        int dWidth = disp->w;
        const paletteColor_t* pxWsg = &wsg->px[(yOff < 0) ? (wsg->h - (yEnd - yStart)) * wWidth : 0];
        paletteColor_t* pxDisp = &disp->pxFb[yStart * dWidth + xOff];

        // Bound in the X direction
        int32_t copyLen = wsg->w;
        if(xOff < 0)
        {
            copyLen += xOff;
            pxDisp -= xOff;
            pxWsg -= xOff;
            xOff = 0;
        }

        if(xOff + copyLen > disp->w)
        {
            copyLen = disp->w - xOff;
        }


        // copy each row
        for(int32_t y = yStart; y < yEnd; y++)
        {
            // Copy the row
            // TODO probably faster if we can guarantee copyLen is a multiple of 4
            memcpy(pxDisp, pxWsg, copyLen);
            pxDisp += dWidth;
            pxWsg += wWidth;
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
        plotLine(disp, xStart, yOff + font->h / 2, xOff, yOff + font->h / 2, color, 0);
    }

    if (textAttrs & TEXT_UNDERLINE)
    {
        plotLine(disp, xStart, yOff + font->h + 1, xOff, yOff + font->h + 1, color, 0);
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

static const char* drawTextWordWrapInner(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                             int16_t *xOff, int16_t *yOff, int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax, uint8_t textAttrs)
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

    // while there is text left to print, and the text would not exceed the Y-bounds...
    // Subtract 1 from the line height to account for the space we don't care about
    while (*textPtr && (textY + lineHeight - 1 <= yMax))
    {
        *yOff = textY;

        // skip leading spaces if we're at the start of the line
        for (; textX == xMin && *textPtr == ' '; textPtr++);

        // handle newlines
        if (*textPtr == '\n')
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
        if (breakPtr == NULL)
        {
            breakPtr = textPtr + bufStrlen;
        }
        else if (breakPtr - textPtr > bufStrlen)
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
    return *textPtr ? textPtr : NULL;
}


/// @brief Draws text, breaking on word boundaries, until the given bounds are filled or all text is drawn.
///
/// Text will be drawn, starting at `(xOff, yOff)`, wrapping to the next line at ' ' or '-' when the next
/// word would exceed `xMax`, or immediately when a newline ('\n') is encountered. Carriage returns and
/// tabs ('\r', '\t') are not supported. When the bottom of the next character would exceed `yMax`, no more
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
    return drawTextWordWrapInner(disp, font, color, text, xOff, yOff, *xOff, *yOff, xMax, yMax, TEXT_NORMAL);
}

const char* drawTextWordWrapExtra(display_t* disp, const font_t* font, paletteColor_t color, const char* text,
                                  int16_t *xOff, int16_t *yOff, int16_t xMin, int16_t yMin,int16_t xMax, int16_t yMax,
                                  uint8_t textAttrs)
{
    return drawTextWordWrapInner(disp, font, color, text, xOff, yOff, xMin, yMin, xMax, yMax, textAttrs);
}

uint16_t textLineHeight(const font_t* font, uint8_t textAttrs)
{
    return font->h + ((textAttrs & TEXT_BOLD) ? 1 : 0) + ((textAttrs & TEXT_UNDERLINE) ? 2 : 0) + 1;
}

uint16_t textHeightAttrs(const font_t* font, const char* text, int16_t startX, int16_t startY, int16_t width, int16_t maxHeight, uint8_t textAttrs)
{
    int16_t xEnd = startX;
    int16_t yEnd = startY;
    drawTextWordWrapInner(NULL, font, cTransparent, text, &xEnd, &yEnd, 0, 0, width, maxHeight, TEXT_NORMAL);
    return yEnd + textLineHeight(font, textAttrs);
}

uint16_t textHeight(const font_t* font, const char* text, int16_t width, int16_t maxHeight)
{
    return textHeightAttrs(font, text, 0, 0, width, maxHeight, TEXT_NORMAL);
}