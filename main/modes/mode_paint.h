#if !defined(MODE_PAINT_H_)
#define _MODE_PAINT_H_

#include "swadgeMode.h"

#define PAINT_MAX_COLORS 16
#define PAINT_SAVE_CHUNK_SIZE 1024
#define PAINT_CANVAS_SCALE 1
#define PAINT_CANVAS_X_OFFSET 18
#define PAINT_CANVAS_Y_OFFSET 26
#define PAINT_CANVAS_WIDTH 256 // 70
#define PAINT_CANVAS_HEIGHT 192 // 60
#define PAINT_COLORBOX_SHADOW_TOP c444
#define PAINT_COLORBOX_SHADOW_BOTTOM c222

// Convert from canvas coordinates to screen coordinates
#define CNV2SCR_X(x) (PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_SCALE * x)
#define CNV2SCR_Y(y) (PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_SCALE * y)

// Convert from world coordinates to canvas coordinates
#define SCR2CNV_X(x) ((x - PAINT_CANVAS_X_OFFSET) / PAINT_CANVAS_SCALE)
#define SCR2CNV_Y(y) ((y - PAINT_CANVAS_Y_OFFSET) / PAINT_CANVAS_SCALE)

#define PAINT_TOOLBAR_BG c333

extern swadgeMode modePaint;

void setPaintMainMenu(bool resetPos);

#endif
