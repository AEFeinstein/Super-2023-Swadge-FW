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

    return RGBtoPalette(contrastCol);
}

paletteColor_t getContrastingColorBW(paletteColor_t col)
{
    uint32_t rgb = paletteToRGB(col);
    uint8_t r = rgb & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = (rgb >> 16) & 0xFF;

    // TODO something with HSL but this pretty much works...
    return (r + g + b) / 3 > 76 ? c000 : c555;
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

void paintColorReplace(paintCanvas_t* canvas, paletteColor_t search, paletteColor_t replace)
{
    // super inefficient dumb color replace, maybe do iterated color fill later?
    for (uint8_t x = 0; x < canvas->w; x++)
    {
        for (uint8_t y = 0; y < canvas->h; y++)
        {
            if (canvas->disp->getPx(canvas->x + x * canvas->xScale, canvas->y + y * canvas->yScale) == search)
            {
                setPxScaled(canvas->disp, x, y, replace, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
            }
        }
    }
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

/// @brief Writes the points of a pxStack_t into the given point_t array.
/// @param pxStack The pxStack_t to be converted
/// @param dest A pointer to an array of point_t. Must have room for at least pxStack->index entries.
void paintConvertPickPoints(const pxStack_t* pxStack, point_t* dest)
{
    for (size_t i = 0; i < pxStack->index; i++)
    {
        dest[i].x = pxStack->data[i].x;
        dest[i].y = pxStack->data[i].y;
    }
}

/// @brief Writes the points of a pxStack_t into the given point_t array, converting them to canvas coordinates
/// @param pxStack The pxStack_t to be converted
/// @param canvas The canvas whose coordinates they should be changed back to
/// @param dest A pointer to an array of point_t. Must have room for at least pxStackSize(pxStack)
void paintConvertPickPointsScaled(const pxStack_t* pxStack, paintCanvas_t* canvas, point_t* dest)
{
    for (size_t i = 0; i < pxStackSize(pxStack); i++)
    {
        dest[i].x = (pxStack->data[i].x - canvas->x) / canvas->xScale;
        dest[i].y = (pxStack->data[i].y - canvas->y) / canvas->yScale;
    }
}

uint16_t canvasToDispX(const paintCanvas_t* canvas, uint16_t x)
{
    return canvas->x + x * canvas->xScale;
}

uint16_t canvasToDispY(const paintCanvas_t* canvas, uint16_t y)
{
    return canvas->y + y * canvas->yScale;
}

void swap(uint8_t* a, uint8_t* b)
{
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
}
