#if !defined(MODE_PAINT_H_)
#define _MODE_PAINT_H_

#include "swadgeMode.h"

// The number of colors in the palette and the max number of colors an image can be saved with
#define PAINT_MAX_COLORS 16

// The size of the buffer for loading/saving the image. Each chunk is saved as a separate blob in NVS
#define PAINT_SAVE_CHUNK_SIZE 1024


#define TFT_WIDTH 280
#define TFT_HEIGHT 240

#define PAINT_CANVAS_SCALE 3
#define PAINT_CANVAS_WIDTH 70
#define PAINT_CANVAS_HEIGHT 60

#define PAINT_CANVAS_X_OFFSET ((TFT_WIDTH - PAINT_CANVAS_WIDTH * PAINT_CANVAS_SCALE) / 2)
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

extern swadgeMode modePaint;

#endif
