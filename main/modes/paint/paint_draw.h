#ifndef _PAINT_DRAW_H_
#define _PAINT_DRAW_H_

#include "palette.h"
#include "swadge_util.h"
#include "swadgeMode.h"

#include "paint_common.h"
#include "paint_brush.h"

static paletteColor_t cursorPxsBox[] =
{
    c000, c000, c000, c000, c000,
    c000, cTransparent, cTransparent, cTransparent, c000,
    c000, cTransparent, cTransparent, cTransparent, c000,
    c000, cTransparent, cTransparent, cTransparent, c000,
    c000, c000, c000, c000, c000,
};

static paletteColor_t cursorPxsCrosshair[] =
{
    cTransparent, cTransparent, c000, cTransparent, cTransparent,
    cTransparent, cTransparent, c000, cTransparent, cTransparent,
    c000, c000, cTransparent, c000, c000,
    cTransparent, cTransparent, c000, cTransparent, cTransparent,
    cTransparent, cTransparent, c000, cTransparent, cTransparent,
};

extern wsg_t cursorCrosshairWsg;;

extern wsg_t cursorBoxWsg;

// Mode callback delegates
void paintDrawScreenMainLoop(int64_t elapsedUs);
void paintSaveModeButtonCb(const buttonEvt_t* evt);
void paintSelectModeButtonCb(const buttonEvt_t* evt);
void paintDrawModeButtonCb(const buttonEvt_t* evt);

void paintDrawScreenSetup(void);
void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col);
void paintUpdateRecents(uint8_t selectedIndex);
void paintDrawPickPoints(void);
void paintHidePickPoints(void);

// Brush Helper Functions
void paintSetupTool(void);
void paintPrevTool(void);
void paintNextTool(void);
void paintDecBrushWidth(void);
void paintIncBrushWidth(void);

// Cursor helper functions
void paintMoveCursorRel(int8_t xDiff, int8_t yDiff);
void enableCursor(void);
void disableCursor(void);

#endif
