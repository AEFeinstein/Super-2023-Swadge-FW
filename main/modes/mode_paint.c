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


/*
 * REMAINING BIG THINGS TO DO:
 *
 * - Draw icons for the tools?
 * - Explicitly mark everything that doesn't work in the canvas coordinate space for whatever reason
 * - Make the pixel push/pop work for Big PixelsTM
 * - Sharing! How will that work...
 * - Airbrush tool
 * - Squarewave tool, obviously
 * - Stamp tool?
 * - Copy/paste???
 * - Easter egg?
 *
 *
 * MORE MINOR POLISH THINGS:
 *
 * - Fix swapping fg/bg color sometimes causing current color not to be the first in the color picker list
 *   (sometimes this makes it so if you change a tool, it also changes your color)
 * - Always draw the cursor in a contrasting color
 * - Use different pick markers / cursors for each brush (e.g. crosshair for circle pen, two L-shaped crosshairs for box pen...)
 */

static const char paintTitle[] = "MFPaint";
static const char menuOptDraw[] = "Draw";
static const char menuOptGallery[] = "Gallery";
static const char menuOptReceive[] = "Receive";
static const char menuOptExit[] = "Exit";

#define PIXEL_STACK_MIN_SIZE 2
#define MAX_PICK_POINTS 16
// hold button for .3s to repeat
#define BUTTON_REPEAT_TIME 300000

#define CURSOR_POINTS 8
static const int8_t cursorShapeX[CURSOR_POINTS] = {-2, -1,  0,  0,  1, 2, 0, 0};
static const int8_t cursorShapeY[CURSOR_POINTS] = { 0,  0, -2, -1,  0, 0, 1, 2};

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
    font_t menuFont;
    font_t toolbarFont;
    meleeMenu_t* menu;
    display_t* disp;
    paintScreen_t screen;

    paletteColor_t fgColor, bgColor;
    paletteColor_t recentColors[PAINT_MAX_COLORS];
    paletteColor_t underCursor[CURSOR_POINTS]; // top left right bottom

    // Index of the currently selected brush
    uint8_t brushIndex;
    // A pointer to the currently selected brush's definition
    brush_t *brush;

    // The current brush width
    uint8_t brushWidth;

    // An array of points that have been selected for the current brush
    point_t pickPoints[MAX_PICK_POINTS];

    // The number of points already selected
    size_t pickCount;

    pxStack_t pxStack;
    pxStack_t cursorPxs;

    bool aHeld;
    bool selectHeld;
    bool clearScreen;
    bool redrawToolbar;
    bool showCursor;
    bool doSave, saveInProgress;
    bool wasSaved;

    bool firstMove;
    int64_t btnHoldTime;

    int subPixelOffsetX, subPixelOffsetY;

    uint8_t paletteSelect;

    int16_t cursorX, cursorY;
    int16_t lastCursorX, lastCursorY;
    int16_t canvasW, canvasH;
    int8_t moveX, moveY;
} paintMenu_t;


// Mode struct function declarations
void paintEnterMode(display_t* disp);
void paintExitMode();
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
void paintDrawPaintBucket(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);
void paintDrawClear(display_t*, point_t*, uint8_t, uint16_t, paletteColor_t);

