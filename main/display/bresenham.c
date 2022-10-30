/********************************************************************
 *                                                                   *
 *                    Curve Rasterizing Algorithm                    *
 *                                                                   *
 ********************************************************************/

/**
 * @author Zingl Alois
 * @date 22.08.2016
 * @version 1.2
 *
 * http://members.chello.at/~easyfilter/bresenham.html
 * http://members.chello.at/~easyfilter/bresenham.c
 */

#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "bresenham.h"


// Internal functions
void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax);
void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax);
void plotLineInner(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int dashWidth,int xTr, int yTr, int xScale, int yScale);
void plotRectInner(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotEllipseInner(display_t*, int xm, int ym, int a, int b, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotCircleInner(display_t*, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotCircleFilledInner(display_t* disp, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotEllipseRectInner(display_t*, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadBezierSegInner(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadBezierInner(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadRationalBezierSegInner(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, float w, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadRationalBezierInner(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, float w, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotCubicBezierSegInner(display_t*, int x0, int y0, float x1, float y1, float x2, float y2, int x3, int y3,
                        paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotCubicBezierInner(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);


// #define assert(x) if(false == (x)) {  return;  }

/**
 * Attempt to fill a shape bounded by a one-pixel border of a given color using
 * the even-odd rule:
 * https://en.wikipedia.org/wiki/Even%E2%80%93odd_rule
 *
 * WARNING!!! This is very finicky and is not guaranteed to work in all cases.
 *
 * This iterates over each row in the bounding box, top to bottom, left to right
 *
 * This assumes that each row starts outside or on the boundary of the shape to
 * be filled.
 *
 * Each time a pixel of the 'boundary color' is iterated over, the in/out
 * boolean will flip, no matter what. This means that stray pixels in the shape
 * can flip the in/out state, and thick boundaries with an even number of pixels
 * will also mess it up.
 *
 * @param disp The display to read from and draw to
 * @param x0 The left index of the bounding box
 * @param y0 The top index of the bounding box
 * @param x1 The right index of the bounding box
 * @param y1 The bottom index of the bounding box
 * @param boundaryColor The color of the boundary to fill in
 * @param fillColor The color to fill
 */
void oddEvenFill(display_t* disp, int x0, int y0, int x1, int y1,
                 paletteColor_t boundaryColor, paletteColor_t fillColor)
{
    SETUP_FOR_TURBO( disp );

    // Adjust the bounding box if it's out of bounds
    if(x0 < 0)
    {
        x0 = 0;
    }
    if(x1 > disp->w)
    {
        x1 = disp->w;
    }
    if(y0 < 0)
    {
        y0 = 0;
    }
    if(y1 > disp->h)
    {
        y1 = disp->h;
    }

    // Iterate over the bounding box
    for(int y = y0; y < y1; y++)
    {
        // Assume starting outside the shape for each row
        bool isInside = false;
        for(int x = x0; x < x1; x++)
        {
            // If a boundary is hit
            if(boundaryColor == GET_PIXEL(disp, x, y))
            {
                // Flip this boolean, don't color the boundary
                isInside = !isInside;
            }
            else if(isInside)
            {
                // If we're in-bounds, color the pixel
                TURBO_SET_PIXEL_BOUNDS(disp, x, y, fillColor);
            }
        }
    }
}

// adapted from http://www.adammil.net/blog/v126_A_More_Efficient_Flood_Fill.html
void floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t col, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax)
{
    if (disp->getPx(x, y) == col)
    {
        // makes no sense to fill with the same color, so just don't
        return;
    }

    _floodFill(disp, x, y, disp->getPx(x, y), col, xMin, yMin, xMax, yMax);
}

void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax)
{
    // at this point, we know array[y,x] is clear, and we want to move as far as possible to the upper-left. moving
    // up is much more important than moving left, so we could try to make this smarter by sometimes moving to
    // the right if doing so would allow us to move further up, but it doesn't seem worth the complexity
    while(true)
    {
        uint16_t ox = x, oy = y;
        while(y != yMin && disp->getPx(x, y-1) == search) y--;
        while(x != xMin && disp->getPx(x-1, y) == search) x--;
        if(x == ox && y == oy) break;
    }
    _floodFillInner(disp, x, y, search, fill, xMin, yMin, xMax, yMax);
}


void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax)
{
    // at this point, we know that array[y,x] is clear, and array[y-1,x] and array[y,x-1] are set.
    // we'll begin scanning down and to the right, attempting to fill an entire rectangular block
    uint16_t lastRowLength = 0; // the number of cells that were clear in the last row we scanned
    do
    {
        uint16_t rowLength = 0, sx = x; // keep track of how long this row is. sx is the starting x for the main scan below
        // now we want to handle a case like |***|, where we fill 3 cells in the first row and then after we move to
        // the second row we find the first  | **| cell is filled, ending our rectangular scan. rather than handling
        // this via the recursion below, we'll increase the starting value of 'x' and reduce the last row length to
        // match. then we'll continue trying to set the narrower rectangular block
        if(lastRowLength != 0 && disp->getPx(x, y) != search) // if this is not the first row and the leftmost cell is filled...
        {
            do
            {
                if(--lastRowLength == 0) return; // shorten the row. if it's full, we're done
            } while(disp->getPx(++x, y) != search); // otherwise, update the starting point of the main scan to match
            sx = x;
        }
        // we also want to handle the opposite case, | **|, where we begin scanning a 2-wide rectangular block and
        // then find on the next row that it has     |***| gotten wider on the left. again, we could handle this
        // with recursion but we'd prefer to adjust x and lastRowLength instead
        else
        {
            for(; x != xMin && disp->getPx(x-1, y) == search; rowLength++, lastRowLength++)
            {
                disp->setPx(--x, y, fill); // to avoid scanning the cells twice, we'll fill them and update rowLength here
                // if there's something above the new starting point, handle that recursively. this deals with cases
                // like |* **| when we begin filling from (2,0), move down to (2,1), and then move left to (0,1).
                // the  |****| main scan assumes the portion of the previous row from x to x+lastRowLength has already
                // been filled. adjusting x and lastRowLength breaks that assumption in this case, so we must fix it
                if(y != yMin && disp->getPx(x, y-1) == search) _floodFill(disp, x, y-1, search, fill, xMin, yMin, xMax, yMax); // use _Fill since there may be more up and left
            }
        }

        // now at this point we can begin to scan the current row in the rectangular block. the span of the previous
        // row from x (inclusive) to x+lastRowLength (exclusive) has already been filled, so we don't need to
        // check it. so scan across to the right in the current row
        for(; sx < xMax && disp->getPx(sx, y) == search; rowLength++, sx++) disp->setPx(sx, y, fill);
        // now we've scanned this row. if the block is rectangular, then the previous row has already been scanned,
        // so we don't need to look upwards and we're going to scan the next row in the next iteration so we don't
        // need to look downwards. however, if the block is not rectangular, we may need to look upwards or rightwards
        // for some portion of the row. if this row was shorter than the last row, we may need to look rightwards near
        // the end, as in the case of |*****|, where the first row is 5 cells long and the second row is 3 cells long.
        // we must look to the right  |*** *| of the single cell at the end of the second row, i.e. at (4,1)
        if(rowLength < lastRowLength)
        {
            for(int end=x+lastRowLength; ++sx < end; ) // 'end' is the end of the previous row, so scan the current row to
            {   // there. any clear cells would have been connected to the previous
                if(disp->getPx(sx, y) == search) _floodFillInner(disp, sx, y, search, fill, xMin, yMin, xMax, yMax); // row. the cells up and left must be set so use FillCore
            }
        }
        // alternately, if this row is longer than the previous row, as in the case |*** *| then we must look above
        // the end of the row, i.e at (4,0)                                         |*****|
        else if(rowLength > lastRowLength && y != yMin) // if this row is longer and we're not already at the top...
        {
            for(int ux=x+lastRowLength; ++ux<sx; ) // sx is the end of the current row
            {
                if(disp->getPx(ux, y-1) == search) _floodFill(disp, ux, y-1, search, fill, xMin, yMin, xMax, yMax); // since there may be clear cells up and left, use _Fill
            }
        }
        lastRowLength = rowLength; // record the new row length
    } while(lastRowLength != 0 && ++y < yMax); // if we get to a full row or to the bottom, we're done
}


void plotLineInner(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int dashWidth, int xTr, int yTr, int xScale, int yScale)
{
    SETUP_FOR_TURBO( disp );
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */
    int dashCnt = 0;
    bool dashDraw = true;

    for (;;)   /* loop */
    {
        if(dashWidth)
        {
            if(dashDraw)
            {
                TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y0 * yScale, col);
            }
            dashCnt++;
            if(dashWidth == dashCnt)
            {
                dashCnt = 0;
                dashDraw = !dashDraw;
            }
        }
        else
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y0 * yScale, col);
        }
        e2 = 2 * err;
        if (e2 >= dy)   /* e_xy+e_x > 0 */
        {
            if (x0 == x1)
            {
                break;
            }
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)   /* e_xy+e_y < 0 */
        {
            if (y0 == y1)
            {
                break;
            }
            err += dx;
            y0 += sy;
        }
    }
}

void plotLine(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int dashWidth)
{
    plotLineInner(disp, x0, y0, x1, y1, col, dashWidth, 0, 0, 1, 1);
}

void plotLineScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int dashWidth, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale*yScale; i++)
    {
        plotLineInner(disp, x0, y0, x1, y1, col, dashWidth, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotRectInner(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    SETUP_FOR_TURBO( disp );

    // Vertical lines
    for(int y = y0; y < y1; y++)
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y * yScale, col);
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (x1 - 1) * xScale, yTr + y * yScale, col);
    }

    // Horizontal lines
    for(int x = x0; x < x1; x++)
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x * xScale, yTr + y0 * yScale, col);
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x * xScale, yTr + (y1 - 1) * yScale, col);
    }
}

