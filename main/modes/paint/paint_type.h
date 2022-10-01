#ifndef _PAINT_TYPE_H_
#define _PAINT_TYPE_H_

#include "stdint.h"


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

#endif