static const brush_t brushes[] =
{
    { .name = "Square Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawSquarePen },
    { .name = "Circle Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawCirclePen },
    { .name = "Line",       .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 8, .fnDraw = paintDrawLine },
    { .name = "Bezier Curve",      .mode = PICK_POINT, .maxPoints = 4, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCurve },
    { .name = "Rectangle",  .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 8, .fnDraw = paintDrawRectangle },
    { .name = "Filled Rectangle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledRectangle },
    { .name = "Circle",     .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCircle },
    { .name = "Filled Circle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledCircle },
    { .name = "Ellipse",    .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawEllipse },
    { .name = "Polygon",    .mode = PICK_POINT_LOOP, .maxPoints = MAX_PICK_POINTS, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawPolygon },
    { .name = "Paint Bucket", .mode = PICK_POINT, .maxPoints = 1, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawPaintBucket },
    { .name = "Clear",      .mode = INSTANT, .maxPoints = 0, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawClear },
};

paintMenu_t* paintState;

// Util function declarations

int xTranslate(int x);
int yTranslate(int y);
void paintInitialize();
void paintRenderAll();
void paintRenderCursor();
void paintRenderToolbar();
void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col);
void paintMoveCursorRel(int8_t xDiff, int8_t yDiff);
void paintClearCanvas();
void paintUpdateRecents(uint8_t selectedIndex);
void paintPrevTool();
void paintNextTool();
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


// Mode struct function implemetations

int xTranslate(int x)
{
    return (x - PAINT_CANVAS_X_OFFSET) * PAINT_CANVAS_SCALE + PAINT_CANVAS_X_OFFSET + paintState->subPixelOffsetX;
}
int yTranslate(int y)
{
    return (y - PAINT_CANVAS_Y_OFFSET) * PAINT_CANVAS_SCALE + PAINT_CANVAS_Y_OFFSET + paintState->subPixelOffsetY;
}

void paintEnterMode(display_t* disp)
{
    paintState = calloc(1, sizeof(paintMenu_t));

    paintState->disp = disp;
    loadFont("mm.font", &(paintState->menuFont));
    loadFont("radiostars.font", &(paintState->toolbarFont));

    paintState->menu = initMeleeMenu(paintTitle, &(paintState->menuFont), paintMainMenuCb);

    paintInitialize();
}

void paintExitMode()
{
    ESP_LOGD("Paint", "Exiting");
    paintSave(0);
    deinitMeleeMenu(paintState->menu);
    freeFont(&(paintState->menuFont));

    freePxStack(&paintState->pxStack);
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
        if (paintState->clearScreen) {
            paintClearCanvas();
            paintRenderAll();
            paintState->clearScreen = false;
            paintState->showCursor = true;
        }

        if (paintState->doSave && !paintState->saveInProgress)
        {
            paintState->saveInProgress = true;
            paintState->doSave = false;
            if (paintState->wasSaved)
            {
                paintLoad(0);
                paintState->redrawToolbar = true;
            }
            else
            {
                paintSave(0);
            }
            paintState->saveInProgress = false;
        }


        if (paintState->redrawToolbar)
        {
            paintRenderToolbar();
            paintState->redrawToolbar = false;
        }
        else
        {
            if (paintState->aHeld)
            {
                paintDoTool(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY, paintState->fgColor);

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
            break;
        }
    }
    default:
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
            if (evt->button & SELECT)
            {
                paintState->selectHeld = true;
                paintState->redrawToolbar = true;
                paintState->aHeld = false;
                paintState->moveX = 0;
                paintState->moveY = 0;
            }

            if (!paintState->selectHeld)
            {
                if (evt->button & BTN_A)
                {
                    paintState->aHeld = true;
                }

                if (evt->button & UP)
                {
                    paintState->firstMove = true;
                    paintState->moveY = -1;
                }
                else if (evt->button & DOWN)
                {
                    paintState->firstMove = true;
                    paintState->moveY = 1;
                }

                if (evt->button & LEFT)
                {
                    paintState->firstMove = true;
                    paintState->moveX = -1;
                }
                else if (evt->button & RIGHT)
                {
                    paintState->firstMove = true;
                    paintState->moveX = 1;
                }
            }
        }
        else
        {
            if (evt->button & SELECT)
            {
                paintUpdateRecents(paintState->paletteSelect);
                paintState->paletteSelect = 0;
                paintState->selectHeld = false;
                paintState->redrawToolbar = true;
            }

            if (paintState->screen == PAINT_DRAW && evt->button & START && !paintState->saveInProgress && !paintState->selectHeld)
            {
                paintState->doSave = true;
            }


            if (paintState->selectHeld)
            {
                if (evt->button & UP)
                {
                    paintState->redrawToolbar = true;
                    if (paintState->paletteSelect == 0)
                    {
                        paintState->paletteSelect = PAINT_MAX_COLORS - 1;
                    } else {
                        paintState->paletteSelect -= 1;
                    }
                }
                else if (evt->button & DOWN)
                {
                    paintState->redrawToolbar = true;
                    paintState->paletteSelect = (paintState->paletteSelect + 1) % PAINT_MAX_COLORS;
                }
                else if (evt->button & LEFT)
                {
                    // previous tool
                    paintPrevTool();
                    paintState->redrawToolbar = true;
                }
                else if (evt->button & RIGHT)
                {
                    // next tool
                    paintNextTool();
                    paintState->redrawToolbar = true;
                }
                else if (evt->button & BTN_A)
                {
                    paintState->brushWidth++;
                    paintState->redrawToolbar = true;
                }
                else if (evt->button & BTN_B)
                {
                    if (paintState->brushWidth > 1)
                    {
                        paintState->brushWidth--;
                        paintState->redrawToolbar = true;
                    }
                }
            }
            else
            {
                if (evt->button & BTN_A)
                {
                    paintState->aHeld = false;
                }

                if (evt->button & BTN_B)
                {
                    // no temporary variables for me thanks
                    paintState->fgColor ^= paintState->bgColor;
                    paintState->bgColor ^= paintState->fgColor;
                    paintState->fgColor ^= paintState->bgColor;

                    paintState->recentColors[0] ^= paintState->recentColors[1];
                    paintState->recentColors[1] ^= paintState->recentColors[0];
                    paintState->recentColors[0] ^= paintState->recentColors[1];
                    paintState->redrawToolbar = true;
                }

                if (evt->button & (UP | DOWN))
                {
                    paintState->moveY = 0;

                    // Reset the button hold time, but only if we're not holding another direction
                    // This lets you make turns quickly instead of waiting for the repeat timeout in the middle
                    if (!paintState->moveX)
                    {
                        paintState->btnHoldTime = 0;
                    }
                }

                if (evt->button & (LEFT | RIGHT))
                {
                    paintState->moveX = 0;

                    if (!paintState->moveY)
                    {
                        paintState->btnHoldTime = 0;
                    }
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

void paintInitialize()
{
    resetMeleeMenu(paintState->menu, paintTitle, paintMainMenuCb);
    addRowToMeleeMenu(paintState->menu, menuOptDraw);
    addRowToMeleeMenu(paintState->menu, menuOptGallery);
    addRowToMeleeMenu(paintState->menu, menuOptReceive);
    addRowToMeleeMenu(paintState->menu, menuOptExit);

    paintState->screen = PAINT_MENU;

    paintState->clearScreen = true;

    paintState->wasSaved = true;
    paintState->doSave = true;

    paintState->canvasW = PAINT_CANVAS_WIDTH;
    paintState->canvasH = PAINT_CANVAS_HEIGHT;

    paintState->brush = &(brushes[0]);
    paintState->brushWidth = 1;

    initPxStack(&paintState->pxStack);

    paintState->disp->clearPx();

    paintState->fgColor = c000; // black
    paintState->bgColor = c555; // white

    // this is kind of a hack but oh well
    for (uint8_t i = 0; i < CURSOR_POINTS; i++)
    {
        if (cursorShapeX[i] < 0 || cursorShapeY[i] < 0)
        {
            paintState->underCursor[i] = PAINT_TOOLBAR_BG;
        }
        else
        {
            paintState->underCursor[i] = paintState->bgColor;
        }
    }

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

    ESP_LOGD("Paint", "It's paintin' time! Canvas is %d x %d pixels!", paintState->disp->w, paintState->disp->h);

}

void paintMainMenuCb(const char* opt)
{
    if (opt == menuOptDraw)
    {
        ESP_LOGD("Paint", "Selected Draw");
        paintState->screen = PAINT_DRAW;
    }
    else if (opt == menuOptGallery)
    {
        ESP_LOGD("Paint", "Selected Gallery");
        paintState->screen = PAINT_GALLERY;
    }
    else if (opt == menuOptReceive)
    {
        ESP_LOGD("Paint", "Selected Receive");
        paintState->screen = PAINT_RECEIVE;
    }
    else if (opt == menuOptExit)
    {
        ESP_LOGD("Paint", "Selected Exit");
        switchToSwadgeMode(&modeMainMenu);
    }
}

void paintClearCanvas()
{
    fillDisplayArea(paintState->disp, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_X_OFFSET + paintState->canvasW * PAINT_CANVAS_SCALE, PAINT_CANVAS_Y_OFFSET + paintState->canvasH * PAINT_CANVAS_SCALE, paintState->bgColor);
}

void paintRenderAll()
{
    paintRenderToolbar();
    paintRenderCursor();
}

void saveCursorPixels(bool useLast)
{
    for (uint8_t i = 0; i < CURSOR_POINTS; i++)
    {
        paintState->underCursor[i] = paintState->disp->getPx(CNV2SCR_X((useLast ? paintState->lastCursorX : paintState->cursorX) + cursorShapeX[i]), CNV2SCR_Y((useLast ? paintState->lastCursorY : paintState->cursorY) + cursorShapeY[i]));
    }
}

void restoreCursorPixels(bool useLast)
{
    for (uint8_t i = 0; i < CURSOR_POINTS; i++)
    {
        paintState->disp->setPx(CNV2SCR_X((useLast ? paintState->lastCursorX : paintState->cursorX) + cursorShapeX[i]), CNV2SCR_Y((useLast ? paintState->lastCursorY : paintState->cursorY) + cursorShapeY[i]), paintState->underCursor[i]);
    }
}

void plotCursor()
{
    for (int i = 0; i < CURSOR_POINTS; i++)
    {
        paintState->disp->setPx(CNV2SCR_X(paintState->cursorX + cursorShapeX[i]), CNV2SCR_Y(paintState->cursorY + cursorShapeY[i]), c300);
    }
}

void paintRenderCursor()
{
    if (paintState->showCursor)
    {
        if (paintState->lastCursorX != paintState->cursorX || paintState->lastCursorY != paintState->cursorY)
        {
            // Restore the pixels under the last cursor position
            restoreCursorPixels(true);

            // Save the pixels that will be under the cursor
            saveCursorPixels(false);

            // Draw the cursor
            plotCursor();
        }
    }
}

void enableCursor()
{
    if (!paintState->showCursor)
    {
        // Save the pixels under the cursor we're about to draw over
        saveCursorPixels(false);

        paintState->showCursor = true;

        plotCursor();
    }
}

void disableCursor()
{
    if (paintState->showCursor)
    {
        restoreCursorPixels(false);

        paintState->showCursor = false;
    }
}

void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col)
{
    fillDisplayArea(disp, x0, y0, x1 - 1, y1 - 1, col);
}

void plotRectFilledTranslate(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, translateFn_t xTr, translateFn_t yTr)
{
    fillDisplayArea(disp, xTr(x0), yTr(y0), xTr(x1 - 1), yTr(y1 - 1), col);
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

void paintRenderToolbar()
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
            drawColorBox(PAINT_COLORBOX_X + i * (PAINT_COLORBOX_MARGIN_LEFT + PAINT_COLORBOX_W), PAINT_COLORBOX_Y, PAINT_COLORBOX_W, PAINT_COLORBOX_H, paintState->recentColors[i], paintState->selectHeld && paintState->paletteSelect == i, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
        }
        else
        {
            drawColorBox(PAINT_COLORBOX_X, PAINT_COLORBOX_Y + i * (PAINT_COLORBOX_MARGIN_TOP + PAINT_COLORBOX_H), PAINT_COLORBOX_W, PAINT_COLORBOX_H, paintState->recentColors[i], paintState->selectHeld && paintState->paletteSelect == i, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
        }
    }

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
        snprintf(text, sizeof(text), "%ld/%d", paintState->pickCount, paintState->brush->maxPoints);
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
            snprintf(text, sizeof(text), "%ld", maxPicks - paintState->pickCount - 1);
        }

        drawText(paintState->disp, &paintState->toolbarFont, c000, text, 220, 4);
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

// void (*fnDraw)(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col);
void paintDrawSquarePen(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    if (paintState->subPixelOffsetX == 0 && paintState->subPixelOffsetY == 0)
    {
        plotRectFilledTranslate(disp, points[0].x, points[0].y, points[0].x + (size - 1) + PAINT_CANVAS_SCALE - 1, points[0].y + (size - 1) + PAINT_CANVAS_SCALE - 1, col, xTranslate, yTranslate);
    }
}

void paintDrawCirclePen(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotCircleFilledTranslate(disp, points[0].x, points[0].y, size, col, xTranslate, yTranslate);
}

void paintDrawLine(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotLineTranslate(paintState->disp, points[0].x, points[0].y, points[1].x, points[1].y, col, 0, xTranslate, yTranslate);
}

void paintDrawCurve(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    plotCubicBezier(disp, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y, col);
}

void paintDrawRectangle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t x0 = (points[0].x > points[1].x) ? points[1].x : points[0].x;
    uint16_t y0 = (points[0].y > points[1].y) ? points[1].y : points[0].y;
    uint16_t x1 = (points[0].x > points[1].x) ? points[0].x : points[1].x;
    uint16_t y1 = (points[0].y > points[1].y) ? points[0].y : points[1].y;

    plotRectTranslate(disp, x0, y0, x1 + 1, y1 + 1, col, xTranslate, yTranslate);
}

void paintDrawFilledRectangle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t x0 = (points[0].x > points[1].x) ? points[1].x : points[0].x;
    uint16_t y0 = (points[0].y > points[1].y) ? points[1].y : points[0].y;
    uint16_t x1 = (points[0].x > points[1].x) ? points[0].x : points[1].x;
    uint16_t y1 = (points[0].y > points[1].y) ? points[0].y : points[1].y;

    // This function takes care of its own scaling because it's very easy and will save a lot of unnecessary draws
    if (paintState->subPixelOffsetX == 0 && paintState->subPixelOffsetY == 0)
    {
        plotRectFilledTranslate(disp, x0, y0, x1 + PAINT_CANVAS_SCALE - 1, y1 + PAINT_CANVAS_SCALE - 1, col, xTranslate, yTranslate);
    }
}

void paintDrawCircle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t dX = abs(points[0].x - points[1].x);
    uint16_t dY = abs(points[0].y - points[1].y);
    uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);

    plotCircleTranslate(disp, points[0].x, points[0].y, r, col, xTranslate, yTranslate);
}

void paintDrawFilledCircle(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    uint16_t dX = abs(points[0].x - points[1].x);
    uint16_t dY = abs(points[0].y - points[1].y);
    uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);

    plotCircleFilledTranslate(disp, points[0].x, points[1].x, r, col, xTranslate, yTranslate);
}

