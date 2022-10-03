#ifndef _PAINT_TYPE_H_
#define _PAINT_TYPE_H_

#include "stdint.h"

#include "display.h"

// The number of colors in the palette and the max number of colors an image can be saved with
#define PAINT_MAX_COLORS 16


typedef paletteColor_t (*colorMapFn_t)(paletteColor_t col);

/// @brief Defines each separate screen in the paint mode.
typedef enum
{
    // Top menu
    PAINT_MENU,
    // Control instructions
    PAINT_HELP,
    // Drawing mode
    PAINT_DRAW,
    // Select and view/edit saved drawings
    PAINT_GALLERY,
    // View a drawing, no editing
    PAINT_VIEW,
    // Share a drawing via ESPNOW
    PAINT_SHARE,
    // Receive a shared drawing over ESPNOW
    PAINT_RECEIVE,
} paintScreen_t;


/// @brief Represents the coordinates of a single pixel or point
typedef struct
{
    uint16_t x, y;
} point_t;


typedef enum
{
    BTN_MODE_DRAW,
    BTN_MODE_SELECT,
    BTN_MODE_SAVE,
} paintButtonMode_t;

typedef enum
{
    HIDDEN,
    PICK_SLOT_SAVE_LOAD,
    CONFIRM_OVERWRITE,
} paintSaveMenu_t;

/// @brief Definition for a paintable screen region
typedef struct
{
    // This screen's display
    display_t* disp;

    // The X and Y offset of the canvas's top-left pixel
    uint16_t x, y;

    // The canvas's width and height, in "canvas pixels"
    uint16_t w, h;

    // The X and Y scale of the canvas. Each "canvas pixel" will be drawn as [xScale x yScale]
    uint8_t xScale, yScale;

    paletteColor_t palette[PAINT_MAX_COLORS];
} paintCanvas_t;

#endif