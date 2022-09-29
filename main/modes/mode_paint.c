#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "swadgeMode.h"
#include "btn.h"
#include "meleeMenu.h"
#include "swadge_esp32.h"
#include "nvs_manager.h"
#include "bresenham.h"
#include "math.h"
#include "mode_paint.h"

#include "mode_main_menu.h"
#include "swadge_util.h"

/*
 * REMAINING BIG THINGS TO DO:
 *
 * - Draw icons for the tools?
 * - Explicitly mark everything that doesn't work in the canvas coordinate space for whatever reason
 * - Sharing! How will that work...
 * - Airbrush tool
 * - Stamp tool?
 * - Copy/paste???
 * - Easter egg?
 *
 *
 * MORE MINOR POLISH THINGS:
 *
 * - Fix swapping fg/bg color sometimes causing current color not to be the first in the color picker list
 *   (sometimes this makes it so if you change a tool, it also changes your color)
 * - Use different pick markers / cursors for each brush (e.g. crosshair for circle pen, two L-shaped crosshairs for box pen...)
 */

#define PAINT_LOGV(...) ESP_LOGV("Paint", __VA_ARGS__)
#define PAINT_LOGD(...) ESP_LOGD("Paint", __VA_ARGS__)
#define PAINT_LOGI(...) ESP_LOGI("Paint", __VA_ARGS__)
#define PAINT_LOGW(...) ESP_LOGW("Paint", __VA_ARGS__)
#define PAINT_LOGE(...) ESP_LOGE("Paint", __VA_ARGS__)

static const char paintTitle[] = "MFPaint";
static const char menuOptDraw[] = "Draw";
static const char menuOptGallery[] = "Gallery";
static const char menuOptReceive[] = "Receive";
static const char menuOptExit[] = "Exit";

static const char startMenuSave[] = "Save";
static const char startMenuLoad[] = "Load";
static const char startMenuSlot[] = "Slot %d";
static const char startMenuSlotUsed[] = "Slot %d (!)";
static const char startMenuOverwrite[] = "Overwrite?";
static const char startMenuYes[] = "Yes";
static const char startMenuNo[] = "No";

typedef paletteColor_t (*colorMapFn_t)(paletteColor_t col);

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

/**
 * Defines different brush behaviors for when A is pressed
 */
typedef enum
{
    // The brush is drawn immediately upon selection without picking or drawing any points
    INSTANT,

    // The brush is drawn whenever A is pressed or held and dragged
    HOLD_DRAW,

    // The brush requires a number of points to be picked first, and then it is drawn
    PICK_POINT,

    // The brush requires a number of points that always connect back to the first point
    PICK_POINT_LOOP,
} brushMode_t;

typedef struct
{
    uint16_t x, y;
} point_t;

typedef struct
{
    /**
     * @brief The behavior mode of this brush
     */
    brushMode_t mode;

    /**
     * @brief The number of points this brush can use
     */
    uint8_t maxPoints;


    /**
     * @brief The minimum size (e.g. stroke width) of the brush
     */
    uint16_t minSize;

    /**
     * @brief The maximum size of the brush
     */
    uint16_t maxSize;

    /**
     * @brief The display name of this brush
     */
    char* name;

    /**
     * @brief Called when all necessary points have been selected and the final shape should be drawn
     */
    void (*fnDraw)(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col);
} brush_t;

typedef struct
{
    uint16_t x, y;
    paletteColor_t col;
} pxVal_t;

typedef struct
{
    // Pointer to the first of pixel coordinates/values, heap-allocated
    pxVal_t* data;

    // The number of pxVal_t entries currently allocated for the stack
    size_t size;

    // The index of the value on the top of the stack
    int32_t index;
} pxStack_t;

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


// Mode struct function declarations
void paintEnterMode(display_t* disp);
void paintExitMode(void);
void paintMainLoop(int64_t elapsedUs);
void paintButtonCb(buttonEvt_t* evt);
void paintMainMenuCb(const char* opt);