void paintDrawEllipse(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    // for some reason, plotting an ellipse also plots 2 extra points outside of the ellipse
    // let's just work around that
    pushPx(&paintState->pxStack, disp, (points[0].x < points[1].x ? points[0].x : points[1].x) + (abs(points[1].x - points[0].x) + 1) / 2, points[0].y < points[1].y ? points[0].y - 2 : points[1].y - 2);
    pushPx(&paintState->pxStack, disp, (points[0].x < points[1].x ? points[0].x : points[1].x) + (abs(points[1].x - points[0].x) + 1) / 2, points[0].y < points[1].y ? points[1].y + 2 : points[0].y + 2);

    plotEllipseRectTranslate(disp, points[0].x, points[0].y, points[1].x, points[1].y, col, xTranslate, yTranslate);

    popPx(&paintState->pxStack, disp);
    popPx(&paintState->pxStack, disp);
}

void paintDrawPolygon(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    for (uint8_t i = 0; i < numPoints - 1; i++)
    {
        plotLineTranslate(disp, points[i].x, points[i].y, points[i+1].x, points[i+1].y, col, 0, xTranslate, yTranslate);
    }
}

void paintDrawPaintBucket(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    if (paintState->subPixelOffsetX == 0 && paintState->subPixelOffsetY == 0)
    {
        floodFill(disp, xTranslate(points[0].x), yTranslate(points[0].y), col);
    }
}

