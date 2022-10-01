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
    void (*fnDraw)(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col);
} brush_t;


void floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t col);
void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);
void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);

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

#endif
