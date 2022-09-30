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

wsg_t cursorCrosshairWsg = {
    .px = cursorPxsCrosshair,
    .w = 5,
    .h = 5,
};

wsg_t cursorBoxWsg = {
    .px = cursorPxsBox,
    .w = 5,
    .h = 5,
};


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

typedef struct
{
    //////// General app data

    // Main Menu Font
    font_t menuFont;
    // Main Menu
    meleeMenu_t* menu;

    // Font for drawing tool info
    // TODO: Use images instead!
    font_t toolbarFont;

    display_t* disp;

    // The screen within paint that the user is in
    paintScreen_t screen;

    paintButtonMode_t buttonMode;

    // Whether or not A is currently pressed
    bool aHeld;

    // The width of the current canvas
    // TODO: Remove and replace with constant? Or, remove constant and use this?
    int16_t canvasW, canvasH;


    // Color data

    // The foreground color and the background color, which can be swapped with B
    paletteColor_t fgColor, bgColor;

    // This image's palette, including foreground and background colors.
    // It is always ordered with the most-recently-used colors at the beginning
    paletteColor_t recentColors[PAINT_MAX_COLORS];

    // The index of the currently selected color, while SELECT is held
    uint8_t paletteSelect;

    led_t leds[NUM_LEDS];


    //////// Brush / Tool data

    // Index of the currently selected brush / tool
    uint8_t brushIndex;

    // A pointer to the currently selected brush's definition for convenience
    const brush_t* brush;

    // The current brush width or variant, depending on the brush
    uint8_t brushWidth;


    //////// Pick Points

    // An array of points that have been selected for the current brush
    // TODO: Replace with a pxStack_t and get rid of the other `pxStack` maybe?
    point_t pickPoints[MAX_PICK_POINTS];

    // The number of points already selected
    size_t pickCount;

    // The saved pixels that are covered up by the pick points
    pxStack_t pxStack;

    // Whether all pick points should be redrawn with the current fgColor, for when the color changes while we're picking
    bool recolorPickPoints;


    //////// Cursor

    // The sprite for the currently selected cursor
    const wsg_t* cursorWsg;

    // The saved pixels thate rae covered up by the cursor
    pxStack_t cursorPxs;

    bool showCursor;

    // The number of canvas pixels to move the cursor this frame
    int8_t moveX, moveY;

    // The current position of the cursor in canvas pixels
    int16_t cursorX, cursorY;

    // The previous position of the cursor in canvas pixels
    // TODO: Remove this and just use `cursorPxs` instead
    int16_t lastCursorX, lastCursorY;

    // When true, this is the initial D-pad button down.
    // If set, the cursor will move by one pixel and then it will be cleared.
    bool firstMove;

    // The time a D-pad button has been held down for, in microseconds
    int64_t btnHoldTime;


    //////// Rendering flags

    // If set, the canvas will be cleared and the screen will be redrawn. Set on startup.
    bool clearScreen;

    // Set to redraw the toolbar on the next loop, when a brush or color is being selected
    bool redrawToolbar;


    //////// Save data flags

    // Whether to perform a save on the next loop
    bool doSave;

    // True when a save has been started but not yet completed. Prevents input while saving.
    bool saveInProgress;

    // The save slot selected when in BTN_MODE_SAVE
    uint8_t selectedSlot;

    paintSaveMenu_t saveMenu;

    // True if "Save" is selected, false if "Load" is selected
    bool isSaveSelected;

    // State for Yes/No in overwrite save menu.
    bool overwriteYesSelected;

    // Index keeping track of which slots are in use and the most recent slot
    int32_t index;
} paintMenu_t;

// UI Rendering Functions
void paintRenderCursor(void);
void paintRenderToolbar(void);
void paintRenderAll(void);
void paintClearCanvas(void);

void plotCursor(void);

// Cursor Functions

void paintMoveCursorRel(int8_t xDiff, int8_t yDiff);

void setCursor(const wsg_t* cursor);

void restoreCursorPixels(void);
void enableCursor(void);
void disableCursor(void);


// Game logic
void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col);
void paintUpdateRecents(uint8_t selectedIndex);
void paintDrawPickPoints(void);
void paintHidePickPoints(void);



#endif
