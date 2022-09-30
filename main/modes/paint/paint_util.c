#include "paint_util.h"

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