void paintDrawClear(display_t* disp, point_t* points, uint8_t numPoints, uint16_t size, paletteColor_t col)
{
    // No need to translate here, so only draw once
    if (paintState->subPixelOffsetX == 0 && paintState->subPixelOffsetY == 0)
    {
        fillDisplayArea(disp, CNV2SCR_X(0), CNV2SCR_Y(0), CNV2SCR_X(PAINT_CANVAS_WIDTH), CNV2SCR_Y(PAINT_CANVAS_HEIGHT), col);
    }
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

        ESP_LOGD("Paint", "pick[%02ld] = SCR(%03d, %03d) / CNV(%d, %d)", paintState->pickCount, x, y, (x - PAINT_CANVAS_X_OFFSET), (y - PAINT_CANVAS_Y_OFFSET));

        // Save the pixel underneath the selection, then draw a temporary pixel to mark it
        // But don't bother if this is the last pick point, since it will never actually be seen
        if (!lastPick)
        {
            for (int xo = 0; xo < PAINT_CANVAS_SCALE; xo++)
            {
                for (int yo = 0; yo < PAINT_CANVAS_SCALE; yo++)
                {
                    pushPx(&paintState->pxStack, paintState->disp, (x - PAINT_CANVAS_X_OFFSET) * PAINT_CANVAS_SCALE + PAINT_CANVAS_X_OFFSET + xo, (y - PAINT_CANVAS_Y_OFFSET) * PAINT_CANVAS_SCALE + PAINT_CANVAS_Y_OFFSET + yo);
                    paintState->disp->setPx((x - PAINT_CANVAS_X_OFFSET) * PAINT_CANVAS_SCALE + PAINT_CANVAS_X_OFFSET + xo, (y - PAINT_CANVAS_Y_OFFSET) * PAINT_CANVAS_SCALE + PAINT_CANVAS_Y_OFFSET + yo, paintState->fgColor);
                }
            }
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

                //instead maybe memcpy(&(paintState->pickPoints[0]), &(paintState->pickPoints[paintState->pickCount]), sizeof(point_t));
                paintState->pickPoints[paintState->pickCount].x = paintState->pickPoints[0].x;
                paintState->pickPoints[paintState->pickCount].y = paintState->pickPoints[0].y;

                // This is going to get cleared immediately, don't do that
                //pushPx(&paintState->pxStack, paintState->disp, paintState->pickPoints[0].x, paintState->pickPoints[0].y);

                ESP_LOGD("Paint", "pick[%02ld] = (%03d, %03d) (last!)", paintState->pickCount, paintState->pickPoints[0].x, paintState->pickPoints[0].y);

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

        for (paintState->subPixelOffsetX = 0; paintState->subPixelOffsetX < PAINT_CANVAS_SCALE; paintState->subPixelOffsetX++)
        {
            for (paintState->subPixelOffsetY = 0; paintState->subPixelOffsetY < PAINT_CANVAS_SCALE; paintState->subPixelOffsetY++)
            {
                paintState->brush->fnDraw(paintState->disp, paintState->pickPoints, paintState->pickCount, paintState->brushWidth, col);
            }
        }

        paintState->pickCount = 0;
    }

    enableCursor();
    paintRenderToolbar();
}