void plotRect(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col)
{
    plotRectInner(disp, x0, y0, x1, y1, col, 0, 0, 1, 1);
}

void plotRectScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotRectInner(disp, x0, y0, x1, y1, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotEllipseInner(display_t* disp, int xm, int ym, int a, int b, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    SETUP_FOR_TURBO( disp );

    int x = -a, y = 0; /* II. quadrant from bottom left to top right */
    long e2 = (long) b * b, err = (long) x * (2 * e2 + x) + e2; /* error of 1.step */

    do
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm - x) * xScale, yTr + (ym + y) * yScale, col); /*   I. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm + x) * xScale, yTr + (ym + y) * yScale, col); /*  II. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm + x) * xScale, yTr + (ym - y) * yScale, col); /* III. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm - x) * xScale, yTr + (ym - y) * yScale, col); /*  IV. Quadrant */
        e2 = 2 * err;
        if (e2 >= (x * 2 + 1) * (long) b * b) /* e_xy+e_x > 0 */
        {
            err += (++x * 2 + 1) * (long) b * b;
        }
        if (e2 <= (y * 2 + 1) * (long) a * a) /* e_xy+e_y < 0 */
        {
            err += (++y * 2 + 1) * (long) a * a;
        }
    } while (x <= 0);

    while (y++ < b)   /* too early stop of flat ellipses a=1, */
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + xm * xScale, yTr + (ym + y) * yScale, col); /* -> finish tip of ellipse */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + xm * xScale, yTr + (ym - y) * yScale, col);
    }
}

void plotEllipse(display_t* disp, int xm, int ym, int a, int b, paletteColor_t col)
{
    plotEllipseInner(disp, xm, ym, a, b, col, 0, 0, 1, 1);
}

void plotEllipseScaled(display_t* disp, int xm, int ym, int a, int b, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotEllipseInner(disp, xm, ym, a, b, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotOptimizedEllipse(display_t* disp, int xm, int ym, int a, int b, paletteColor_t col)
{
    SETUP_FOR_TURBO( disp );

    long x = -a, y = 0; /* II. quadrant from bottom left to top right */
    long e2 = b, dx = (1 + 2 * x) * e2 * e2; /* error increment  */
    long dy = x * x, err = dx + dy; /* error of 1.step */

    do
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xm - x, ym + y, col); /*   I. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xm + x, ym + y, col); /*  II. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xm + x, ym - y, col); /* III. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xm - x, ym - y, col); /*  IV. Quadrant */
        e2 = 2 * err;
        if (e2 >= dx)
        {
            x++;
            err += dx += 2 * (long) b * b;
        } /* x step */
        if (e2 <= dy)
        {
            y++;
            err += dy += 2 * (long) a * a;
        } /* y step */
    } while (x <= 0);

    while (y++ < b)   /* too early stop for flat ellipses with a=1, */
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xm, ym + y, col); /* -> finish tip of ellipse */
        TURBO_SET_PIXEL_BOUNDS(disp, xm, ym - y, col);
    }
}

