#include "paint_common.h"

#include "display.h"

paintMenu_t* paintState;


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