swadgeMode modePaint =
{
    .modeName = paintTitle,
    .fnEnterMode = paintEnterMode,
    .fnExitMode = paintExitMode,
    .fnMainLoop = paintMainLoop,
    .fnButtonCallback = paintButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

// Brush function declarations
void paintDrawSquarePen(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCirclePen(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawLine(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCurve(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawRectangle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawFilledRectangle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawCircle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawFilledCircle(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawEllipse(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawPolygon(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawSquareWave(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawPaintBucket(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawClear(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);

static const brush_t brushes[] =
{
    { .name = "Square Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawSquarePen },
    { .name = "Circle Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawCirclePen },
    { .name = "Line",       .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawLine },
    { .name = "Bezier Curve",      .mode = PICK_POINT, .maxPoints = 4, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCurve },
    { .name = "Rectangle",  .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawRectangle },
    { .name = "Filled Rectangle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledRectangle },
    { .name = "Circle",     .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCircle },
    { .name = "Filled Circle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledCircle },
    { .name = "Ellipse",    .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawEllipse },
    { .name = "Polygon",    .mode = PICK_POINT_LOOP, .maxPoints = MAX_PICK_POINTS, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawPolygon },
    { .name = "Squarewave", .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawSquareWave },
    { .name = "Paint Bucket", .mode = PICK_POINT, .maxPoints = 1, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawPaintBucket },
    { .name = "Clear",      .mode = INSTANT, .maxPoints = 0, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawClear },
};

paintMenu_t* paintState;

// Util function declarations

paletteColor_t getContrastingColor(paletteColor_t col);
void paintInitialize(void);
void paintRenderAll(void);
void restoreCursorPixels(void);
void plotCursor(void);
void enableCursor(void);
void disableCursor(void);
void paintRenderCursor(void);
void paintRenderToolbar(void);
void setCursor(const wsg_t* cursor);

void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col);
void paintMoveCursorRel(int8_t xDiff, int8_t yDiff);
void paintClearCanvas(void);
void paintUpdateRecents(uint8_t selectedIndex);
void paintDrawPickPoints(void);
void paintHidePickPoints(void);

void paintDrawWsgTemp(display_t* display, const wsg_t* wsg, pxStack_t* saveTo, uint16_t x, uint16_t y, colorMapFn_t colorSwap);
void paintPlotSquareWave(display_t* disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col);
void plotRectFilledScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void setPxScaled(display_t* disp, int x, int y, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void pushPxScaled(pxStack_t* pxStack, display_t* disp, int x, int y, int xTr, int yTr, int xScale, int yScale);
void popPxScaled(pxStack_t* pxStack, display_t* disp, int xScale, int yScale);
void drawColorBox(uint16_t xOffset, uint16_t yOffset, uint16_t w, uint16_t h, paletteColor_t col, bool selected, paletteColor_t topBorder, paletteColor_t bottomBorder);

void paintSetupTool(void);
void paintPrevTool(void);
void paintNextTool(void);
void paintDecBrushWidth(void);
void paintIncBrushWidth(void);
void paintLoadIndex(void);
void paintSaveIndex(void);
void paintResetStorage(void);
bool paintGetSlotInUse(uint8_t slot);
void paintSetSlotInUse(uint8_t slot);
uint8_t paintGetRecentSlot(void);
void paintSetRecentSlot(uint8_t slot);
void paintSave(uint8_t slot);
void paintLoad(uint8_t slot);
void floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t col);
void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);
void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);

void initPxStack(pxStack_t* pxStack);
void freePxStack(pxStack_t* pxStack);
void maybeGrowPxStack(pxStack_t* pxStack);
void maybeShrinkPxStack(pxStack_t* pxStack);
void pushPx(pxStack_t* pxStack, display_t* disp, uint16_t x, uint16_t y);
bool popPx(pxStack_t* pxStack, display_t* disp);

// Generic util functions

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

// Mode struct function implemetations

void paintEnterMode(display_t* disp)
{
    PAINT_LOGI("Allocatind %zu bytes for paintState...", sizeof(paintMenu_t));
    paintState = calloc(1, sizeof(paintMenu_t));

    paintState->disp = disp;
    loadFont("mm.font", &(paintState->menuFont));
    loadFont("radiostars.font", &(paintState->toolbarFont));

    paintState->menu = initMeleeMenu(paintTitle, &(paintState->menuFont), paintMainMenuCb);

    // Clear the LEDs
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        paintState->leds[i].r = 0;
        paintState->leds[i].g = 0;
        paintState->leds[i].b = 0;
    }
    setLeds(paintState->leds, NUM_LEDS);

    paintInitialize();
}

void paintExitMode(void)
{
    PAINT_LOGD("Exiting");
    deinitMeleeMenu(paintState->menu);
    freeFont(&(paintState->menuFont));

    freePxStack(&paintState->pxStack);
    freePxStack(&paintState->cursorPxs);
    free(paintState);
}

void paintMainLoop(int64_t elapsedUs)
{
    switch (paintState->screen)
    {
    case PAINT_MENU:
    {
        drawMeleeMenu(paintState->disp, paintState->menu);
        break;
    }

    case PAINT_DRAW:
    {
        if (paintState->clearScreen)
        {
            paintClearCanvas();
            paintRenderAll();
            paintState->clearScreen = false;
            paintState->showCursor = true;
        }

        if (paintState->doSave)
        {
            paintState->saveInProgress = true;
            if (paintState->isSaveSelected)
            {
                paintSave(paintState->selectedSlot);
            }
            else
            {
                if (paintGetSlotInUse(paintGetRecentSlot()))
                {
                    // Load from the selected slot if it's been used
                    paintLoad(paintState->selectedSlot);
                }
                else
                {
                    // If the slot hasn't been used yet, just clear the screen
                    paintState->clearScreen = true;
                    paintState->isSaveSelected = false;
                }
            }

            paintState->doSave = false;
            paintState->saveInProgress = false;

            paintState->buttonMode = BTN_MODE_DRAW;
            paintState->saveMenu = HIDDEN;

            paintState->redrawToolbar = true;
        }

        if (paintState->recolorPickPoints)
        {
            paintDrawPickPoints();
            paintState->recolorPickPoints = false;
        }


        if (paintState->redrawToolbar)
        {
            paintRenderToolbar();
            paintState->redrawToolbar = false;
        }
        else
        {
            // Don't remember why we only do this when redrawToolbar is true
            // Oh, it's because `paintState->redrawToolbar` is mostly only set in select mode unless you press B?
            if (paintState->aHeld)
            {
                paintDoTool(paintState->cursorX, paintState->cursorY, paintState->fgColor);

                if (paintState->brush->mode != HOLD_DRAW)
                {
                    paintState->aHeld = false;
                }
            }

            if (paintState->moveX || paintState->moveY)
            {
                paintState->btnHoldTime += elapsedUs;
                if (paintState->firstMove || paintState->btnHoldTime >= BUTTON_REPEAT_TIME)
                {

                    paintMoveCursorRel(paintState->moveX, paintState->moveY);
                    paintRenderAll();

                    paintState->firstMove = false;
                }
            }
        }
        break;
    }

    case PAINT_SHARE:
    break;

    case PAINT_RECEIVE:
    break;

    case PAINT_VIEW:
    break;

    case PAINT_GALLERY:
    break;

    case PAINT_HELP:
    break;
    }
}

void paintButtonCb(buttonEvt_t* evt)
{
    switch (paintState->screen)
    {
    case PAINT_MENU:
    {
        if (evt->down)
        {
            meleeMenuButton(paintState->menu, evt->button);
        }
        break;
    }

    case PAINT_DRAW:
    {
        if (evt->down)
        {
            //////// Button Press
            if (paintState->buttonMode == BTN_MODE_DRAW)
            {
                // Draw mode buttons
                switch (evt->button)
                {
                    case SELECT:
                    {
                        // Enter select mode (change color / brush)
                        paintState->buttonMode = BTN_MODE_SELECT;
                        paintState->redrawToolbar = true;
                        paintState->aHeld = false;
                        paintState->moveX = 0;
                        paintState->moveY = 0;
                        break;
                    }

                    case BTN_A:
                    {
                        // Draw
                        paintState->aHeld = true;
                        break;
                    }

                    case BTN_B:
                    {
                        // Swap the foreground and background colors
                        // no temporary variables for me thanks
                        paintState->fgColor ^= paintState->bgColor;
                        paintState->bgColor ^= paintState->fgColor;
                        paintState->fgColor ^= paintState->bgColor;

                        paintState->recentColors[0] ^= paintState->recentColors[1];
                        paintState->recentColors[1] ^= paintState->recentColors[0];
                        paintState->recentColors[0] ^= paintState->recentColors[1];

                        paintState->redrawToolbar = true;
                        paintState->recolorPickPoints = true;
                        break;
                    }

                    case UP:
                    {
                        paintState->firstMove = true;
                        paintState->moveY = -1;
                        break;
                    }

                    case DOWN:
                    {
                        paintState->firstMove = true;
                        paintState->moveY = 1;
                        break;
                    }

                    case LEFT:
                    {
                        paintState->firstMove = true;
                        paintState->moveX = -1;
                        break;
                    }

                    case RIGHT:
                    {
                        paintState->firstMove = true;
                        paintState->moveX = 1;
                        break;
                    }

                    case START:
                    // Don't do anything until start is released to avoid conflicting with EXIT
                    break;
                }
            }
            else if (paintState->buttonMode == BTN_MODE_SAVE)
            {
                //////// Save-mode button down

                paintState->redrawToolbar = true;
                switch (evt->button)
                {
                    case BTN_A:
                    {
                        if (paintState->saveMenu == PICK_SLOT_SAVE_LOAD)
                        {
                            if (paintState->isSaveSelected)
                            {
                                // The screen says "Save" and "Slot X"
                                if (paintGetSlotInUse(paintState->selectedSlot))
                                {
                                    // This slot is in use! Move on to the overwrite menu
                                    paintState->overwriteYesSelected = false;
                                    paintState->saveMenu = CONFIRM_OVERWRITE;
                                }
                                else
                                {
                                    // The slot isn't in use, go for it!
                                    paintState->doSave = true;
                                }
                            }
                            else
                            {
                                // The screen says "Load" and "Slot X"
                                // TODO: Track if image has been modified since saved and add a prompt for that
                                paintState->doSave = true;
                            }
                        }
                        else if (paintState->saveMenu == CONFIRM_OVERWRITE)
                        {
                            if (paintState->overwriteYesSelected)
                            {
                                paintState->doSave = true;
                            }
                            else
                            {
                                paintState->saveMenu = PICK_SLOT_SAVE_LOAD;
                            }
                        }
                        else if (paintState->saveMenu == HIDDEN)
                        {
                            // We shouldn't be here!!!
                            paintState->buttonMode = BTN_MODE_DRAW;
                        }

                        break;
                    }

                    case UP:
                    case DOWN:
                    case SELECT:
                    {
                        if (paintState->saveMenu == PICK_SLOT_SAVE_LOAD)
                        {
                            // Toggle between Save / Load
                            paintState->isSaveSelected = !paintState->isSaveSelected;
                        }
                        break;
                    }

                    case LEFT:
                    {
                        if (paintState->saveMenu == PICK_SLOT_SAVE_LOAD)
                        {
                            // Previous save slot
                            paintState->selectedSlot = (paintState->selectedSlot == 0) ? PAINT_SAVE_SLOTS - 1 : paintState->selectedSlot - 1;
                        }
                        else if (paintState->saveMenu == CONFIRM_OVERWRITE)
                        {
                            // Toggle Yes / No
                            paintState->overwriteYesSelected = !paintState->overwriteYesSelected;
                        }
                        break;
                    }

                    case RIGHT:
                    {
                        if (paintState->saveMenu == PICK_SLOT_SAVE_LOAD)
                        {
                            // Next save slot
                            // TODO: Skip empty slots
                            paintState->selectedSlot = (paintState->selectedSlot + 1) % PAINT_SAVE_SLOTS;
                        }
                        else if (paintState->saveMenu == CONFIRM_OVERWRITE)
                        {
                            // Toggle Yes / No
                            paintState->overwriteYesSelected = !paintState->overwriteYesSelected;
                        }
                        break;
                    }

                    case BTN_B:
                    {
                        // Exit save menu
                        paintState->saveMenu = HIDDEN;
                        paintState->buttonMode = BTN_MODE_DRAW;
                        break;
                    }

                    case START:
                    // Handle this in button up
                    break;
                }
            }
        }
        else
        {
            if (paintState->buttonMode == BTN_MODE_SELECT)
            {
                //////// Select-mode button release
                switch (evt->button)
                {
                    case SELECT:
                    {
                        // Exit select mode
                        paintState->buttonMode = BTN_MODE_DRAW;

                        // Set the current selection as the FG color and rearrange the rest
                        paintUpdateRecents(paintState->paletteSelect);
                        paintState->paletteSelect = 0;

                        paintState->redrawToolbar = true;
                        break;
                    }

                    case UP:
                    {
                        // Select previous color
                        paintState->redrawToolbar = true;
                        if (paintState->paletteSelect == 0)
                        {
                            paintState->paletteSelect = PAINT_MAX_COLORS - 1;
                        } else {
                            paintState->paletteSelect -= 1;
                        }
                        break;
                    }

                    case DOWN:
                    {
                        // Select next color
                        paintState->redrawToolbar = true;
                        paintState->paletteSelect = (paintState->paletteSelect + 1) % PAINT_MAX_COLORS;
                        break;
                    }

                    case LEFT:
                    {
                        // Select previous brush
                        paintPrevTool();
                        paintState->redrawToolbar = true;
                        break;
                    }

                    case RIGHT:
                    {
                        // Select next brush
                        paintNextTool();
                        paintState->redrawToolbar = true;
                        break;
                    }


                    case BTN_A:
                    {
                        // Increase brush size / next variant
                        paintIncBrushWidth();
                        paintState->redrawToolbar = true;
                        break;
                    }

                    case BTN_B:
                    {
                        // Decrease brush size / prev variant
                        paintDecBrushWidth();
                        paintState->redrawToolbar = true;
                        break;
                    }

                    case START:
                    // Start does nothing in select-mode, plus it's used for exit
                    break;
                }
            }
            else if (paintState->buttonMode == BTN_MODE_DRAW)
            {
                //////// Draw mode button release
                switch (evt->button)
                {
                    case START:
                    {
                        if (!paintState->saveInProgress)
                        {
                            // Enter the save menu
                            paintState->buttonMode = BTN_MODE_SAVE;
                            paintState->saveMenu = PICK_SLOT_SAVE_LOAD;
                            paintState->redrawToolbar = true;
                            paintState->isSaveSelected = true;
                        }
                        break;
                    }

                    case BTN_A:
                    {
                        // Stop drawing
                        paintState->aHeld = false;
                        break;
                    }

                    case BTN_B:
                    // Do nothing; color swap is handled on button down
                    break;

                    case UP:
                    case DOWN:
                    {
                        // Stop moving vertically
                        paintState->moveY = 0;

                        // Reset the button hold time, but only if we're not holding another direction
                        // This lets you make turns quickly instead of waiting for the repeat timeout in the middle
                        if (!paintState->moveX)
                        {
                            paintState->btnHoldTime = 0;
                        }
                        break;
                    }

                    case LEFT:
                    case RIGHT:
                    {
                        // Stop moving horizontally
                        paintState->moveX = 0;

                        if (!paintState->moveY)
                        {
                            paintState->btnHoldTime = 0;
                        }
                        break;
                    }

                    case SELECT:
                    {
                        paintState->isSaveSelected = true;
                        paintState->overwriteYesSelected = false;
                        paintState->buttonMode = BTN_MODE_SAVE;
                        paintState->redrawToolbar = true;
                    }
                    break;
                }
            }
            else if (paintState->buttonMode == BTN_MODE_SAVE)
            {
                //////// Save mode button release
                if (evt->button == START)
                {
                    // Exit save menu
                    paintState->saveMenu = HIDDEN;
                    paintState->buttonMode = BTN_MODE_DRAW;
                    paintState->redrawToolbar = true;
                    break;
                }
            }
        }
        break;
    }

    case PAINT_VIEW:
    break;

    case PAINT_HELP:
    break;

    case PAINT_GALLERY:
    break;

    case PAINT_SHARE:
    break;

    case PAINT_RECEIVE:
    break;
    }
}

// Util function implementations

void paintInitialize(void)
{
    resetMeleeMenu(paintState->menu, paintTitle, paintMainMenuCb);
    addRowToMeleeMenu(paintState->menu, menuOptDraw);
    addRowToMeleeMenu(paintState->menu, menuOptGallery);
    addRowToMeleeMenu(paintState->menu, menuOptReceive);
    addRowToMeleeMenu(paintState->menu, menuOptExit);

    paintState->screen = PAINT_MENU;

    paintState->clearScreen = true;

    paintLoadIndex();

    for (uint8_t i = 0; i < PAINT_SAVE_SLOTS; i++)
    {
        if (paintGetSlotInUse(i))
        {
            paintState->isSaveSelected = false;
            paintState->doSave = true;
            break;
        }
    }

    paintState->canvasW = PAINT_CANVAS_WIDTH;
    paintState->canvasH = PAINT_CANVAS_HEIGHT;

    paintState->brush = &(brushes[0]);
    paintState->brushWidth = 1;

    paintState->cursorWsg = &cursorBoxWsg;

    initPxStack(&paintState->pxStack);
    initPxStack(&paintState->cursorPxs);

    paintState->disp->clearPx();

    paintState->fgColor = c000; // black
    paintState->bgColor = c555; // white

    // pick some colors to start with
    paintState->recentColors[0] = c000; // black
    paintState->recentColors[1] = c555; // white
    paintState->recentColors[2] = c222; // light gray
    paintState->recentColors[3] = c444; // dark gray
    paintState->recentColors[4] = c500; // red
    paintState->recentColors[5] = c550; // yellow
    paintState->recentColors[6] = c050; // green
    paintState->recentColors[7] = c055; // cyan

    paintState->recentColors[8] = c005; // blue
    paintState->recentColors[9] = c530; // orange?
    paintState->recentColors[10] = c505; // fuchsia
    paintState->recentColors[11] = c503; // idk
    paintState->recentColors[12] = c350;
    paintState->recentColors[13] = c035;
    paintState->recentColors[14] = c522;
    paintState->recentColors[15] = c103;

    PAINT_LOGI("It's paintin' time! Canvas is %d x %d pixels!", paintState->canvasW, paintState->canvasH);

}

void paintMainMenuCb(const char* opt)
{
    if (opt == menuOptDraw)
    {
        PAINT_LOGI("Selected Draw");
        paintState->screen = PAINT_DRAW;
    }
    else if (opt == menuOptGallery)
    {
        PAINT_LOGI("Selected Gallery");
        paintState->screen = PAINT_GALLERY;
    }
    else if (opt == menuOptReceive)
    {
        PAINT_LOGI("Selected Receive");
        paintState->screen = PAINT_RECEIVE;
    }
    else if (opt == menuOptExit)
    {
        PAINT_LOGI("Selected Exit");
        switchToSwadgeMode(&modeMainMenu);
    }
}

void paintClearCanvas(void)
{
    fillDisplayArea(paintState->disp, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_X_OFFSET + paintState->canvasW * PAINT_CANVAS_SCALE, PAINT_CANVAS_Y_OFFSET + paintState->canvasH * PAINT_CANVAS_SCALE, paintState->bgColor);
}

void paintRenderAll(void)
{
    paintRenderToolbar();
    paintRenderCursor();
}

void setCursor(const wsg_t* cursorWsg)
{
    restoreCursorPixels();
    paintState->cursorWsg = cursorWsg;
    paintRenderCursor();
}

void restoreCursorPixels(void)
{
    PAINT_LOGV("Restoring pixels underneath cursor...");
    while (popPx(&paintState->cursorPxs, paintState->disp));
}

void plotCursor(void)
{
    restoreCursorPixels();
    paintDrawWsgTemp(paintState->disp, paintState->cursorWsg, &paintState->cursorPxs, CNV2SCR_X(paintState->cursorX) + PAINT_CANVAS_SCALE / 2 - paintState->cursorWsg->w / 2, CNV2SCR_Y(paintState->cursorY) + PAINT_CANVAS_SCALE / 2 - paintState->cursorWsg->h / 2, getContrastingColor);
}

void paintRenderCursor(void)
{
    if (paintState->showCursor)
    {
        if (paintState->lastCursorX != paintState->cursorX || paintState->lastCursorY != paintState->cursorY)
        {
            // Draw the cursor
            plotCursor();
        }
    }
}

void enableCursor(void)
{
    if (!paintState->showCursor)
    {
        paintState->showCursor = true;

        plotCursor();
    }
}

void disableCursor(void)
{
    if (paintState->showCursor)
    {
        restoreCursorPixels();

        paintState->showCursor = false;
    }
}

void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col)
{
    fillDisplayArea(disp, x0, y0, x1 - 1, y1 - 1, col);
}

void plotRectFilledScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    fillDisplayArea(disp, xTr + x0 * xScale, yTr + y0 * yScale, xTr + (x1) * xScale, yTr + (y1) * yScale, col);
}

void setPxScaled(display_t* disp, int x, int y, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    plotRectFilledScaled(disp, x, y, x + 1, y + 1, col, xTr, yTr, xScale, yScale);
}

void pushPxScaled(pxStack_t* pxStack, display_t* disp, int x, int y, int xTr, int yTr, int xScale, int yScale)
{
    for (int i = 0; i < xScale * yScale; i++)
    {
        pushPx(pxStack, disp, xTr + x * xScale + i % yScale, yTr + y * yScale + i / xScale);
    }
}

void popPxScaled(pxStack_t* pxStack, display_t* disp, int xScale, int yScale)
{
    for (int i = 0; i < xScale * yScale; i++)
    {
        popPx(pxStack, disp);
    }
}

void drawColorBox(uint16_t xOffset, uint16_t yOffset, uint16_t w, uint16_t h, paletteColor_t col, bool selected, paletteColor_t topBorder, paletteColor_t bottomBorder)
{
    int dashLen = selected ? 1 : 0;
    if (selected)
    {
        topBorder = c000;
        bottomBorder = c000;
    }

    if (col == cTransparent)
    {
        // Draw a lil checkerboard
        fillDisplayArea(paintState->disp, xOffset, yOffset, xOffset + w / 2, yOffset + h / 2, c111);
        fillDisplayArea(paintState->disp, xOffset + w / 2, yOffset, xOffset + w, yOffset + h / 2, c555);
        fillDisplayArea(paintState->disp, xOffset, yOffset + h / 2, xOffset + w / 2, yOffset + h, c555);
        fillDisplayArea(paintState->disp, xOffset + w / 2, yOffset + h / 2, xOffset + w, yOffset + h, c111);
    }
    else
    {
        fillDisplayArea(paintState->disp, xOffset, yOffset, xOffset + w, yOffset + h, col);
    }

    if (topBorder != cTransparent)
    {
        // Top border
        plotLine(paintState->disp, xOffset - 1,  yOffset, xOffset + w - 1, yOffset, topBorder, dashLen);
        // Left border
        plotLine(paintState->disp, xOffset - 1, yOffset, xOffset - 1, yOffset + h - 1, topBorder, dashLen);
    }

    if (bottomBorder != cTransparent)
    {
        // Bottom border
        plotLine(paintState->disp, xOffset, yOffset + h, xOffset + w - 1, yOffset + h, bottomBorder, dashLen);
        // Right border
        plotLine(paintState->disp, xOffset + w, yOffset + 1, xOffset + w, yOffset + h - 1, bottomBorder, dashLen);
    }
}

void paintRenderToolbar(void)
{
    //////// Background

    // Fill top bar
    fillDisplayArea(paintState->disp, 0, 0, paintState->disp->w, PAINT_CANVAS_Y_OFFSET, PAINT_TOOLBAR_BG);

    // Fill left side bar
    fillDisplayArea(paintState->disp, 0, 0, PAINT_CANVAS_X_OFFSET, paintState->disp->h, PAINT_TOOLBAR_BG);

    // Fill right bar, if there's room
    if (PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_HEIGHT * PAINT_CANVAS_SCALE < paintState->disp->w)
    {
        fillDisplayArea(paintState->disp, PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_WIDTH * PAINT_CANVAS_SCALE, 0, paintState->disp->w, paintState->disp->h, PAINT_TOOLBAR_BG);
    }

    // Fill bottom bar, if there's room
    if (PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT * PAINT_CANVAS_SCALE < paintState->disp->h)
    {
        fillDisplayArea(paintState->disp, 0, PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT * PAINT_CANVAS_SCALE, paintState->disp->w, paintState->disp->h, PAINT_TOOLBAR_BG);

        // Draw a black rectangle under where the exit progress bar will be so it can be seen
        fillDisplayArea(paintState->disp, 0, paintState->disp->h - 11, paintState->disp->w, paintState->disp->h, c000);
    }

    // Draw border around canvas
    plotRect(paintState->disp, PAINT_CANVAS_X_OFFSET - 1, PAINT_CANVAS_Y_OFFSET - 1, PAINT_CANVAS_X_OFFSET + paintState->canvasW * PAINT_CANVAS_SCALE + 1, PAINT_CANVAS_Y_OFFSET + paintState->canvasH * PAINT_CANVAS_SCALE + 1, c000);


    //////// Draw the active FG/BG colors and the color palette

    //////// Active Colors
    // Draw the background color, then draw the foreground color overlapping it and offset by half in both directions
    drawColorBox(PAINT_ACTIVE_COLOR_X, PAINT_ACTIVE_COLOR_Y, PAINT_COLORBOX_W, PAINT_COLORBOX_H, paintState->bgColor, false, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
    drawColorBox(PAINT_ACTIVE_COLOR_X + PAINT_COLORBOX_W / 2, PAINT_ACTIVE_COLOR_Y + PAINT_COLORBOX_H / 2, PAINT_COLORBOX_W, PAINT_COLORBOX_H, paintState->fgColor, false, cTransparent, PAINT_COLORBOX_SHADOW_BOTTOM);

    //////// Recent Colors (palette)
    for (int i = 0; i < PAINT_MAX_COLORS; i++)
    {
        if (PAINT_COLORBOX_HORIZONTAL)
        {
            drawColorBox(PAINT_COLORBOX_X + i * (PAINT_COLORBOX_MARGIN_LEFT + PAINT_COLORBOX_W), PAINT_COLORBOX_Y, PAINT_COLORBOX_W, PAINT_COLORBOX_H, paintState->recentColors[i], paintState->buttonMode == BTN_MODE_SELECT && paintState->paletteSelect == i, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
        }
        else
        {
            drawColorBox(PAINT_COLORBOX_X, PAINT_COLORBOX_Y + i * (PAINT_COLORBOX_MARGIN_TOP + PAINT_COLORBOX_H), PAINT_COLORBOX_W, PAINT_COLORBOX_H, paintState->recentColors[i], paintState->buttonMode == BTN_MODE_SELECT && paintState->paletteSelect == i, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
        }
    }


    if (paintState->saveMenu == HIDDEN)
    {
        //////// Tools

        // Draw the brush name
        uint16_t textX = 30, textY = 4;
        drawText(paintState->disp, &paintState->toolbarFont, c000, paintState->brush->name, textX, textY);

        // Draw the brush size, if applicable and not constant
        char text[16];
        if (paintState->brush->minSize > 0 && paintState->brush->maxSize > 0 && paintState->brush->minSize != paintState->brush->maxSize)
        {
            snprintf(text, sizeof(text), "%d", paintState->brushWidth);
            drawText(paintState->disp, &paintState->toolbarFont, c000, text, 200, 4);
        }

        if (paintState->brush->mode == PICK_POINT && paintState->brush->maxPoints > 1)
        {
            // Draw the number of picks made / total
            snprintf(text, sizeof(text), "%zu/%d", paintState->pickCount, paintState->brush->maxPoints);
            drawText(paintState->disp, &paintState->toolbarFont, c000, text, 220, 4);
        }
        else if (paintState->brush->mode == PICK_POINT_LOOP && paintState->brush->maxPoints > 1)
        {
            // Draw the number of remaining picks
            uint8_t maxPicks = paintState->brush->maxPoints < MAX_PICK_POINTS ? paintState->brush->maxPoints : MAX_PICK_POINTS;

            if (paintState->pickCount + 1 == maxPicks - 1)
            {
                snprintf(text, sizeof(text), "Last");
            }
            else
            {
                snprintf(text, sizeof(text), "%zu", maxPicks - paintState->pickCount - 1);
            }

            drawText(paintState->disp, &paintState->toolbarFont, c000, text, 220, 4);
        }
    }
    else if (paintState->saveMenu == PICK_SLOT_SAVE_LOAD)
    {
        uint16_t textX = 30, textY = 4;

        // Draw "Save" / "Load"
        drawText(paintState->disp, &paintState->toolbarFont, c000, paintState->isSaveSelected ? startMenuSave : startMenuLoad, textX, textY);

        // Draw the slot number
        char text[16];
        snprintf(text, sizeof(text), (paintState->isSaveSelected && paintGetSlotInUse(paintState->selectedSlot)) ? startMenuSlotUsed : startMenuSlot, paintState->selectedSlot + 1);
        drawText(paintState->disp, &paintState->toolbarFont, c000, text, 160, textY);
    }
    else if (paintState->saveMenu == CONFIRM_OVERWRITE)
    {
        // Draw "Overwrite?"
        drawText(paintState->disp, &paintState->toolbarFont, c000, startMenuOverwrite, 30, 4);

        // Draw "Yes" / "No"
        drawText(paintState->disp, &paintState->toolbarFont, c000, paintState->overwriteYesSelected ? startMenuYes : startMenuNo, 160, 4);
    }
}


// adapted from http://www.adammil.net/blog/v126_A_More_Efficient_Flood_Fill.html
void floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t col)
{
    if (disp->getPx(x, y) == col)
    {
        // makes no sense to fill with the same color, so just don't
        return;
    }

    _floodFill(disp, x, y, disp->getPx(x, y), col);
}

void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill)
{
    // at this point, we know array[y,x] is clear, and we want to move as far as possible to the upper-left. moving
    // up is much more important than moving left, so we could try to make this smarter by sometimes moving to
    // the right if doing so would allow us to move further up, but it doesn't seem worth the complexity
    while(true)
    {
        uint16_t ox = x, oy = y;
        while(y != PAINT_CANVAS_Y_OFFSET && disp->getPx(x, y-1) == search) y--;
        while(x != PAINT_CANVAS_X_OFFSET && disp->getPx(x-1, y) == search) x--;
        if(x == ox && y == oy) break;
    }
    _floodFillInner(disp, x, y, search, fill);
}


void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill)
{
    // at this point, we know that array[y,x] is clear, and array[y-1,x] and array[y,x-1] are set.
    // we'll begin scanning down and to the right, attempting to fill an entire rectangular block
    uint16_t lastRowLength = 0; // the number of cells that were clear in the last row we scanned
    do
    {
        uint16_t rowLength = 0, sx = x; // keep track of how long this row is. sx is the starting x for the main scan below
        // now we want to handle a case like |***|, where we fill 3 cells in the first row and then after we move to
        // the second row we find the first  | **| cell is filled, ending our rectangular scan. rather than handling
        // this via the recursion below, we'll increase the starting value of 'x' and reduce the last row length to
        // match. then we'll continue trying to set the narrower rectangular block
        if(lastRowLength != 0 && disp->getPx(x, y) != search) // if this is not the first row and the leftmost cell is filled...
        {
            do
            {
                if(--lastRowLength == 0) return; // shorten the row. if it's full, we're done
            } while(disp->getPx(++x, y) != search); // otherwise, update the starting point of the main scan to match
            sx = x;
        }
        // we also want to handle the opposite case, | **|, where we begin scanning a 2-wide rectangular block and
        // then find on the next row that it has     |***| gotten wider on the left. again, we could handle this
        // with recursion but we'd prefer to adjust x and lastRowLength instead
        else
        {
            for(; x != PAINT_CANVAS_X_OFFSET && disp->getPx(x-1, y) == search; rowLength++, lastRowLength++)
            {
                disp->setPx(--x, y, fill); // to avoid scanning the cells twice, we'll fill them and update rowLength here
                // if there's something above the new starting point, handle that recursively. this deals with cases
                // like |* **| when we begin filling from (2,0), move down to (2,1), and then move left to (0,1).
                // the  |****| main scan assumes the portion of the previous row from x to x+lastRowLength has already
                // been filled. adjusting x and lastRowLength breaks that assumption in this case, so we must fix it
                if(y != PAINT_CANVAS_Y_OFFSET && disp->getPx(x, y-1) == search) _floodFill(disp, x, y-1, search, fill); // use _Fill since there may be more up and left
            }
        }

        // now at this point we can begin to scan the current row in the rectangular block. the span of the previous
        // row from x (inclusive) to x+lastRowLength (exclusive) has already been filled, so we don't need to
        // check it. so scan across to the right in the current row
        for(; sx < PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_WIDTH * PAINT_CANVAS_SCALE && disp->getPx(sx, y) == search; rowLength++, sx++) disp->setPx(sx, y, fill);
        // now we've scanned this row. if the block is rectangular, then the previous row has already been scanned,
        // so we don't need to look upwards and we're going to scan the next row in the next iteration so we don't
        // need to look downwards. however, if the block is not rectangular, we may need to look upwards or rightwards
        // for some portion of the row. if this row was shorter than the last row, we may need to look rightwards near
        // the end, as in the case of |*****|, where the first row is 5 cells long and the second row is 3 cells long.
        // we must look to the right  |*** *| of the single cell at the end of the second row, i.e. at (4,1)
        if(rowLength < lastRowLength)
        {
            for(int end=x+lastRowLength; ++sx < end; ) // 'end' is the end of the previous row, so scan the current row to
            {   // there. any clear cells would have been connected to the previous
                if(disp->getPx(sx, y) == search) _floodFillInner(disp, sx, y, search, fill); // row. the cells up and left must be set so use FillCore
            }
        }
        // alternately, if this row is longer than the previous row, as in the case |*** *| then we must look above
        // the end of the row, i.e at (4,0)                                         |*****|
        else if(rowLength > lastRowLength && y != PAINT_CANVAS_Y_OFFSET) // if this row is longer and we're not already at the top...
        {
            for(int ux=x+lastRowLength; ++ux<sx; ) // sx is the end of the current row
            {
                if(disp->getPx(ux, y-1) == search) _floodFill(disp, ux, y-1, search, fill); // since there may be clear cells up and left, use _Fill
            }
        }
        lastRowLength = rowLength; // record the new row length
    } while(lastRowLength != 0 && ++y < PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT * PAINT_CANVAS_SCALE); // if we get to a full row or to the bottom, we're done
}

void paintDrawSquarePen(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotRectFilledScaled(disp, points[0].x, points[0].y, points[0].x + (size), points[0].y + (size), col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawCirclePen(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    // Add one to the size because it isn't really a circle at r=1
    plotCircleFilledScaled(disp, points[0].x, points[0].y, size + 1, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawLine(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    PAINT_LOGD("Drawing line from (%d, %d) to (%d, %d)", points[0].x, points[0].y, points[1].x, points[1].y);
    plotLineScaled(paintState->disp, points[0].x, points[0].y, points[1].x, points[1].y, col, 0, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawCurve(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotCubicBezierScaled(disp, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawRectangle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t x0 = (points[0].x > points[1].x) ? points[1].x : points[0].x;
    uint16_t y0 = (points[0].y > points[1].y) ? points[1].y : points[0].y;
    uint16_t x1 = (points[0].x > points[1].x) ? points[0].x : points[1].x;
    uint16_t y1 = (points[0].y > points[1].y) ? points[0].y : points[1].y;

    plotRectScaled(disp, x0, y0, x1 + 1, y1 + 1, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawFilledRectangle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t x0 = (points[0].x > points[1].x) ? points[1].x : points[0].x;
    uint16_t y0 = (points[0].y > points[1].y) ? points[1].y : points[0].y;
    uint16_t x1 = (points[0].x > points[1].x) ? points[0].x : points[1].x;
    uint16_t y1 = (points[0].y > points[1].y) ? points[0].y : points[1].y;

    // This function takes care of its own scaling because it's very easy and will save a lot of unnecessary draws
    plotRectFilledScaled(disp, x0, y0, x1 + 1, y1 + 1, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawCircle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t dX = abs(points[0].x - points[1].x);
    uint16_t dY = abs(points[0].y - points[1].y);
    uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);

    plotCircleScaled(disp, points[0].x, points[0].y, r, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawFilledCircle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t dX = abs(points[0].x - points[1].x);
    uint16_t dY = abs(points[0].y - points[1].y);
    uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);

    plotCircleFilledScaled(disp, points[0].x, points[0].y, r, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawEllipse(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    // for some reason, plotting an ellipse also plots 2 extra points outside of the ellipse
    // let's just work around that
    pushPxScaled(&paintState->pxStack, disp, (points[0].x < points[1].x ? points[0].x : points[1].x) + (abs(points[1].x - points[0].x) + 1) / 2, points[0].y < points[1].y ? points[0].y - 2 : points[1].y - 2, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
    pushPxScaled(&paintState->pxStack, disp, (points[0].x < points[1].x ? points[0].x : points[1].x) + (abs(points[1].x - points[0].x) + 1) / 2, points[0].y < points[1].y ? points[1].y + 2 : points[0].y + 2, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);

    plotEllipseRectScaled(disp, points[0].x, points[0].y, points[1].x, points[1].y, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);

    for (uint8_t i = 0; i < PAINT_CANVAS_SCALE * PAINT_CANVAS_SCALE * 2; i++)
    {
        popPx(&paintState->pxStack, disp);
    }
}

void paintDrawPolygon(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    for (uint8_t i = 0; i < numPoints - 1; i++)
    {
        plotLineScaled(disp, points[i].x, points[i].y, points[i+1].x, points[i+1].y, col, 0, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
    }
}

void paintPlotSquareWave(display_t* disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale)
{
    // swap so x0 < x1 && y0 < y1
    if (x0 > x1)
    {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
    }

    if (y0 > y1)
    {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
    }

    // use the shortest axis as the wave size
    uint16_t waveSize = (x1 - x0 < y1 - y0) ? x1 - x0 : y1 - y0;

    uint16_t x = x0;
    uint16_t y = y0;
    uint16_t stop, extra;

    int16_t xDir = (x0 < x1) ? 1 : -1;
    int16_t yDir = (y0 < y1) ? 1 : -1;

    if (waveSize < 2)
    {
        // TODO: Draw a rectangular wave if a real square wave would be too small?
        // (a 2xN square wave is just a 2-thick line)
        return;
    }

    if (x1 - x0 > y1 - y0)
    {
        // Horizontal
        extra = (x1 - x0 + 1) % (waveSize * 2);
        stop = x + waveSize * xDir - extra / 2;

        while (x != x1)
        {
            PAINT_LOGV("Squarewave H -- (%d, %d)", x, y);
            setPxScaled(disp, x, y, col, xTr, yTr, xScale, yScale);

            if (x == stop)
            {
                plotLineScaled(disp, x, y, x, y + yDir * waveSize, col, 0, xTr, yTr, xScale, yScale);
                y += yDir * waveSize;
                yDir = -yDir;
                stop = x + waveSize * xDir;
            }

            x+= xDir;
        }
    }
    else
    {
        // Vertical
        extra = (x1 - x0 + 1) % (waveSize * 2);
        stop = y + waveSize * yDir - extra / 2;

        while (y != y1)
        {
            PAINT_LOGV("Squarewave V -- (%d, %d)", x, y);
            setPxScaled(disp, x, y, col, xTr, yTr, xScale, yScale);

            if (y == stop)
            {
                plotLineScaled(disp, x, y, x + xDir * waveSize, y, col, 0, xTr, yTr, xScale, yScale);
                x += xDir * waveSize;
                xDir = -xDir;
                stop = y + waveSize * yDir;
            }

            y += yDir;
        }
    }
}

void paintDrawSquareWave(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    paintPlotSquareWave(disp, points[0].x, points[0].y, points[1].x, points[1].y, col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
}

void paintDrawPaintBucket(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    floodFill(disp, CNV2SCR_X(points[0].x), CNV2SCR_Y(points[0].y), col);
}

void paintDrawClear(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    // No need to translate here, so only draw once
    fillDisplayArea(disp, CNV2SCR_X(0), CNV2SCR_Y(0), CNV2SCR_X(PAINT_CANVAS_WIDTH), CNV2SCR_Y(PAINT_CANVAS_HEIGHT), col);
}

void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col)
{
    disableCursor();
    bool drawNow = false;
    bool lastPick = false;

    // Determine if this is the last pick for the tool
    // This is so we don't draw a pick-marker that will be immediately removed
    switch (paintState->brush->mode)
    {
        case PICK_POINT:
        lastPick = (paintState->pickCount + 1 == paintState->brush->maxPoints);
        break;

        case PICK_POINT_LOOP:
        lastPick = paintState->pickCount + 1 == MAX_PICK_POINTS - 1 || paintState->pickCount + 1 == paintState->brush->maxPoints - 1;
        break;

        case HOLD_DRAW:
        break;

        case INSTANT:
        break;

        default:
        break;
    }

    if (paintState->brush->mode == HOLD_DRAW)
    {
        paintState->pickPoints[0].x = x;
        paintState->pickPoints[0].y = y;
        paintState->pickCount = 1;

        drawNow = true;
    }
    else if (paintState->brush->mode == PICK_POINT || paintState->brush->mode == PICK_POINT_LOOP)
    {
        paintState->pickPoints[paintState->pickCount].x = x;
        paintState->pickPoints[paintState->pickCount].y = y;

        PAINT_LOGD("pick[%02zu] = SCR(%03d, %03d) / CNV(%d, %d)", paintState->pickCount, CNV2SCR_X(x), CNV2SCR_Y(y), x, y);

        // Save the pixel underneath the selection, then draw a temporary pixel to mark it
        // But don't bother if this is the last pick point, since it will never actually be seen
        if (!lastPick)
        {
            pushPxScaled(&paintState->pxStack, paintState->disp, x, y, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
            setPxScaled(paintState->disp, x, y, paintState->fgColor, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
        }

        if (paintState->brush->mode == PICK_POINT_LOOP)
        {
            if (paintState->pickCount > 0 && paintState->pickPoints[0].x == x && paintState->pickPoints[0].y == y)
            {
                // If this isn't the first pick, and it's in the same position as the first pick, we're done!
                drawNow = true;
            }
            else if (lastPick)
            {
                // Special case: If we're on the next-to-last possible point, we have to add the start again as the last point
                ++paintState->pickCount;

                paintState->pickPoints[paintState->pickCount].x = paintState->pickPoints[0].x;
                paintState->pickPoints[paintState->pickCount].y = paintState->pickPoints[0].y;


                PAINT_LOGD("pick[%02zu] = (%03d, %03d) (last!)", paintState->pickCount, paintState->pickPoints[0].x, paintState->pickPoints[0].y);

                drawNow = true;
            }

            ++paintState->pickCount;
        }
        // only for non-loop brushes
        else if (++paintState->pickCount == paintState->brush->maxPoints)
        {
            drawNow = true;
        }
    }
    else if (paintState->brush->mode == INSTANT)
    {
        drawNow = true;
    }

    if (drawNow)
    {
        // Restore the pixels under the pick markers BEFORE the tool draws
        if (paintState->brush->mode == PICK_POINT || paintState->brush->mode == PICK_POINT_LOOP)
        {
            for (size_t i = 0; i < (paintState->pickCount - 1) * PAINT_CANVAS_SCALE * PAINT_CANVAS_SCALE; i++)
            {
                popPx(&paintState->pxStack, paintState->disp);
            }
        }


        paintState->brush->fnDraw(paintState->disp, paintState->pickPoints, paintState->pickCount, paintState->brushWidth, col);

        paintState->pickCount = 0;
    }

    enableCursor();
    paintRenderToolbar();
}

void initPxStack(pxStack_t* pxStack)
{
    pxStack->size = PIXEL_STACK_MIN_SIZE;
    PAINT_LOGD("Allocating pixel stack with size %zu", pxStack->size);
    pxStack->data = malloc(sizeof(pxVal_t) * pxStack->size);
    pxStack->index = -1;
}

void freePxStack(pxStack_t* pxStack)
{
    if (pxStack->data != NULL)
    {
        free(pxStack->data);
        PAINT_LOGD("Freed pixel stack");
        pxStack->size = 0;
        pxStack->index = -1;
    }
}

void maybeGrowPxStack(pxStack_t* pxStack)
{
    if (pxStack->index >= pxStack->size)
    {
        pxStack->size *= 2;
        PAINT_LOGD("Expanding pixel stack to size %zu", pxStack->size);
        pxStack->data = realloc(pxStack->data, sizeof(pxVal_t) * pxStack->size);
    }
}

void maybeShrinkPxStack(pxStack_t* pxStack)
{
    // If the stack is at least 4 times bigger than it needs to be, shrink it by half
    // (but only if the stack is bigger than the minimum)
    if (pxStack->index >= 0 && pxStack->index * 4 <= pxStack->size && pxStack->size > PIXEL_STACK_MIN_SIZE)
    {
        pxStack->size /= 2;
        PAINT_LOGD("Shrinking pixel stack to %zu", pxStack->size);
        pxStack->data = realloc(pxStack->data, sizeof(pxVal_t) * pxStack->size);
        PAINT_LOGD("Done shrinking pixel stack");
    }
}

/**
 * The color at the given pixel coordinates is pushed onto the pixel stack,
 * along with its coordinates. If the pixel stack is uninitialized, it will
 * be allocated. If the pixel stack is full, its size will be doubled.
 *
 * @brief Pushes a pixel onto the pixel stack so that it can be restored later
 * @param x The screen X coordinate of the pixel to save
 * @param y The screen Y coordinate of the pixel to save
 *
 */
void pushPx(pxStack_t* pxStack, display_t* disp, uint16_t x, uint16_t y)
{
    pxStack->index++;

    maybeGrowPxStack(pxStack);

    pxStack->data[pxStack->index].x = x;
    pxStack->data[pxStack->index].y = y;
    pxStack->data[pxStack->index].col = disp->getPx(x, y);

    PAINT_LOGV("Saved pixel %d at (%d, %d)", pxStack->index, x, y);
}

/**
 * Removes the pixel from the top of the stack and draws its color at its coordinates.
 * If the stack is already empty, no pixels will be drawn. If the pixel stack's size is
 * at least 4 times its number of entries, its size will be halved, at most to the minimum size.
 * Returns `true` if a value was popped, and `false` if the stack was empty and no value was popped.
 *
 * @brief Pops a pixel from the stack and restores it to the screen
 * @return `true` if a pixel was popped, and `false` if the stack was empty.
 */
bool popPx(pxStack_t* pxStack, display_t* disp)
{
    // Make sure the stack isn't empty
    if (pxStack->index >= 0)
    {
        PAINT_LOGV("Popping pixel %d...", pxStack->index);

        // Draw the pixel from the top of the stack
        disp->setPx(pxStack->data[pxStack->index].x, pxStack->data[pxStack->index].y, pxStack->data[pxStack->index].col);
        pxStack->index--;

        // Is this really necessary? The stack empties often so maybe it's better not to reallocate constantly
        //maybeShrinkPxStack(pxStack);

        return true;
    }

    return false;
}

void paintSetupTool(void)
{
    paintState->brush = &(brushes[paintState->brushIndex]);

    // Reset the pick state
    paintState->pickCount = 0;

    if (paintState->brushWidth < paintState->brush->minSize)
    {
        paintState->brushWidth = paintState->brush->minSize;
    }
    else if (paintState->brushWidth > paintState->brush->maxSize)
    {
        paintState->brushWidth = paintState->brush->maxSize;
    }

    // Remove any stored temporary pixels
    while (popPx(&paintState->pxStack, paintState->disp));
}

void paintPrevTool(void)
{
    if (paintState->brushIndex > 0)
    {
        paintState->brushIndex--;
    }
    else
    {
        paintState->brushIndex = sizeof(brushes) / sizeof(brush_t) - 1;
    }

    paintSetupTool();
}

void paintNextTool(void)
{
    if (paintState->brushIndex < sizeof(brushes) / sizeof(brush_t) - 1)
    {
        paintState->brushIndex++;
    }
    else
    {
        paintState->brushIndex = 0;
    }

    paintSetupTool();
}

void paintDecBrushWidth()
{
    if (paintState->brushWidth == 0 || paintState->brushWidth <= paintState->brush->minSize)
    {
        paintState->brushWidth = paintState->brush->minSize;
    }
    else
    {
        paintState->brushWidth--;
    }
}

void paintIncBrushWidth()
{
    paintState->brushWidth++;

    if (paintState->brushWidth > paintState->brush->maxSize)
    {
        paintState->brushWidth = paintState->brush->maxSize;
    }
}

void paintUpdateRecents(uint8_t selectedIndex)
{
    paintState->fgColor = paintState->recentColors[selectedIndex];

    for (uint8_t i = selectedIndex; i > 0; i--)
    {
        paintState->recentColors[i] = paintState->recentColors[i-1];
    }
    paintState->recentColors[0] = paintState->fgColor;

    // If there are any pick points, update their color to reduce confusion
    paintDrawPickPoints();
}

void paintDrawPickPoints(void)
{
    for (uint8_t i = 0; i < paintState->pickCount; i++)
    {
        setPxScaled(paintState->disp, paintState->pickPoints[i].x, paintState->pickPoints[i].y, paintState->fgColor, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
    }
}

void paintHidePickPoints(void)
{
    for (uint8_t i = 0; i < (paintState->pxStack.index < paintState->pickCount) ? paintState->pxStack.index : paintState->pickCount; i++)
    {
        setPxScaled(paintState->disp, paintState->pickPoints[i].x, paintState->pickPoints[i].y, paintState->pxStack.data[i].col, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
    }
}

void paintDrawWsgTemp(display_t* disp, const wsg_t* wsg, pxStack_t* saveTo, uint16_t xOffset, uint16_t yOffset, colorMapFn_t colorSwap)
{
    size_t i = 0;
    for (uint16_t x = 0; x < wsg->w; x++)
    {
        for (uint16_t y = 0; y < wsg->h; y++, i++)
        {
            if (wsg->px[i] != cTransparent)
            {
                pushPx(saveTo, disp, xOffset + x, yOffset + y);

                disp->setPx(xOffset + x, yOffset + y, colorSwap ? colorSwap(disp->getPx(xOffset + x, yOffset + y)) : wsg->px[i]);
            }
            else
            {
                PAINT_LOGV("Skipping cursor[%d][%d] == cTransparent", x, y);
            }
        }
    }
}

void paintMoveCursorRel(int8_t xDiff, int8_t yDiff)
{
    paintState->lastCursorX = paintState->cursorX;
    paintState->lastCursorY = paintState->cursorY;

    // TODO: prevent overflow when bounds checking
    if (paintState->cursorX + xDiff < 0)
    {
        paintState->cursorX = 0;
    }
    else if (paintState->cursorX + xDiff >= paintState->canvasW)
    {
        paintState->cursorX = paintState->canvasW - 1;
    }
    else {
        paintState->cursorX += xDiff;
    }

    if (paintState->cursorY + yDiff < 0)
    {
        paintState->cursorY = 0;
    }
    else if (paintState->cursorY + yDiff >= paintState->canvasH)
    {
        paintState->cursorY = paintState->canvasH - 1;
    }
    else
    {
        paintState->cursorY += yDiff;
    }
}

void paintLoadIndex(void)
{
    //       SFX?
    //     BGM | Lights?
    //        \|/
    // |xxxxxvvv  |Recent?|  |Inuse? |
    // 0000 0000  0000 0000  0000 0000

    if (!readNvs32("pnt_idx", &paintState->index))
    {
        PAINT_LOGW("No metadata! Setting defaults");
        paintState->index = PAINT_DEFAULTS;
    }
}

void paintSaveIndex(void)
{
    if (writeNvs32("pnt_idx", paintState->index))
    {
        PAINT_LOGD("Saved index: %04x", paintState->index);
    }
    else
    {
        PAINT_LOGE("Failed to save index :(");
    }
}

void paintResetStorage(void)
{
    paintState->index = PAINT_DEFAULTS;
    paintSaveIndex();
}

bool paintGetSlotInUse(uint8_t slot)
{
    paintLoadIndex();
    return (paintState->index & (1 << slot)) != 0;
}

void paintSetSlotInUse(uint8_t slot)
{
    paintState->index |= (1 << slot);
    paintSaveIndex();
}

uint8_t paintGetRecentSlot(void)
{
    paintLoadIndex();
    return (paintState->index >> PAINT_SAVE_SLOTS) & 0b111;
}

void paintSetRecentSlot(uint8_t slot)
{
    // TODO if we change the number of slots this will totally not work anymore
    // I mean, we could just do & 0xFF and waste 5 bits
    paintState->index = (paintState->index & PAINT_MASK_NOT_RECENT) | ((slot & 0b111) << PAINT_SAVE_SLOTS);
    paintSaveIndex();
}

void paintSave(uint8_t slot)
{
    // palette in reverse for quick transformation
    uint8_t paletteIndex[256];

    // NVS blob key name
    char key[16];

    // pointer to converted image segment
    uint8_t* imgChunk = NULL;

    // Save the palette map, this lets us compact the image by 50%
    snprintf(key, 16, "paint_%02d_pal", slot);
    PAINT_LOGD("paletteColor_t size: %zu, max colors: %d", sizeof(paletteColor_t), PAINT_MAX_COLORS);
    PAINT_LOGD("Palette will take up %zu bytes", sizeof(paintState->recentColors));
    if (writeNvsBlob(key, paintState->recentColors, sizeof(paintState->recentColors)))
    {
        PAINT_LOGD("Saved palette to slot %s", key);
    }
    else
    {
        PAINT_LOGE("Could't save palette to slot %s", key);
        return;
    }

    // Save the canvas dimensions
    uint32_t packedSize = paintState->canvasW << 16 | paintState->canvasH;
    snprintf(key, 16, "paint_%02d_dim", slot);
    if (writeNvs32(key, packedSize))
    {
        PAINT_LOGD("Saved dimensions to slot %s", key);
    }
    else
    {
        PAINT_LOGE("Couldn't save dimensions to slot %s",key);
        return;
    }

    // Build the reverse-palette map
    for (uint16_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        paletteIndex[((uint8_t)paintState->recentColors[i])] = i;
    }

    // Allocate space for the chunk
    uint32_t totalPx = paintState->canvasH * paintState->canvasW;
    uint32_t finalChunkSize = totalPx % PAINT_SAVE_CHUNK_SIZE / 2;

    // don't skip the entire last chunk if it falls on a boundary
    if (finalChunkSize == 0)
    {
        finalChunkSize = PAINT_SAVE_CHUNK_SIZE;
    }

    // Add double the chunk size (pixels per chunk, really), minus one, so that if there are e.g. 2049 pixels,
    // it would become 4096 and round up to 2 chunks instead of 1
    uint8_t chunkCount = (totalPx + (PAINT_SAVE_CHUNK_SIZE * 2) - 1) / (PAINT_SAVE_CHUNK_SIZE * 2);
    if ((imgChunk = malloc(sizeof(uint8_t) * PAINT_SAVE_CHUNK_SIZE)) != NULL)
    {
        PAINT_LOGD("Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        PAINT_LOGE("malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    PAINT_LOGD("We will use %d chunks of size %dB (%d), plus one of %dB == %dB to save the image", chunkCount - 1, PAINT_SAVE_CHUNK_SIZE, (chunkCount - 1) * PAINT_SAVE_CHUNK_SIZE, finalChunkSize, (chunkCount - 1) * PAINT_SAVE_CHUNK_SIZE + finalChunkSize);
    PAINT_LOGD("The image is %d x %d px == %dpx, at 2px/B that's %dB", paintState->canvasW, paintState->canvasH, totalPx, totalPx / 2);

    disableCursor();

    uint16_t x0, y0, x1, y1;
    // Write all the chunks
    for (uint32_t i = 0; i < chunkCount; i++)
    {
        // build the chunk
        for (uint16_t n = 0; n < PAINT_SAVE_CHUNK_SIZE; n++)
        {
            // exit the last chunk early if needed
            if (i + 1 == chunkCount && n == finalChunkSize)
            {
                break;
            }

            // calculate the real coordinates given the pixel indices
            // (we store 2 pixels in each byte)
            // that's 100% more pixel, per pixel!
            x0 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) % paintState->canvasW * PAINT_CANVAS_SCALE;
            y0 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) / paintState->canvasW * PAINT_CANVAS_SCALE;
            x1 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) % paintState->canvasW * PAINT_CANVAS_SCALE;
            y1 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) / paintState->canvasW * PAINT_CANVAS_SCALE;

            // we only need to save the top-left pixel of each scaled pixel, since they're the same unless something is very broken
            imgChunk[n] = paletteIndex[(uint8_t)paintState->disp->getPx(x0, y0)] << 4 | paletteIndex[(uint8_t)paintState->disp->getPx(x1, y1)];
        }

        // save the chunk
        snprintf(key, 16, "paint_%02dc%05d", slot, i);
        if (writeNvsBlob(key, imgChunk, (i + 1 < chunkCount) ? PAINT_SAVE_CHUNK_SIZE : finalChunkSize))
        {
            PAINT_LOGD("Saved blob %d of %d", i+1, chunkCount);
        }
        else
        {
            PAINT_LOGE("Unable to save blob %d of %d", i+1, chunkCount);
            free(imgChunk);

            enableCursor();
            return;
        }
    }

    paintSetSlotInUse(slot);
    paintSetRecentSlot(slot);
    paintSaveIndex();

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();
}

void paintLoad(uint8_t slot)
{
    // NVS blob key name
    char key[16];

    // pointer to converted image segment
    uint8_t* imgChunk = NULL;

    // read the palette and load it into the recentColors
    // read the dimensions and do the math
    // read the pixels

    size_t paletteSize;

    if (!paintGetSlotInUse(slot))
    {
        PAINT_LOGW("Attempted to load from uninitialized slot %d", slot);
        return;
    }

    // Load the palette map
    snprintf(key, 16, "paint_%02d_pal", slot);

    if (!readNvsBlob(key, NULL, &paletteSize))
    {
        PAINT_LOGE("Couldn't read size of palette in slot %s", key);
        return;
    }

    if (readNvsBlob(key, paintState->recentColors, &paletteSize))
    {
        paintState->fgColor = paintState->recentColors[0];
        paintState->bgColor = paintState->recentColors[1];
        PAINT_LOGD("Read %zu bytes of palette from slot %s", paletteSize, key);
    }
    else
    {
        PAINT_LOGE("Could't read palette from slot %s", key);
        return;
    }

    // Read the canvas dimensions
    PAINT_LOGD("Reading dimensions");
    int32_t packedSize;
    snprintf(key, 16, "paint_%02d_dim", slot);
    if (readNvs32(key, &packedSize))
    {
        paintState->canvasH = (uint32_t)packedSize & 0xFFFF;
        paintState->canvasW = (((uint32_t)packedSize) >> 16) & 0xFFFF;
        PAINT_LOGD("Read dimensions from slot %s: %d x %d", key, paintState->canvasW, paintState->canvasH);
    }
    else
    {
        PAINT_LOGE("Couldn't read dimensions from slot %s",key);
        return;
    }

    // Allocate space for the chunk
    uint32_t totalPx = paintState->canvasH * paintState->canvasW;
    uint8_t chunkCount = (totalPx + (PAINT_SAVE_CHUNK_SIZE * 2) - 1) / (PAINT_SAVE_CHUNK_SIZE * 2);
    if ((imgChunk = malloc(PAINT_SAVE_CHUNK_SIZE)) != NULL)
    {
        PAINT_LOGD("Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        PAINT_LOGE("malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    disableCursor();
    paintClearCanvas();

    size_t lastChunkSize;
    uint16_t x0, y0, x1, y1;
    // Read all the chunks
    for (uint32_t i = 0; i < chunkCount; i++)
    {
        snprintf(key, 16, "paint_%02dc%05d", slot, i);

        // get the chunk size
        if (!readNvsBlob(key, NULL, &lastChunkSize))
        {
            PAINT_LOGE("Unable to read size of blob %d in slot %s", i, key);
            continue;
        }

        // read the chunk
        if (readNvsBlob(key, imgChunk, &lastChunkSize))
        {
            PAINT_LOGD("Read blob %d of %d (%zu bytes)", i+1, chunkCount, lastChunkSize);
        }
        else
        {
            PAINT_LOGE("Unable to read blob %d of %d", i+1, chunkCount);
            // don't panic if we miss one chunk, maybe it's ok...
            continue;
        }


        // build the chunk
        for (uint16_t n = 0; n < lastChunkSize; n++)
        {
            // no need for logic to exit the final chunk early, since each chunk's size is given to us

            // calculate the real coordinates given the pixel indices
            x0 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) % paintState->canvasW * PAINT_CANVAS_SCALE;
            y0 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) / paintState->canvasW * PAINT_CANVAS_SCALE;
            x1 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) % paintState->canvasW * PAINT_CANVAS_SCALE;
            y1 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) / paintState->canvasW * PAINT_CANVAS_SCALE;

            // Draw the pixel SCALE**2 times
            // TODO get rid of all the math and use the scaled functions
            plotRectFilled(paintState->disp, x0, y0, x0 + PAINT_CANVAS_SCALE + 1, y0 + PAINT_CANVAS_SCALE + 1, paintState->recentColors[imgChunk[n] >> 4]);
            plotRectFilled(paintState->disp, x1, y1, x1 + PAINT_CANVAS_SCALE + 1, y1 + PAINT_CANVAS_SCALE + 1, paintState->recentColors[imgChunk[n] & 0xF]);
        }
    }

    // This should't be necessary in the final version but whenever I change the canvas dimensions it breaks stuff
    if (paintState->canvasH > PAINT_CANVAS_HEIGHT || paintState->canvasW > PAINT_CANVAS_WIDTH)
    {
        paintState->canvasH = PAINT_CANVAS_HEIGHT;
        paintState->canvasW = PAINT_CANVAS_WIDTH;

        PAINT_LOGW("Loaded image had invalid bounds. Resetting to %d x %d", paintState->canvasW, paintState->canvasH);
    }

    paintSetRecentSlot(slot);

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();
}