void plotCircleInner(display_t* disp, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    SETUP_FOR_TURBO( disp );

    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
		TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm - x) * xScale, yTr + (ym + y) * yScale, col); /*   I. Quadrant +x +y */
		TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm - y) * xScale, yTr + (ym - x) * yScale, col); /*  II. Quadrant -x +y */
		TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm + x) * xScale, yTr + (ym - y) * yScale, col); /* III. Quadrant -x -y */
		TURBO_SET_PIXEL_BOUNDS(disp, xTr + (xm + y) * xScale, yTr + (ym + x) * yScale, col); /*  IV. Quadrant +x -y */
        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1;    /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1;    /* -> x-step now */
        }
    } while (x < 0);
}

void plotCircle(display_t* disp, int xm, int ym, int r, paletteColor_t col)
{
    plotCircleInner(disp, xm, ym, r, col, 0, 0, 1, 1);
}

void plotCircleScaled(display_t* disp, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotCircleInner(disp, xm, ym, r, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotCircleQuadrants(display_t* disp, int xm, int ym, int r, bool q1,
                         bool q2, bool q3, bool q4, paletteColor_t col)
{
    SETUP_FOR_TURBO( disp );

    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
        if(q1)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xm - x, ym + y, col); /*   I. Quadrant +x +y */
        }
        if(q2)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xm - y, ym - x, col); /*  II. Quadrant -x +y */
        }
        if(q3)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xm + x, ym - y, col); /* III. Quadrant -x -y */
        }
        if(q4)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xm + y, ym + x, col); /*  IV. Quadrant +x -y */
        }
        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1;    /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1;    /* -> x-step now */
        }
    } while (x < 0);
}

void plotCircleFilledInner(display_t* disp, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    SETUP_FOR_TURBO( disp );

    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
        for (int lineX = xm + x; lineX <= xm - x; lineX++)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xTr + lineX * xScale, yTr + (ym - y) * yScale, col);
            TURBO_SET_PIXEL_BOUNDS(disp, xTr + lineX * xScale, yTr + (ym + y) * yScale, col);
        }

        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1;    /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1;    /* -> x-step now */
        }
    } while (x < 0);
}

void plotCircleFilled(display_t* disp, int xm, int ym, int r, paletteColor_t col)
{
    plotCircleFilledInner(disp, xm, ym, r, col, 0, 0, 1, 1);
}

void plotCircleFilledScaled(display_t* disp, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotCircleFilledInner(disp, xm, ym, r, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotEllipseRectInner(display_t* disp, int x0, int y0, int x1,
                     int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)   /* rectangular parameter enclosing the ellipse */
{
    SETUP_FOR_TURBO( disp );

    long a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1; /* diameter */
    double dx = 4 * (1.0 - a) * b * b, dy = 4 * (b1 + 1) * a * a; /* error increment */
    double err = dx + dy + b1 * a * a, e2; /* error of 1.step */

    if (x0 > x1)
    {
        x0 = x1;
        x1 += a;
    } /* if called with swapped points */
    if (y0 > y1)
    {
        y0 = y1;    /* .. exchange them */
    }
    y0 += (b + 1) / 2;
    y1 = y0 - b1; /* starting pixel */
    a = 8 * a * a;
    b1 = 8 * b * b;

    do
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x1 * xScale, yTr + y0 * yScale, col); /*   I. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y0 * yScale, col); /*  II. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y1 * yScale, col); /* III. Quadrant */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + x1 * xScale, yTr + y1 * yScale, col); /*  IV. Quadrant */
        e2 = 2 * err;
        if (e2 <= dy)
        {
            y0++;
            y1--;
            err += dy += a;
        } /* y step */
        if (e2 >= dx || 2 * err > dy)
        {
            x0++;
            x1--;
            err += dx += b1;
        } /* x step */
    } while (x0 <= x1);

    while (y0 - y1 <= b)   /* too early stop of flat ellipses a=1 */
    {
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (x0 - 1) * xScale, yTr + y0 * yScale, col); /* -> finish tip of ellipse */
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (x1 + 1) * xScale, yTr + y0++ * yScale, col);
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (x0 - 1) * xScale, yTr + y1 * yScale, col);
        TURBO_SET_PIXEL_BOUNDS(disp, xTr + (x1 + 1) * xScale, yTr + y1-- * yScale, col);
    }
}

void plotEllipseRect(display_t* disp, int x0, int y0, int x1,
                     int y1, paletteColor_t col)   /* rectangular parameter enclosing the ellipse */
{
    plotEllipseRectInner(disp, x0, y0, x1, y1, col, 0, 0, 1, 1);
}

void plotEllipseRectScaled(display_t* disp, int x0, int y0, int x1,
                     int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotEllipseRectInner(disp, x0, y0, x1, y1, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotQuadBezierSegInner(display_t* disp, int x0, int y0, int x1, int y1, int x2,
                       int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)   /* plot a limited quadratic Bezier segment */
{
    SETUP_FOR_TURBO( disp );

    int sx = x2 - x1, sy = y2 - y1;
    long xx = x0 - x1, yy = y0 - y1, xy; /* relative values for checks */
    double dx, dy, err, cur = xx * sy - yy * sx; /* curvature */

    assert(xx * sx <= 0 && yy * sy <= 0); /* sign of gradient must not change */

    if (sx * (long) sx + sy * (long) sy > xx * xx + yy * yy)   /* begin with longer part */
    {
        x2 = x0;
        x0 = sx + x1;
        y2 = y0;
        y0 = sy + y1;
        cur = -cur; /* swap P0 P2 */
    }
    if (cur != 0)   /* no straight line */
    {
        xx += sx;
        xx *= sx = x0 < x2 ? 1 : -1; /* x step direction */
        yy += sy;
        yy *= sy = y0 < y2 ? 1 : -1; /* y step direction */
        xy = 2 * xx * yy;
        xx *= xx;
        yy *= yy; /* differences 2nd degree */
        if (cur * sx * sy < 0)   /* negated curvature? */
        {
            xx = -xx;
            yy = -yy;
            xy = -xy;
            cur = -cur;
        }
        dx = 4.0 * sy * cur * (x1 - x0) + xx - xy; /* differences 1st degree */
        dy = 4.0 * sx * cur * (y0 - y1) + yy - xy;
        xx += xx;
        yy += yy;
        err = dx + dy + xy; /* error 1st step */
        do
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y0 * yScale, col); /* plot curve */
            if (x0 == x2 && y0 == y2)
            {
                return;    /* last pixel -> curve finished */
            }
            y1 = 2 * err < dx; /* save value for test of y step */
            if (2 * err > dy)
            {
                x0 += sx;
                dx -= xy;
                err += dy += yy;
            } /* x step */
            if (y1)
            {
                y0 += sy;
                dy -= xy;
                err += dx += xx;
            } /* y step */
        } while (dy < 0 && dx > 0); /* gradient negates -> algorithm fails */
    }
    plotLineScaled(disp, x0, y0, x2, y2, col, 0, xTr, yTr, xScale, yScale); /* plot remaining part to end */
}

