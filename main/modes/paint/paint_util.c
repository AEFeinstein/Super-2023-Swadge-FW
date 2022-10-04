#include "paint_util.h"

#include "paint_common.h"

#include "bresenham.h"

paletteColor_t getContrastingColor(paletteColor_t col)
{
    uint32_t rgb = paletteToRGB(col);

    // TODO I guess this actually won't work at all on 50% gray, or that well on other grays
    uint8_t r = 255 - (rgb & 0xFF);
    uint8_t g = 255 - ((rgb >> 8) & 0xFF);
    uint8_t b = 255 - ((rgb >> 16) & 0xFF);
    uint32_t contrastCol = (r << 16) | (g << 8) | (b);

    PAINT_LOGV("Converted color to RGB c%d%d%d", r, g, b);
    return RGBtoPalette(contrastCol);
}

void paintPlotSquareWave(display_t* disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    // swap so x0 < x1 && y0 < y1
    if (x0 > x1)
    {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
    }

    if (y0 > y1)
    {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
    }

    // use the shortest axis as the wave size
    uint16_t waveSize = (x1 - x0 < y1 - y0) ? x1 - x0 : y1 - y0;

    uint16_t x = x0;
    uint16_t y = y0;
    uint16_t stop, extra;

    int16_t xDir = (x0 < x1) ? 1 : -1;
    int16_t yDir = (y0 < y1) ? 1 : -1;

    if (waveSize < 2)
    {
        // TODO: Draw a rectangular wave if a real square wave would be too small?
        // (a 2xN square wave is just a 2-thick line)
        return;
    }

    if (x1 - x0 > y1 - y0)
    {
        // Horizontal
        extra = (x1 - x0 + 1) % (waveSize * 2);
        stop = x + waveSize * xDir - extra / 2;

        while (x != x1)
        {
            PAINT_LOGV("Squarewave H -- (%d, %d)", x, y);
            setPxScaled(disp, x, y, col, xTr, yTr, xScale, yScale);

            if (x == stop)
            {
                plotLineScaled(disp, x, y, x, y + yDir * waveSize, col, 0, xTr, yTr, xScale, yScale);
                y += yDir * waveSize;
                yDir = -yDir;
                stop = x + waveSize * xDir;
            }

            x+= xDir;
        }
    }
    else
    {
        // Vertical
        extra = (x1 - x0 + 1) % (waveSize * 2);
        stop = y + waveSize * yDir - extra / 2;

        while (y != y1)
        {
            PAINT_LOGV("Squarewave V -- (%d, %d)", x, y);
            setPxScaled(disp, x, y, col, xTr, yTr, xScale, yScale);

            if (y == stop)
            {
                plotLineScaled(disp, x, y, x + xDir * waveSize, y, col, 0, xTr, yTr, xScale, yScale);
                x += xDir * waveSize;
                xDir = -xDir;
                stop = y + waveSize * yDir;
            }

            y += yDir;
        }
    }
}

void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col)
{
    if (x0 >= x1 || y0 >= y1)
    {
        PAINT_LOGE("Attempted to plot invalid rect plotRectFilled(%d, %d, %d, %d). Returning to avoid segfault", x0, y0, x1, y1);
        return;
    }

    fillDisplayArea(disp, x0, y0, x1 - 1, y1 - 1, col);
}

void plotRectFilledScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    fillDisplayArea(disp, xTr + x0 * xScale, yTr + y0 * yScale, xTr + (x1) * xScale, yTr + (y1) * yScale, col);
}

void setPxScaled(display_t* disp, int x, int y, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    plotRectFilledScaled(disp, x, y, x + 1, y + 1, col, xTr, yTr, xScale, yScale);
}

void paintDrawWsgTemp(display_t* disp, const wsg_t* wsg, pxStack_t* saveTo, uint16_t xOffset, uint16_t yOffset, colorMapFn_t colorSwap)
{
    size_t i = 0;
    for (uint16_t x = 0; x < wsg->w; x++)
    {
        for (uint16_t y = 0; y < wsg->h; y++, i++)
        {
            if (wsg->px[i] != cTransparent)
            {
                pushPx(saveTo, disp, xOffset + x, yOffset + y);

                disp->setPx(xOffset + x, yOffset + y, colorSwap ? colorSwap(disp->getPx(xOffset + x, yOffset + y)) : wsg->px[i]);
            }
            else
            {
                PAINT_LOGV("Skipping cursor[%d][%d] == cTransparent", x, y);
            }
        }
    }
}

// Calculate the maximum possible [square] scale, given the display's dimensions and the image dimensions, plus any margins required
uint8_t paintGetMaxScale(display_t* disp, uint16_t imgW, uint16_t imgH, uint16_t xMargin, uint16_t yMargin)
{
    uint16_t maxW = disp->w - xMargin;
    uint16_t maxH = disp->h - yMargin;

    uint8_t scale = 1;

    while (imgW * (scale + 1) <= maxW && imgH * (scale + 1) <= maxH)
    {
        scale++;
    }

    return scale;
}