void initPxStack(pxStack_t* pxStack)
{
    pxStack->size = PIXEL_STACK_MIN_SIZE;
    ESP_LOGD("Paint", "Allocating pixel stack with size %ld", pxStack->size);
    pxStack->data = malloc(sizeof(pxVal_t) * pxStack->size);
    pxStack->index = -1;
}

void freePxStack(pxStack_t* pxStack)
{
    if (pxStack->data != NULL)
    {
        free(pxStack->data);
        ESP_LOGD("Paint", "Freed pixel stack");
        pxStack->size = 0;
        pxStack->index = -1;
    }
}

void maybeGrowPxStack(pxStack_t* pxStack)
{
    if (pxStack->index >= pxStack->size)
    {
        pxStack->size *= 2;
        ESP_LOGD("Paint", "Expanding pixel stack to size %ld", pxStack->size);
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
        ESP_LOGD("Paint", "Shrinking pixel stack to %ld", pxStack->size);
        pxStack->data = realloc(pxStack->data, sizeof(pxVal_t) * pxStack->size);
        ESP_LOGD("Paint", "Done shrinking pixel stack");
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

    ESP_LOGD("Paint", "Saved pixel %d at (%d, %d)", pxStack->index, x, y);
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
        ESP_LOGD("Paint", "Popping pixel %d...", pxStack->index);

        // Draw the pixel from the top of the stack
        disp->setPx(pxStack->data[pxStack->index].x, pxStack->data[pxStack->index].y, pxStack->data[pxStack->index].col);
        pxStack->index--;

        // Is this really necessary? The stack empties often so maybe it's better not to reallocate constantly
        //maybeShrinkPxStack(pxStack);

        return true;
    }

    return false;
}

