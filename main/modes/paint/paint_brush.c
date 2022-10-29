#include "paint_brush.h"

#include <stdlib.h>
#include <math.h>

#include "paint_common.h"
#include "paint_util.h"
#include "bresenham.h"


void paintDrawSquarePen(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotRectFilledScaled(canvas->disp, points[0].x, points[0].y, points[0].x + (size), points[0].y + (size), col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawCirclePen(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotCircleFilledScaled(canvas->disp, points[0].x, points[0].y, size, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);

    // fill out the circle if it's very small
    if (size == 1)
    {
        setPxScaled(canvas->disp, points[0].x, points[0].y - 1, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
        setPxScaled(canvas->disp, points[0].x, points[0].y + 1, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
    }
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
    paintPlotSquareWave(canvas->disp, points[0].x, points[0].y, points[1].x, points[1].y, size, col, canvas->x, canvas->y, canvas->xScale, canvas->yScale);
}

void paintDrawPaintBucket(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    floodFill(canvas->disp, canvas->x + canvas->xScale * points[0].x, canvas->y + canvas->yScale * points[0].y, col, canvas->x, canvas->y, canvas->x + canvas->xScale * canvas->w, canvas->y + canvas->yScale * canvas->h);
}
