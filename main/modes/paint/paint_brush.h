#ifndef _PAINT_BRUSH_H_
#define _PAINT_BRUSH_H_

#include <stdint.h>

#include "display.h"

#include "paint_common.h"

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
    void (*fnDraw)(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col);
} brush_t;

void paintDrawSquarePen(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCirclePen(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawLine(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCurve(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawRectangle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawFilledRectangle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCircle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawFilledCircle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawEllipse(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawPolygon(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawSquareWave(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawPaintBucket(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawClear(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);

const brush_t brushes[] =
{
    { .name = "Square Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawSquarePen },
    { .name = "Circle Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawCirclePen },
    { .name = "Line",       .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawLine },
    { .name = "Bezier Curve", .mode = PICK_POINT, .maxPoints = 4, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCurve },
    { .name = "Rectangle",  .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawRectangle },
    { .name = "Filled Rectangle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledRectangle },
    { .name = "Circle",     .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCircle },
    { .name = "Filled Circle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledCircle },
    { .name = "Ellipse",    .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawEllipse },
    { .name = "Polygon",    .mode = PICK_POINT_LOOP, .maxPoints = MAX_PICK_POINTS, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawPolygon },
    { .name = "Squarewave", .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawSquareWave },
    { .name = "Paint Bucket", .mode = PICK_POINT, .maxPoints = 1, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawPaintBucket },
    { .name = "Clear",      .mode = INSTANT, .maxPoints = 0, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawClear },
};



#endif