void paintPrevTool()
{
    if (paintState->brushIndex > 0)
    {
        paintState->brushIndex--;
    }
    else
    {
        paintState->brushIndex = sizeof(brushes) / sizeof(brush_t) - 1;
    }
    paintState->brush = &(brushes[paintState->brushIndex]);

    // Reset the pick state
    paintState->pickCount = 0;

    // Remove any stored temporary pixels
    while (popPx(&paintState->pxStack, paintState->disp));
}

void paintNextTool()
{
    if (paintState->brushIndex < sizeof(brushes) / sizeof(brush_t) - 1)
    {
        paintState->brushIndex++;
    }
    else
    {
        paintState->brushIndex = 0;
    }
    paintState->brush = &(brushes[paintState->brushIndex]);

    // Reset the pick state
    paintState->pickCount = 0;

    // Remove any stored temporary pixels
    while (popPx(&paintState->pxStack, paintState->disp));
}

void paintUpdateRecents(uint8_t selectedIndex)
{
    paintState->fgColor = paintState->recentColors[selectedIndex];

    for (uint8_t i = selectedIndex; i > 0; i--)
    {
        paintState->recentColors[i] = paintState->recentColors[i-1];
    }
    paintState->recentColors[0] = paintState->fgColor;
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
    ESP_LOGD("Paint", "Palette will take up %ld bytes", sizeof(paletteColor_t) * PAINT_MAX_COLORS);
    if (writeNvsBlob(key, paintState->recentColors, sizeof(paletteColor_t) * PAINT_MAX_COLORS))
    {
        ESP_LOGD("Paint", "Saved palette to slot %s", key);
    }
    else
    {
        ESP_LOGE("Paint", "Could't save palette to slot %s", key);
        return;
    }

    // Save the canvas dimensions
    uint32_t packedSize = paintState->canvasW << 16 | paintState->canvasH;
    snprintf(key, 16, "paint_%02d_dim", slot);
    if (writeNvs32(key, packedSize))
    {
        ESP_LOGD("Paint", "Saved dimensions to slot %s", key);
    }
    else
    {
        ESP_LOGE("Paint", "Couldn't save dimensions to slot %s",key);
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
        ESP_LOGD("Paint", "Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        ESP_LOGE("Paint", "malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    ESP_LOGD("Paint", "We will use %d chunks of size %dB (%d), plus one of %dB == %dB to save the image", chunkCount - 1, PAINT_SAVE_CHUNK_SIZE, (chunkCount - 1) * PAINT_SAVE_CHUNK_SIZE, finalChunkSize, (chunkCount - 1) * PAINT_SAVE_CHUNK_SIZE + finalChunkSize);
    ESP_LOGD("Paint", "The image is %d x %d px == %dpx, at 2px/B that's %dB", paintState->canvasW, paintState->canvasH, totalPx, totalPx / 2);

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
            ESP_LOGD("Paint", "Saved blob %d of %d", i+1, chunkCount);
        }
        else
        {
            ESP_LOGE("Paint", "Unable to save blob %d of %d", i+1, chunkCount);
            free(imgChunk);

            enableCursor();
            return;
        }
    }

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();

    paintState->wasSaved = true;
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

    // Save the palette map, this lets us compact the image by 50%
    snprintf(key, 16, "paint_%02d_pal", slot);
    ESP_LOGD("Paint", "Reading palette from %s...", key);
    if (readNvsBlob(key, paintState->recentColors, &paletteSize))
    {
        ESP_LOGD("Paint", "Read %ld bytes of palette from slot %s", paletteSize, key);
    }
    else
    {
        ESP_LOGE("Paint", "Could't read palette from slot %s", key);
        return;
    }

    // Read the canvas dimensions
    ESP_LOGD("Paint", "Reading dimensions");
    int32_t packedSize;
    snprintf(key, 16, "paint_%02d_dim", slot);
    if (readNvs32(key, &packedSize))
    {
        paintState->canvasH = (uint32_t)packedSize & 0xFFFF;
        paintState->canvasW = (((uint32_t)packedSize) >> 16) & 0xFFFF;
        ESP_LOGD("Paint", "Read dimensions from slot %s: %d x %d", key, paintState->canvasW, paintState->canvasH);
    }
    else
    {
        ESP_LOGE("Paint", "Couldn't read dimensions from slot %s",key);
        return;
    }

    // Allocate space for the chunk
    uint32_t totalPx = paintState->canvasH * paintState->canvasW;
    uint8_t chunkCount = (totalPx + (PAINT_SAVE_CHUNK_SIZE * 2) - 1) / (PAINT_SAVE_CHUNK_SIZE * 2);
    if ((imgChunk = malloc(PAINT_SAVE_CHUNK_SIZE)) != NULL)
    {
        ESP_LOGD("Paint", "Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        ESP_LOGE("Paint", "malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    disableCursor();
    paintClearCanvas();

    size_t lastChunkSize;
    uint16_t x0, y0, x1, y1;
    // Read all the chunks
    for (uint32_t i = 0; i < chunkCount; i++)
    {
        // read the chunk
        snprintf(key, 16, "paint_%02dc%05d", slot, i);
        if (readNvsBlob(key, imgChunk, &lastChunkSize))
        {
            ESP_LOGD("Paint", "Read blob %d of %d (%ld bytes)", i+1, chunkCount, lastChunkSize);
        }
        else
        {
            ESP_LOGE("Paint", "Unable to read blob %d of %d", i+1, chunkCount);
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
            for (int xo = 0; xo < PAINT_CANVAS_SCALE; xo++)
            {
                for (int yo = 0; yo < PAINT_CANVAS_SCALE; yo++)
                {
                    paintState->disp->setPx(x0 + xo, y0 + yo, paintState->recentColors[imgChunk[n] >> 4]);
                    paintState->disp->setPx(x1 + xo, y1 + yo, paintState->recentColors[imgChunk[n] & 0xF]);
                }
            }
        }
    }

    // This should't be necessary in the final version but whenever I change the canvas dimensions it breaks stuff
    if (paintState->canvasH > PAINT_CANVAS_HEIGHT || paintState->canvasW > PAINT_CANVAS_WIDTH)
    {
        paintState->canvasH = PAINT_CANVAS_HEIGHT;
        paintState->canvasW = PAINT_CANVAS_WIDTH;

        ESP_LOGW("Paint", "Loaded image had invalid bounds. Resetting to %d x %d", paintState->canvasW, paintState->canvasH);
    }

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();

    paintState->wasSaved = false;
}