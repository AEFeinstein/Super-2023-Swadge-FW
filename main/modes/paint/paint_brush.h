#ifndef _PAINT_BRUSH_H_
#define _PAINT_BRUSH_H_

#include <stdint.h>

#include "display.h"

#include "paint_type.h"

/**
 * Defines different brush behaviors for when A is pressed
 */
typedef enum
{
    // The brush is drawn immediately upon selection without picking or drawing any points
    INSTANT,

    // The brush is drawn whenever A is pressed or held and dragged
    HOLD_DRAW,

    // The brush requires a number of points to be picked first, and then it is drawn
    PICK_POINT,

    // The brush requires a number of points that always connect back to the first point
    PICK_POINT_LOOP,
} brushMode_t;

typedef struct
{
    /**
     * @brief The behavior mode of this brush
     */
    brushMode_t mode;

    /**
     * @brief The number of points this brush can use
     */
    uint8_t maxPoints;


    /**
     * @brief The minimum size (e.g. stroke width) of the brush
     */
    uint16_t minSize;

    /**
     * @brief The maximum size of the brush
     */
    uint16_t maxSize;

    /**
     * @brief The display name of this brush
     */
    char* name;

    /**
     * @brief Called when all necessary points have been selected and the final shape should be drawn
     */
    void (*fnDraw)(paintCanvas_t* canvas, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col);
} brush_t;


void floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t col, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax);
void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax);
void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill, uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax);

void paintDrawSquarePen(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCirclePen(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawLine(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCurve(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawRectangle(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawFilledRectangle(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCircle(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawFilledCircle(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawEllipse(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawPolygon(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawSquareWave(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawPaintBucket(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawClear(paintCanvas_t*, point_t*, uint8_t, uint16_t, paletteColor_t);

#endif
