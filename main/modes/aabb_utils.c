//==============================================================================
// Includes
//==============================================================================

#include "display.h"
#include "bresenham.h"
#include "aabb_utils.h"

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 * @param disp
 * @param box
 * @param color
 * @param fractional_bits
 */
void drawBox(display_t * disp, box_t box, rgba_pixel_t color, uint8_t fractional_bits)
{
    plotRect(disp,
             (box.x0 + (fractional_bits >> 2)) / fractional_bits,
             (box.y0 + (fractional_bits >> 2)) / fractional_bits,
             (box.x1 + (fractional_bits >> 2)) / fractional_bits,
             (box.y1 + (fractional_bits >> 2)) / fractional_bits,
             color);
}

/**
 * @brief TODO
 *
 * @param box0
 * @param box1
 * @return true
 * @return false
 */
bool boxesCollide(box_t box0, box_t box1)
{
    return (box0.x0 < box1.x1 &&
            box0.x1 > box1.x0 &&
            box0.y0 < box1.y1 &&
            box0.y1 > box0.y0);
}
