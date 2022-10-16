#ifndef _PAINT_DRAW_H_
#define _PAINT_DRAW_H_

#include "palette.h"
#include "swadge_util.h"
#include "swadgeMode.h"

#include "paint_common.h"
#include "paint_brush.h"

extern paintDraw_t* paintState;

extern wsg_t cursorCrosshairWsg;;

extern wsg_t cursorBoxWsg;

// Mode callback delegates
void paintDrawScreenMainLoop(int64_t elapsedUs);
void paintDrawScreenButtonCb(const buttonEvt_t* evt);
void paintPaletteModeButtonCb(const buttonEvt_t* evt);
void paintSaveModeButtonCb(const buttonEvt_t* evt);
void paintSelectModeButtonCb(const buttonEvt_t* evt);
void paintDrawModeButtonCb(const buttonEvt_t* evt);

// Palette mode helpers
void paintEditPaletteUpdate(void);
void paintEditPaletteDecChannel(void);
void paintEditPaletteIncChannel(void);
void paintEditPaletteNextChannel(void);
void paintEditPaletteSetupColor(void);
void paintEditPalettePrevColor(void);
void paintEditPaletteNextColor(void);
void paintEditPaletteConfirm(void);


// Save menu helpers
void paintSaveModePrevItem(void);
void paintSaveModeNextItem(void);
void paintSaveModePrevOption(void);
void paintSaveModeNextOption(void);

void paintDrawScreenSetup(display_t* disp);
void paintDrawScreenCleanup(void);

void paintPositionDrawCanvas(void);

void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col);
void paintUpdateRecents(uint8_t selectedIndex);
void paintUpdateLeds(void);
void paintDrawPickPoints(void);
void paintHidePickPoints(void);

// Brush Helper Functions
void paintSetupTool(void);
void paintPrevTool(void);
void paintNextTool(void);
void paintDecBrushWidth(void);
void paintIncBrushWidth(void);

// Cursor helper functions
void setCursor(const wsg_t* cursorWsg);
void paintMoveCursorRel(int8_t xDiff, int8_t yDiff);
void enableCursor(void);
void disableCursor(void);

// Artist helpers
paintArtist_t* getArtist(void);
paintCursor_t* getCursor(void);

#endif