void plotQuadBezierSeg(display_t* disp, int x0, int y0, int x1, int y1, int x2,
                       int y2, paletteColor_t col)   /* plot a limited quadratic Bezier segment */
{
    plotQuadBezierSegInner(disp, x0, y0, x1, y1, x2, y2, col, 0, 0, 1, 1);
}

void plotQuadBezierSegScaled(display_t* disp, int x0, int y0, int x1, int y1, int x2,
                       int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotQuadBezierSegInner(disp, x0, y0, x1, y1, x2, y2, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotQuadBezierInner(display_t* disp, int x0, int y0, int x1, int y1, int x2,
                    int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)   /* plot any quadratic Bezier curve */
{
    int x = x0 - x1, y = y0 - y1;
    double t = x0 - 2 * x1 + x2, r;

    if ((long) x * (x2 - x1) > 0)   /* horizontal cut at P4? */
    {
        if ((long) y * (y2 - y1) > 0) /* vertical cut at P6 too? */
            if (fabs((y0 - 2 * y1 + y2) / t * x) > abs(y))   /* which first? */
            {
                x0 = x2;
                x2 = x + x1;
                y0 = y2;
                y2 = y + y1; /* swap points */
            } /* now horizontal cut at P4 comes first */
        t = (x0 - x1) / t;
        r = (1 - t) * ((1 - t) * y0 + 2.0 * t * y1) + t * t * y2; /* By(t=P4) */
        t = (x0 * x2 - x1 * x1) * t / (x0 - x1); /* gradient dP4/dx=0 */
        x = floor(t + 0.5);
        y = floor(r + 0.5);
        r = (y1 - y0) * (t - x0) / (x1 - x0) + y0; /* intersect P3 | P0 P1 */
        plotQuadBezierSegInner(disp, x0, y0, x, floor(r + 0.5), x, y, col, xTr, yTr, xScale, yScale);
        r = (y1 - y2) * (t - x2) / (x1 - x2) + y2; /* intersect P4 | P1 P2 */
        x0 = x1 = x;
        y0 = y;
        y1 = floor(r + 0.5); /* P0 = P4, P1 = P8 */
    }
    if ((long) (y0 - y1) * (y2 - y1) > 0)   /* vertical cut at P6? */
    {
        t = y0 - 2 * y1 + y2;
        t = (y0 - y1) / t;
        r = (1 - t) * ((1 - t) * x0 + 2.0 * t * x1) + t * t * x2; /* Bx(t=P6) */
        t = (y0 * y2 - y1 * y1) * t / (y0 - y1); /* gradient dP6/dy=0 */
        x = floor(r + 0.5);
        y = floor(t + 0.5);
        r = (x1 - x0) * (t - y0) / (y1 - y0) + x0; /* intersect P6 | P0 P1 */
        plotQuadBezierSegInner(disp, x0, y0, floor(r + 0.5), y, x, y, col, xTr, yTr, xScale, yScale);
        r = (x1 - x2) * (t - y2) / (y1 - y2) + x2; /* intersect P7 | P1 P2 */
        x0 = x;
        x1 = floor(r + 0.5);
        y0 = y1 = y; /* P0 = P6, P1 = P7 */
    }
    plotQuadBezierSegInner(disp, x0, y0, x1, y1, x2, y2, col, xTr, yTr, xScale, yScale); /* remaining part */
}

void plotQuadBezier(display_t* disp, int x0, int y0, int x1, int y1, int x2,
                    int y2, paletteColor_t col)   /* plot any quadratic Bezier curve */
{
    plotQuadBezierInner(disp, x0, y0, x1, y1, x2, y2, col, 0, 0, 1, 1);
}

void plotQuadBezierScaled(display_t* disp, int x0, int y0, int x1, int y1, int x2,
                    int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)   /* plot any quadratic Bezier curve */
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotQuadBezierInner(disp, x0, y0, x1, y1, x2, y2, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotQuadRationalBezierSeg(display_t* disp, int x0, int y0, int x1, int y1, int x2, int y2,
                               float w, paletteColor_t col)   /* plot a limited rational Bezier segment, squared weight */
{
    SETUP_FOR_TURBO( disp );

    int sx = x2 - x1, sy = y2 - y1; /* relative values for checks */
    double dx = x0 - x2, dy = y0 - y2, xx = x0 - x1, yy = y0 - y1;
    double xy = xx * sy + yy * sx, cur = xx * sy - yy * sx, err; /* curvature */

    assert(xx * sx <= 0.0 && yy * sy <= 0.0); /* sign of gradient must not change */

    if (cur != 0.0 && w > 0.0)   /* no straight line */
    {
        if (sx * (long) sx + sy * (long) sy > xx * xx + yy * yy)   /* begin with longer part */
        {
            x2 = x0;
            x0 -= dx;
            y2 = y0;
            y0 -= dy;
            cur = -cur; /* swap P0 P2 */
        }
        xx = 2.0 * (4.0 * w * sx * xx + dx * dx); /* differences 2nd degree */
        yy = 2.0 * (4.0 * w * sy * yy + dy * dy);
        sx = x0 < x2 ? 1 : -1; /* x step direction */
        sy = y0 < y2 ? 1 : -1; /* y step direction */
        xy = -2.0 * sx * sy * (2.0 * w * xy + dx * dy);

        if (cur * sx * sy < 0.0)   /* negated curvature? */
        {
            xx = -xx;
            yy = -yy;
            xy = -xy;
            cur = -cur;
        }
        dx = 4.0 * w * (x1 - x0) * sy * cur + xx / 2.0 + xy; /* differences 1st degree */
        dy = 4.0 * w * (y0 - y1) * sx * cur + yy / 2.0 + xy;

        if (w < 0.5 && (dy > xy || dx < xy))   /* flat ellipse, algorithm fails */
        {
            cur = (w + 1.0) / 2.0;
            w = sqrt(w);
            xy = 1.0 / (w + 1.0);
            sx = floor((x0 + 2.0 * w * x1 + x2) * xy / 2.0 + 0.5); /* subdivide curve in half */
            sy = floor((y0 + 2.0 * w * y1 + y2) * xy / 2.0 + 0.5);
            dx = floor((w * x1 + x0) * xy + 0.5);
            dy = floor((y1 * w + y0) * xy + 0.5);
            plotQuadRationalBezierSeg(disp, x0, y0, dx, dy, sx, sy, cur, col);/* plot separately */
            dx = floor((w * x1 + x2) * xy + 0.5);
            dy = floor((y1 * w + y2) * xy + 0.5);
            plotQuadRationalBezierSeg(disp, sx, sy, dx, dy, x2, y2, cur, col);
            return;
        }
        err = dx + dy - xy; /* error 1.step */
        do
        {
            TURBO_SET_PIXEL_BOUNDS(disp, x0, y0, col); /* plot curve */
            if (x0 == x2 && y0 == y2)
            {
                return;    /* last pixel -> curve finished */
            }
            x1 = 2 * err > dy;
            y1 = 2 * (err + yy) < -dy;/* save value for test of x step */
            if (2 * err < dx || y1)
            {
                y0 += sy;
                dy += xy;
                err += dx += xx;
            }/* y step */
            if (2 * err > dx || x1)
            {
                x0 += sx;
                dx += xy;
                err += dy += yy;
            }/* x step */
        } while (dy <= xy && dx >= xy); /* gradient negates -> algorithm fails */
    }
    plotLine(disp, x0, y0, x2, y2, col, 0); /* plot remaining needle to end */
}

void plotQuadRationalBezier(display_t* disp, int x0, int y0, int x1, int y1, int x2, int y2,
                            float w, paletteColor_t col)   /* plot any quadratic rational Bezier curve */
{
    int x = x0 - 2 * x1 + x2, y = y0 - 2 * y1 + y2;
    double xx = x0 - x1, yy = y0 - y1, ww, t, q;

    assert(w >= 0.0);

    if (xx * (x2 - x1) > 0)   /* horizontal cut at P4? */
    {
        if (yy * (y2 - y1) > 0) /* vertical cut at P6 too? */
            if (fabs(xx * y) > fabs(yy * x))   /* which first? */
            {
                x0 = x2;
                x2 = xx + x1;
                y0 = y2;
                y2 = yy + y1; /* swap points */
            } /* now horizontal cut at P4 comes first */
        if (x0 == x2 || w == 1.0)
        {
            t = (x0 - x1) / (double) x;
        }
        else   /* non-rational or rational case */
        {
            q = sqrt(
                    4.0 * w * w * (x0 - x1) * (x2 - x1)
                    + (x2 - x0) * (long) (x2 - x0));
            if (x1 < x0)
            {
                q = -q;
            }
            t = (2.0 * w * (x0 - x1) - x0 + x2 + q)
                / (2.0 * (1.0 - w) * (x2 - x0)); /* t at P4 */
        }
        q = 1.0 / (2.0 * t * (1.0 - t) * (w - 1.0) + 1.0); /* sub-divide at t */
        xx = (t * t * (x0 - 2.0 * w * x1 + x2) + 2.0 * t * (w * x1 - x0) + x0)
             * q; /* = P4 */
        yy = (t * t * (y0 - 2.0 * w * y1 + y2) + 2.0 * t * (w * y1 - y0) + y0)
             * q;
        ww = t * (w - 1.0) + 1.0;
        ww *= ww * q; /* squared weight P3 */
        w = ((1.0 - t) * (w - 1.0) + 1.0) * sqrt(q); /* weight P8 */
        x = floor(xx + 0.5);
        y = floor(yy + 0.5); /* P4 */
        yy = (xx - x0) * (y1 - y0) / (x1 - x0) + y0; /* intersect P3 | P0 P1 */
        plotQuadRationalBezierSeg(disp, x0, y0, x, floor(yy + 0.5), x, y, ww, col);
        yy = (xx - x2) * (y1 - y2) / (x1 - x2) + y2; /* intersect P4 | P1 P2 */
        y1 = floor(yy + 0.5);
        x0 = x1 = x;
        y0 = y; /* P0 = P4, P1 = P8 */
    }
    if ((y0 - y1) * (long) (y2 - y1) > 0)   /* vertical cut at P6? */
    {
        if (y0 == y2 || w == 1.0)
        {
            t = (y0 - y1) / (y0 - 2.0 * y1 + y2);
        }
        else   /* non-rational or rational case */
        {
            q = sqrt(
                    4.0 * w * w * (y0 - y1) * (y2 - y1)
                    + (y2 - y0) * (long) (y2 - y0));
            if (y1 < y0)
            {
                q = -q;
            }
            t = (2.0 * w * (y0 - y1) - y0 + y2 + q)
                / (2.0 * (1.0 - w) * (y2 - y0)); /* t at P6 */
        }
        q = 1.0 / (2.0 * t * (1.0 - t) * (w - 1.0) + 1.0); /* sub-divide at t */
        xx = (t * t * (x0 - 2.0 * w * x1 + x2) + 2.0 * t * (w * x1 - x0) + x0)
             * q; /* = P6 */
        yy = (t * t * (y0 - 2.0 * w * y1 + y2) + 2.0 * t * (w * y1 - y0) + y0)
             * q;
        ww = t * (w - 1.0) + 1.0;
        ww *= ww * q; /* squared weight P5 */
        w = ((1.0 - t) * (w - 1.0) + 1.0) * sqrt(q); /* weight P7 */
        x = floor(xx + 0.5);
        y = floor(yy + 0.5); /* P6 */
        xx = (x1 - x0) * (yy - y0) / (y1 - y0) + x0; /* intersect P6 | P0 P1 */
        plotQuadRationalBezierSeg(disp, x0, y0, floor(xx + 0.5), y, x, y, ww, col);
        xx = (x1 - x2) * (yy - y2) / (y1 - y2) + x2; /* intersect P7 | P1 P2 */
        x1 = floor(xx + 0.5);
        x0 = x;
        y0 = y1 = y; /* P0 = P6, P1 = P7 */
    }
    plotQuadRationalBezierSeg(disp, x0, y0, x1, y1, x2, y2, w * w, col); /* remaining */
}

void plotRotatedEllipse(display_t* disp, int x, int y, int a, int b,
                        float angle, paletteColor_t col)   /* plot ellipse rotated by angle (radian) */
{
    float xd = (long) a * a, yd = (long) b * b;
    float s = sin(angle), zd = (xd - yd) * s; /* ellipse rotation */
    xd = sqrt(xd - zd * s), yd = sqrt(yd + zd * s); /* surrounding rectangle */
    a = xd + 0.5;
    b = yd + 0.5;
    zd = zd * a * b / (xd * yd); /* scale to integer */
    plotRotatedEllipseRect(disp, x - a, y - b, x + a, y + b,
                           (long) (4 * zd * cos(angle)), col);
}

void plotRotatedEllipseRect(display_t* disp, int x0, int y0, int x1, int y1,
                            long zd, paletteColor_t col)   /* rectangle enclosing the ellipse, integer rotation angle */
{
    int xd = x1 - x0, yd = y1 - y0;
    float w = xd * (long) yd;
    if (zd == 0)
    {
        return plotEllipseRect(disp, x0, y0, x1, y1, col);    /* looks nicer */
    }
    if (w != 0.0)
    {
        w = (w - zd) / (w + w);    /* squared weight of P1 */
    }
    assert(w <= 1.0 && w >= 0.0); /* limit angle to |zd|<=xd*yd */
    xd = floor(xd * w + 0.5);
    yd = floor(yd * w + 0.5); /* snap xe,ye to int */
    plotQuadRationalBezierSeg(disp, x0, y0 + yd, x0, y0, x0 + xd, y0, 1.0 - w, col);
    plotQuadRationalBezierSeg(disp, x0, y0 + yd, x0, y1, x1 - xd, y1, w, col);
    plotQuadRationalBezierSeg(disp, x1, y1 - yd, x1, y1, x1 - xd, y1, 1.0 - w, col);
    plotQuadRationalBezierSeg(disp, x1, y1 - yd, x1, y0, x0 + xd, y0, w, col);
}

void plotCubicBezierSegInner(display_t* disp, int x0, int y0, float x1, float y1, float x2, float y2,
                        int x3, int y3, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)   /* plot limited cubic Bezier segment */
{
    SETUP_FOR_TURBO( disp );

    int f, fx, fy, leg = 1;
    int sx = x0 < x3 ? 1 : -1, sy = y0 < y3 ? 1 : -1; /* step direction */
    float xc = -fabs(x0 + x1 - x2 - x3), xa = xc - 4 * sx * (x1 - x2), xb = sx
               * (x0 - x1 - x2 + x3);
    float yc = -fabs(y0 + y1 - y2 - y3), ya = yc - 4 * sy * (y1 - y2), yb = sy
               * (y0 - y1 - y2 + y3);
    double ab, ac, bc, cb, xx, xy, yy, dx, dy, ex, *pxy, EP = 0.01;
    /* check for curve restrains */
    /* slope P0-P1 == P2-P3    and  (P0-P3 == P1-P2      or   no slope change) */
    assert(
        (x1 - x0) * (x2 - x3) < EP
        && ((x3 - x0) * (x1 - x2) < EP || xb * xb < xa * xc + EP));
    assert(
        (y1 - y0) * (y2 - y3) < EP
        && ((y3 - y0) * (y1 - y2) < EP || yb * yb < ya * yc + EP));

    if (xa == 0 && ya == 0)   /* quadratic Bezier */
    {
        sx = floor((3 * x1 - x0 + 1) / 2);
        sy = floor((3 * y1 - y0 + 1) / 2); /* new midpoint */
        return plotQuadBezierSegInner(disp, x0, y0, sx, sy, x3, y3, col, xTr, yTr, xScale, yScale);
    }
    x1 = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + 1; /* line lengths */
    x2 = (x2 - x3) * (x2 - x3) + (y2 - y3) * (y2 - y3) + 1;
    do   /* loop over both ends */
    {
        ab = xa * yb - xb * ya;
        ac = xa * yc - xc * ya;
        bc = xb * yc - xc * yb;
        ex = ab * (ab + ac - 3 * bc) + ac * ac; /* P0 part of self-intersection loop? */
        f = ex > 0 ? 1 : sqrt(1 + 1024 / x1); /* calculate resolution */
        ab *= f;
        ac *= f;
        bc *= f;
        ex *= f * f; /* increase resolution */
        xy = 9 * (ab + ac + bc) / 8;
        cb = 8 * (xa - ya); /* init differences of 1st degree */
        dx = 27 * (8 * ab * (yb * yb - ya * yc) + ex * (ya + 2 * yb + yc)) / 64
             - ya * ya * (xy - ya);
        dy = 27 * (8 * ab * (xb * xb - xa * xc) - ex * (xa + 2 * xb + xc)) / 64
             - xa * xa * (xy + xa);
        /* init differences of 2nd degree */
        xx = 3
             * (3 * ab * (3 * yb * yb - ya * ya - 2 * ya * yc)
                - ya * (3 * ac * (ya + yb) + ya * cb)) / 4;
        yy = 3
             * (3 * ab * (3 * xb * xb - xa * xa - 2 * xa * xc)
                - xa * (3 * ac * (xa + xb) + xa * cb)) / 4;
        xy = xa * ya * (6 * ab + 6 * ac - 3 * bc + cb);
        ac = ya * ya;
        cb = xa * xa;
        xy = 3
             * (xy + 9 * f * (cb * yb * yc - xb * xc * ac)
                - 18 * xb * yb * ab) / 8;

        if (ex < 0)   /* negate values if inside self-intersection loop */
        {
            dx = -dx;
            dy = -dy;
            xx = -xx;
            yy = -yy;
            xy = -xy;
            ac = -ac;
            cb = -cb;
        } /* init differences of 3rd degree */
        ab = 6 * ya * ac;
        ac = -6 * xa * ac;
        bc = 6 * ya * cb;
        cb = -6 * xa * cb;
        dx += xy;
        ex = dx + dy;
        dy += xy; /* error of 1st step */

        for (pxy = &xy, fx = fy = f; x0 != x3 && y0 != y3;)
        {
            TURBO_SET_PIXEL_BOUNDS(disp, xTr + x0 * xScale, yTr + y0 * yScale, col); /* plot curve */
            do   /* move sub-steps of one pixel */
            {
                if (dx > *pxy || dy < *pxy)
                {
                    goto exit;
                }
                /* confusing values */
                y1 = 2 * ex - dy; /* save value for test of y step */
                if (2 * ex >= dx)   /* x sub-step */
                {
                    fx--;
                    ex += dx += xx;
                    dy += xy += ac;
                    yy += bc;
                    xx += ab;
                }
                if (y1 <= 0)   /* y sub-step */
                {
                    fy--;
                    ex += dy += yy;
                    dx += xy += bc;
                    xx += ac;
                    yy += cb;
                }
            } while (fx > 0 && fy > 0); /* pixel complete? */
            if (2 * fx <= f)
            {
                x0 += sx;
                fx += f;
            } /* x step */
            if (2 * fy <= f)
            {
                y0 += sy;
                fy += f;
            } /* y step */
            if (pxy == &xy && dx < 0 && dy > 0)
            {
                pxy = &EP;    /* pixel ahead valid */
            }
        }
exit:
        xx = x0;
        x0 = x3;
        x3 = xx;
        sx = -sx;
        xb = -xb; /* swap legs */
        yy = y0;
        y0 = y3;
        y3 = yy;
        sy = -sy;
        yb = -yb;
        x1 = x2;
    } while (leg--); /* try other end */
    plotLineInner(disp, x0, y0, x3, y3, col, 0, xTr, yTr, xScale, yScale); /* remaining part in case of cusp or crunode */
}

void plotCubicBezierSeg(display_t* disp, int x0, int y0, float x1, float y1, float x2, float y2,
                        int x3, int y3, paletteColor_t col)   /* plot limited cubic Bezier segment */
{
    plotCubicBezierSegInner(disp, x0, y0, x1, y1, x2, y2, x3, y3, col, 0, 0, 1, 1);
}

void plotCubicBezierInner(display_t* disp, int x0, int y0, int x1, int y1, int x2, int y2, int x3,
                     int y3, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)   /* plot any cubic Bezier curve */
{
    int n = 0, i = 0;
    long xc = x0 + x1 - x2 - x3, xa = xc - 4 * (x1 - x2);
    long xb = x0 - x1 - x2 + x3, xd = xb + 4 * (x1 + x2);
    long yc = y0 + y1 - y2 - y3, ya = yc - 4 * (y1 - y2);
    long yb = y0 - y1 - y2 + y3, yd = yb + 4 * (y1 + y2);
    float fx0 = x0, fx1, fx2, fx3, fy0 = y0, fy1, fy2, fy3;
    double t1 = xb * xb - xa * xc, t2, t[5];
    /* sub-divide curve at gradient sign changes */
    if (xa == 0)   /* horizontal */
    {
        if (abs(xc) < 2 * abs(xb))
        {
            t[n++] = xc / (2.0 * xb);    /* one change */
        }
    }
    else if (t1 > 0.0)     /* two changes */
    {
        t2 = sqrt(t1);
        t1 = (xb - t2) / xa;
        if (fabs(t1) < 1.0)
        {
            t[n++] = t1;
        }
        t1 = (xb + t2) / xa;
        if (fabs(t1) < 1.0)
        {
            t[n++] = t1;
        }
    }
    t1 = yb * yb - ya * yc;
    if (ya == 0)   /* vertical */
    {
        if (abs(yc) < 2 * abs(yb))
        {
            t[n++] = yc / (2.0 * yb);    /* one change */
        }
    }
    else if (t1 > 0.0)     /* two changes */
    {
        t2 = sqrt(t1);
        t1 = (yb - t2) / ya;
        if (fabs(t1) < 1.0)
        {
            t[n++] = t1;
        }
        t1 = (yb + t2) / ya;
        if (fabs(t1) < 1.0)
        {
            t[n++] = t1;
        }
    }
    for (i = 1; i < n; i++) /* bubble sort of 4 points */
        if ((t1 = t[i - 1]) > t[i])
        {
            t[i - 1] = t[i];
            t[i] = t1;
            i = 0;
        }

    t1 = -1.0;
    t[n] = 1.0; /* begin / end point */
    for (i = 0; i <= n; i++)   /* plot each segment separately */
    {
        t2 = t[i]; /* sub-divide at t[i-1], t[i] */
        fx1 = (t1 * (t1 * xb - 2 * xc) - t2 * (t1 * (t1 * xa - 2 * xb) + xc)
               + xd) / 8 - fx0;
        fy1 = (t1 * (t1 * yb - 2 * yc) - t2 * (t1 * (t1 * ya - 2 * yb) + yc)
               + yd) / 8 - fy0;
        fx2 = (t2 * (t2 * xb - 2 * xc) - t1 * (t2 * (t2 * xa - 2 * xb) + xc)
               + xd) / 8 - fx0;
        fy2 = (t2 * (t2 * yb - 2 * yc) - t1 * (t2 * (t2 * ya - 2 * yb) + yc)
               + yd) / 8 - fy0;
        fx0 -= fx3 = (t2 * (t2 * (3 * xb - t2 * xa) - 3 * xc) + xd) / 8;
        fy0 -= fy3 = (t2 * (t2 * (3 * yb - t2 * ya) - 3 * yc) + yd) / 8;
        x3 = floor(fx3 + 0.5);
        y3 = floor(fy3 + 0.5); /* scale bounds to int */
        if (fx0 != 0.0)
        {
            fx1 *= fx0 = (x0 - x3) / fx0;
            fx2 *= fx0;
        }
        if (fy0 != 0.0)
        {
            fy1 *= fy0 = (y0 - y3) / fy0;
            fy2 *= fy0;
        }
        if (x0 != x3 || y0 != y3) /* segment t1 - t2 */
        {
            plotCubicBezierSegInner(disp, x0, y0, x0 + fx1, y0 + fy1, x0 + fx2, y0 + fy2,
                            x3, y3, col, xTr, yTr, xScale, yScale);
        }
        x0 = x3;
        y0 = y3;
        fx0 = fx3;
        fy0 = fy3;
        t1 = t2;
    }
}

void plotCubicBezier(display_t* disp, int x0, int y0, int x1, int y1, int x2, int y2, int x3,
                     int y3, paletteColor_t col)
{
    plotCubicBezierInner(disp, x0, y0, x1, y1, x2, y2, x3, y3, col, 0, 0, 1, 1);
}

void plotCubicBezierScaled(display_t* disp, int x0, int y0, int x1, int y1, int x2, int y2, int x3,
                     int y3, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    for (uint8_t i = 0; i < xScale * yScale; i++)
    {
        plotCubicBezierInner(disp, x0, y0, x1, y1, x2, y2, x3, y3, col, xTr + i % yScale, yTr + i / xScale, xScale, yScale);
    }
}

void plotQuadSpline(display_t* disp, int n, int x[], int y[],
                    paletteColor_t col)   /* plot quadratic spline, destroys input arrays x,y */
{
#define M_MAX 6
    float mi = 1, m[M_MAX]; /* diagonal constants of matrix */
    int i, x0, y0, x1, y1, x2 = x[n], y2 = y[n];

    assert(n > 1); /* need at least 3 points P[0]..P[n] */

    x[1] = x0 = 8 * x[1] - 2 * x[0]; /* first row of matrix */
    y[1] = y0 = 8 * y[1] - 2 * y[0];

    for (i = 2; i < n; i++)   /* forward sweep */
    {
        if (i - 2 < M_MAX)
        {
            m[i - 2] = mi = 1.0 / (6.0 - mi);
        }
        x[i] = x0 = floor(8 * x[i] - x0 * mi + 0.5); /* store yi */
        y[i] = y0 = floor(8 * y[i] - y0 * mi + 0.5);
    }
    x1 = floor((x0 - 2 * x2) / (5.0 - mi) + 0.5); /* correction last row */
    y1 = floor((y0 - 2 * y2) / (5.0 - mi) + 0.5);

    for (i = n - 2; i > 0; i--)   /* back substitution */
    {
        if (i <= M_MAX)
        {
            mi = m[i - 1];
        }
        x0 = floor((x[i] - x1) * mi + 0.5); /* next corner */
        y0 = floor((y[i] - y1) * mi + 0.5);
        plotQuadBezier(disp, (x0 + x1) / 2, (y0 + y1) / 2, x1, y1, x2, y2, col);
        x2 = (x0 + x1) / 2;
        x1 = x0;
        y2 = (y0 + y1) / 2;
        y1 = y0;
    }
    plotQuadBezier(disp, x[0], y[0], x1, y1, x2, y2, col);
}

void plotCubicSpline(display_t* disp, int n, int x[], int y[],
                     paletteColor_t col)   /* plot cubic spline, destroys input arrays x,y */
{
#define M_MAX 6
    float mi = 0.25, m[M_MAX]; /* diagonal constants of matrix */
    int x3 = x[n - 1], y3 = y[n - 1], x4 = x[n], y4 = y[n];
    int i, x0, y0, x1, y1, x2, y2;

    assert(n > 2); /* need at least 4 points P[0]..P[n] */

    x[1] = x0 = 12 * x[1] - 3 * x[0]; /* first row of matrix */
    y[1] = y0 = 12 * y[1] - 3 * y[0];

    for (i = 2; i < n; i++)   /* foreward sweep */
    {
        if (i - 2 < M_MAX)
        {
            m[i - 2] = mi = 0.25 / (2.0 - mi);
        }
        x[i] = x0 = floor(12 * x[i] - 2 * x0 * mi + 0.5);
        y[i] = y0 = floor(12 * y[i] - 2 * y0 * mi + 0.5);
    }
    x2 = floor((x0 - 3 * x4) / (7 - 4 * mi) + 0.5); /* correct last row */
    y2 = floor((y0 - 3 * y4) / (7 - 4 * mi) + 0.5);
    plotCubicBezier(disp, x3, y3, (x2 + x4) / 2, (y2 + y4) / 2, x4, y4, x4, y4, col);

    if (n - 3 < M_MAX)
    {
        mi = m[n - 3];
    }
    x1 = floor((x[n - 2] - 2 * x2) * mi + 0.5);
    y1 = floor((y[n - 2] - 2 * y2) * mi + 0.5);
    for (i = n - 3; i > 0; i--)   /* back substitution */
    {
        if (i <= M_MAX)
        {
            mi = m[i - 1];
        }
        x0 = floor((x[i] - 2 * x1) * mi + 0.5);
        y0 = floor((y[i] - 2 * y1) * mi + 0.5);
        x4 = floor((x0 + 4 * x1 + x2 + 3) / 6.0); /* reconstruct P[i] */
        y4 = floor((y0 + 4 * y1 + y2 + 3) / 6.0);
        plotCubicBezier(disp, x4, y4, floor((2 * x1 + x2) / 3 + 0.5),
                        floor((2 * y1 + y2) / 3 + 0.5), floor((x1 + 2 * x2) / 3 + 0.5),
                        floor((y1 + 2 * y2) / 3 + 0.5), x3, y3, col);
        x3 = x4;
        y3 = y4;
        x2 = x1;
        y2 = y1;
        x1 = x0;
        y1 = y0;
    }
    x0 = x[0];
    x4 = floor((3 * x0 + 7 * x1 + 2 * x2 + 6) / 12.0); /* reconstruct P[1] */
    y0 = y[0];
    y4 = floor((3 * y0 + 7 * y1 + 2 * y2 + 6) / 12.0);
    plotCubicBezier(disp, x4, y4, floor((2 * x1 + x2) / 3 + 0.5),
                    floor((2 * y1 + y2) / 3 + 0.5), floor((x1 + 2 * x2) / 3 + 0.5),
                    floor((y1 + 2 * y2) / 3 + 0.5), x3, y3, col);
    plotCubicBezier(disp, x0, y0, x0, y0, (x0 + x1) / 2, (y0 + y1) / 2, x4, y4, col);
}
