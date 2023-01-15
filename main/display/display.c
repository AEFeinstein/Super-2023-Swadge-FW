//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include <string.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include "display.h"
#include "heatshrink_decoder.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

//==============================================================================
// Defines
//==============================================================================

#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))
#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

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
