#ifndef _PAINT_COMMON_H_
#define _PAINT_COMMON_H_

#include <stddef.h>

#define PAINT_LOGV(...) ESP_LOGV("Paint", __VA_ARGS__)
#define PAINT_LOGD(...) ESP_LOGD("Paint", __VA_ARGS__)
#define PAINT_LOGI(...) ESP_LOGI("Paint", __VA_ARGS__)
#define PAINT_LOGW(...) ESP_LOGW("Paint", __VA_ARGS__)
#define PAINT_LOGE(...) ESP_LOGE("Paint", __VA_ARGS__)


// The number of colors in the palette and the max number of colors an image can be saved with
#define PAINT_MAX_COLORS 16


// The total number of save slots available
#define PAINT_SAVE_SLOTS 8

// Whether to have the LEDs show the current colors
#define PAINT_ENABLE_LEDS (0x0001 << (PAINT_SAVE_SLOTS * 2))

// Whether to play SFX on drawing, etc.
#define PAINT_ENABLE_SFX  (0x0002 << (PAINT_SAVE_SLOTS * 2))

// Whether to play background music
#define PAINT_ENABLE_BGM  (0x0004 << (PAINT_SAVE_SLOTS * 2))

// Default to LEDs, SFX, and music on, with slot 0 marked as most recent
#define PAINT_DEFAULTS (PAINT_ENABLE_LEDS | PAINT_ENABLE_SFX | PAINT_ENABLE_BGM)

// Mask for the index that includes everything except the most-recent index
#define PAINT_MASK_NOT_RECENT (PAINT_ENABLE_LEDS | PAINT_ENABLE_SFX | PAINT_ENABLE_BGM | ((1 << PAINT_SAVE_SLOTS) - 1))

// The size of the buffer for loading/saving the image. Each chunk is saved as a separate blob in NVS
#define PAINT_SAVE_CHUNK_SIZE 1024

#define PAINT_CANVAS_SCALE 3
#define PAINT_CANVAS_WIDTH 70
#define PAINT_CANVAS_HEIGHT 60

#define PAINT_CANVAS_X_OFFSET ((paintState->disp->w - PAINT_CANVAS_WIDTH * PAINT_CANVAS_SCALE) / 2)
#define PAINT_CANVAS_Y_OFFSET 26


#define PAINT_TOOLBAR_BG c333

// Set to 1 for horizontal colorboxes
#define PAINT_COLORBOX_HORIZONTAL 0

// Dimensions of the color boxes in the palette
#define PAINT_COLORBOX_W 9
#define PAINT_COLORBOX_H 9

#define PAINT_COLORBOX_SHADOW_TOP c444
#define PAINT_COLORBOX_SHADOW_BOTTOM c222

#define PAINT_COLORBOX_MARGIN_TOP 2
#define PAINT_COLORBOX_MARGIN_LEFT 2

// X and Y position of the active color boxes (foreground/background color)
#define PAINT_ACTIVE_COLOR_X ((PAINT_CANVAS_X_OFFSET - PAINT_COLORBOX_W - PAINT_COLORBOX_W / 2) / 2)
#define PAINT_ACTIVE_COLOR_Y (PAINT_CANVAS_Y_OFFSET - PAINT_COLORBOX_H / 2)

// X and Y position of the first palette color box
#define PAINT_COLORBOX_X ((PAINT_CANVAS_X_OFFSET - PAINT_COLORBOX_W) / 2)
#define PAINT_COLORBOX_Y (PAINT_ACTIVE_COLOR_Y + PAINT_COLORBOX_H * 2)


// Convert from canvas coordinates to screen coordinates
#define CNV2SCR_X(x) (PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_SCALE * x)
#define CNV2SCR_Y(y) (PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_SCALE * y)

// Convert from world coordinates to canvas coordinates
#define SCR2CNV_X(x) ((x - PAINT_CANVAS_X_OFFSET) / PAINT_CANVAS_SCALE)
#define SCR2CNV_Y(y) ((y - PAINT_CANVAS_Y_OFFSET) / PAINT_CANVAS_SCALE)


#define PIXEL_STACK_MIN_SIZE 2
#define MAX_PICK_POINTS 16

// hold button for .3s to begin repeating
#define BUTTON_REPEAT_TIME 300000


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

typedef paletteColor_t (*colorMapFn_t)(paletteColor_t col);

// Extra drawing functions
void paintDrawWsgTemp(display_t* display, const wsg_t* wsg, pxStack_t* saveTo, uint16_t x, uint16_t y, colorMapFn_t colorSwap);

void paintPlotSquareWave(display_t* disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col);
void plotRectFilledScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);

void setPxScaled(display_t* disp, int x, int y, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);


extern paintMenu_t* paintState;


#endif _PAINT_COMMON_H_
