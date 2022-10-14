#include "paint_brush.h"

#include <stdlib.h>
#include <math.h>

#include "paint_common.h"
#include "paint_util.h"
#include "bresenham.h"


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

void paintDrawSquarePen(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotRectFilledScaled(canvas->disp, points[0].x, points[0].y, points[0].x + (size), points[0].y + (size), col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawCirclePen(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    // Add one to the size because it isn't really a circle at r=1
    plotCircleFilledScaled(canvas->disp, points[0].x, points[0].y, size + 1, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawLine(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    PAINT_LOGD("Drawing line from (%d, %d) to (%d, %d)", points[0].x, points[0].y, points[1].x, points[1].y);
    plotLineScaled(canvas->disp, points[0].x, points[0].y, points[1].x, points[1].y, col, 0, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawCurve(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotCubicBezierScaled(canvas->disp, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawRectangle(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t x0 = (points[0].x > points[1].x) ? points[1].x : points[0].x;
    uint16_t y0 = (points[0].y > points[1].y) ? points[1].y : points[0].y;
    uint16_t x1 = (points[0].x > points[1].x) ? points[0].x : points[1].x;
    uint16_t y1 = (points[0].y > points[1].y) ? points[0].y : points[1].y;

    plotRectScaled(canvas->disp, x0, y0, x1 + 1, y1 + 1, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawFilledRectangle(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t x0 = (points[0].x > points[1].x) ? points[1].x : points[0].x;
    uint16_t y0 = (points[0].y > points[1].y) ? points[1].y : points[0].y;
    uint16_t x1 = (points[0].x > points[1].x) ? points[0].x : points[1].x;
    uint16_t y1 = (points[0].y > points[1].y) ? points[0].y : points[1].y;

    // This function takes care of its own scaling because it's very easy and will save a lot of unnecessary draws
    plotRectFilledScaled(canvas->disp, x0, y0, x1 + 1, y1 + 1, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawCircle(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t dX = abs(points[0].x - points[1].x);
    uint16_t dY = abs(points[0].y - points[1].y);
    uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);

    plotCircleScaled(canvas->disp, points[0].x, points[0].y, r, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawFilledCircle(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t dX = abs(points[0].x - points[1].x);
    uint16_t dY = abs(points[0].y - points[1].y);
    uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);

    plotCircleFilledScaled(canvas->disp, points[0].x, points[0].y, r, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawEllipse(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    pxStack_t tmpPxs;
    // TODO maybe we shouldn't use a heap-allocated pxStack here
    initPxStack(&tmpPxs);

    // for some reason, plotting an ellipse also plots 2 extra points outside of the ellipse
    // let's just work around that
    pushPxScaled(&tmpPxs, canvas->disp, (points[0].x < points[1].x ? points[0].x : points[1].x) + (abs(points[1].x - points[0].x) + 1) / 2, points[0].y < points[1].y ? points[0].y - 2 : points[1].y - 2, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
    pushPxScaled(&tmpPxs, canvas->disp, (points[0].x < points[1].x ? points[0].x : points[1].x) + (abs(points[1].x - points[0].x) + 1) / 2, points[0].y < points[1].y ? points[1].y + 2 : points[0].y + 2, canvas->x, canvas->y, canvas->xScale, canvas->yScale);

    plotEllipseRectScaled(canvas->disp, points[0].x, points[0].y, points[1].x, points[1].y, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);

    while (popPxScaled(&tmpPxs, canvas->disp, canvas->xScale, canvas->yScale));
    freePxStack(&tmpPxs);
}

void paintDrawPolygon(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    for (uint8_t i = 0; i < numPoints - 1; i++)
    {
        plotLineScaled(canvas->disp, points[i].x, points[i].y, points[i+1].x, points[i+1].y, col, 0, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
    }
}

void paintDrawSquareWave(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    paintPlotSquareWave(canvas->disp, points[0].x, points[0].y, points[1].x, points[1].y, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawPaintBucket(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    floodFill(canvas->disp, canvas->x + canvas->xScale * points[0].x, canvas->y + canvas->yScale * points[0].y, col, canvas->x, canvas->y, canvas->x + canvas->xScale * canvas->w, canvas->y + canvas->yScale * canvas->h);
}
