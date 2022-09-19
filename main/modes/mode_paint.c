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

static const char paintTitle[] = "MAGPaint";
static const char menuOptDraw[] = "Draw";
static const char menuOptGallery[] = "Gallery";
static const char menuOptReceive[] = "Receive";
static const char menuOptExit[] = "Exit";

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
    NORTH,
    SOUTH,
    WEST,
    EAST,
    NONE,
} direction_t;

typedef enum
{
    // Draws a square
    TOOL_PEN_SQUARE,
    TOOL_PEN_CIRCLE,
    TOOL_FLOOD_FILL,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_CURVE,
    TOOL_BOX_EMPTY,
    TOOL_BOX_FILL,
    TOOL_CIRCLE_EMPTY,
    TOOL_CIRCLE_FILL,
    TOOL_ELLIPSE,
    TOOL_SELECT,
} brush_t;

typedef enum
{
    PICK_START,
    PICK_END,
} brushState_t;

typedef struct
{
    font_t menuFont;
    font_t toolbarFont;
    meleeMenu_t* menu;
    display_t* disp;
    paintScreen_t screen;

    paletteColor_t fgColor, bgColor;
    paletteColor_t recentColors[PAINT_MAX_COLORS];
    paletteColor_t underCursor[4]; // top left right bottom

    brush_t brush;
    uint8_t brushWidth;
    brushState_t brushState;
    uint16_t pickX, pickY;

    bool aHeld;
    bool selectHeld;
    bool clearScreen;
    bool redrawToolbar;
    bool showCursor;
    bool toolHoldable;
    bool doSave, saveInProgress;
    bool wasSaved;

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

paintMenu_t* paintState;

// Util function declarations

void paintInitialize();
void paintRenderAll();
void paintRenderCursor();
void paintRenderToolbar();
void paintMoveCursorRel(int8_t xDiff, int8_t yDiff);
void paintClearCanvas();
void paintUpdateRecents(uint8_t selectedIndex);
void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col);
void paintPrevTool();
void paintNextTool();
void paintSave(uint8_t slot);
void paintLoad(uint8_t slot);
void _MyFill(uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);
void MyFillCore(uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);


// Mode struct function implemetations

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
    free(paintState);
    //pm = NULL;
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
                // TODO use different tools
                paintDoTool(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY, paintState->fgColor);

                if (!paintState->toolHoldable)
                {
                    paintState->aHeld = false;
                }
            }

            if (paintState->moveX || paintState->moveY)
            {
                paintMoveCursorRel(paintState->moveX, paintState->moveY);
                paintRenderAll();
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
            }

            if (!paintState->selectHeld)
            {
                if (evt->button & BTN_A)
                {
                    paintState->aHeld = true;
                }

                if (evt->button & UP)
                {
                    paintState->moveY = -1;
                }
                else if (evt->button & DOWN)
                {
                    paintState->moveY = 1;
                }

                if (evt->button & LEFT)
                {
                    paintState->moveX = -1;
                }
                else if (evt->button & RIGHT)
                {
                    paintState->moveX = 1;
                }
            }
        }
        else
        {
            ESP_LOGD("Paint", "Release Buttons: %x", evt->button);

            if (evt->button & SELECT)
            {
                paintUpdateRecents(paintState->paletteSelect);
                paintState->paletteSelect = 0;
                paintState->selectHeld = false;
                paintState->redrawToolbar = true;
            }

            if (evt->button & START && !paintState->saveInProgress)
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
                }
                else if (evt->button & BTN_B)
                {
                    if (paintState->brushWidth > 1)
                    {
                        paintState->brushWidth--;
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
                }

                if (evt->button & (LEFT | RIGHT))
                {
                    paintState->moveX = 0;
                }
            }
        }
        break;
    }

    default:
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

    paintState->brush = TOOL_PEN_SQUARE;
    paintState->toolHoldable = true;
    paintState->brushWidth = 1;

    paintState->disp->clearPx();

    paintState->fgColor = c000; // black
    paintState->bgColor = c555; // white

    // this is kind of a hack but oh well
    paintState->underCursor[0] = PAINT_TOOLBAR_BG;
    paintState->underCursor[1] = PAINT_TOOLBAR_BG;
    paintState->underCursor[2] = paintState->bgColor;
    paintState->underCursor[3] = paintState->bgColor;

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
    fillDisplayArea(paintState->disp, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_X_OFFSET + paintState->canvasW, PAINT_CANVAS_Y_OFFSET + paintState->canvasH, paintState->bgColor);
}

void paintRenderAll()
{
    ESP_LOGD("Paint", "Re-rendering all");
    //paintState->disp->clearPx();

    paintRenderToolbar();
    paintRenderCursor();
}

void paintRenderCursor()
{
    ESP_LOGD("Paint", "Rendering cursor...");
    if (paintState->showCursor)
    {
        ESP_LOGD("Paint", "Actualy no");
        if (paintState->lastCursorX != paintState->cursorX || paintState->lastCursorY != paintState->cursorY)
        {
            // Restore the pixels under the last cursor position
            paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->lastCursorX, PAINT_CANVAS_Y_OFFSET + paintState->lastCursorY - 1, paintState->underCursor[0]);
            paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->lastCursorX - 1, PAINT_CANVAS_Y_OFFSET + paintState->lastCursorY, paintState->underCursor[1]);
            paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->lastCursorX + 1, PAINT_CANVAS_Y_OFFSET + paintState->lastCursorY, paintState->underCursor[2]);
            paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->lastCursorX, PAINT_CANVAS_Y_OFFSET + paintState->lastCursorY + 1, paintState->underCursor[3]);

            // Save the pixels that will be under the cursor
            paintState->underCursor[0] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY - 1);
            paintState->underCursor[1] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX - 1, PAINT_CANVAS_Y_OFFSET + paintState->cursorY);
            paintState->underCursor[2] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX + 1, PAINT_CANVAS_Y_OFFSET + paintState->cursorY);
            paintState->underCursor[3] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY + 1);

            // Draw the cursor
            plotCircle(paintState->disp, PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY, 1, c300);
        }
    }
}

void enableCursor()
{
    if (!paintState->showCursor)
    {
        // Save the pixels under the cursor we're about to draw over
        paintState->underCursor[0] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY - 1);
        paintState->underCursor[1] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX - 1, PAINT_CANVAS_Y_OFFSET + paintState->cursorY);
        paintState->underCursor[2] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX + 1, PAINT_CANVAS_Y_OFFSET + paintState->cursorY);
        paintState->underCursor[3] = paintState->disp->getPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY + 1);

        paintState->showCursor = true;

        plotCircle(paintState->disp, PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY, 1, c300);
    }
}

void disableCursor()
{
    if (paintState->showCursor)
    {
        paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY - 1, paintState->underCursor[0]);
        paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX - 1, PAINT_CANVAS_Y_OFFSET + paintState->cursorY, paintState->underCursor[1]);
        paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX + 1, PAINT_CANVAS_Y_OFFSET + paintState->cursorY, paintState->underCursor[2]);
        paintState->disp->setPx(PAINT_CANVAS_X_OFFSET + paintState->cursorX, PAINT_CANVAS_Y_OFFSET + paintState->cursorY + 1, paintState->underCursor[3]);

        paintState->showCursor = false;
    }
}

void plotRectFilled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col)
{
    fillDisplayArea(disp, x0, y0, x1, y1, col);
}

void paintRenderToolbar()
{
    ESP_LOGD("Paint", "Rendering UI");

    //////// Background

    // Fill top bar
    fillDisplayArea(paintState->disp, 0, 0, paintState->disp->w, PAINT_CANVAS_Y_OFFSET, PAINT_TOOLBAR_BG);

    // Fill left side bar
    fillDisplayArea(paintState->disp, 0, 0, PAINT_CANVAS_X_OFFSET, paintState->disp->h, PAINT_TOOLBAR_BG);

    // Fill bottom bar, if there's room
    if (PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT < paintState->disp->h)
    {
        fillDisplayArea(paintState->disp, 0, PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT, paintState->disp->w, paintState->disp->h, PAINT_TOOLBAR_BG);
    }

    // Fill right bar, if there's room
    if (PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_HEIGHT < paintState->disp->w)
    {
        fillDisplayArea(paintState->disp, PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_WIDTH, 0, paintState->disp->w, paintState->disp->h, PAINT_TOOLBAR_BG);
    }

    //////// Active Colors
    plotRectFilled(paintState->disp, 4, PAINT_CANVAS_Y_OFFSET + 4, 4 + 8, PAINT_CANVAS_Y_OFFSET + 4 + 8, paintState->bgColor);
    plotRectFilled(paintState->disp, 4 + 4, PAINT_CANVAS_Y_OFFSET + 4 + 4, 4 + 4 + 8, PAINT_CANVAS_Y_OFFSET + 4 + 4 + 8, paintState->fgColor);

    int16_t spaceBetween = 2;
    int16_t colorBoxSize = 8;
    int16_t colorBoxStartY = PAINT_CANVAS_Y_OFFSET + 16 + spaceBetween * 2;
    int16_t colorBoxStartX = PAINT_CANVAS_X_OFFSET / 2 - colorBoxSize / 2;
    //////// Recent Colors
    for (int i = 0; i < PAINT_MAX_COLORS; i++)
    {
        if (paintState->recentColors[i] == cTransparent)
        {
            plotRectFilled(paintState->disp, colorBoxStartX, colorBoxStartY + (spaceBetween + colorBoxSize) * i, colorBoxStartX + colorBoxSize / 2 - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i + colorBoxSize / 2, c111);
            plotRectFilled(paintState->disp, colorBoxStartX + (colorBoxSize / 2) - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i, colorBoxStartX + colorBoxSize - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i + colorBoxSize / 2, c444);
            plotRectFilled(paintState->disp, colorBoxStartX, (colorBoxSize / 2) + colorBoxStartY + (spaceBetween + colorBoxSize) * i, colorBoxStartX + colorBoxSize / 2 - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i + colorBoxSize, c444);
            plotRectFilled(paintState->disp, colorBoxStartX + (colorBoxSize / 2) - 1, (colorBoxSize / 2) + colorBoxStartY + (spaceBetween + colorBoxSize) * i, colorBoxStartX + colorBoxSize - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i + colorBoxSize, c111);
        }
        else
        {
            plotRectFilled(paintState->disp, colorBoxStartX, colorBoxStartY + (spaceBetween + colorBoxSize) * i, colorBoxStartX + colorBoxSize - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i + colorBoxSize, paintState->recentColors[i]);
        }
        if (paintState->selectHeld && paintState->paletteSelect == i)
        {
            plotRect(paintState->disp, colorBoxStartX - 1, colorBoxStartY + (spaceBetween + colorBoxSize) * i - 1, colorBoxStartX + colorBoxSize, colorBoxStartY + (spaceBetween + colorBoxSize) * i + colorBoxSize + 1, c500);
        }
    }

    uint16_t textX = 30, textY = 4;
    switch (paintState->brush)
    {
    case TOOL_PEN_SQUARE:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Square Pen", textX, textY);
        break;

    case TOOL_PEN_CIRCLE:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Circle Pen", textX, textY);
        break;

    case TOOL_BOX_EMPTY:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Rectangle", textX, textY);
        break;

    case TOOL_BOX_FILL:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Filled Rectangle", textX, textY);
        break;

    case TOOL_CIRCLE_EMPTY:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Circle", textX, textY);
        break;

    case TOOL_CIRCLE_FILL:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Filled Circle", textX, textY);
        break;

    case TOOL_ELLIPSE:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Ellipse", textX, textY);
        break;

    case TOOL_LINE:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Line", textX, textY);
        break;

    case TOOL_FLOOD_FILL:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "Paint Bucket", textX, textY);
        break;

    default:
        drawText(paintState->disp, &paintState->toolbarFont, c000, "???", textX, textY);
        break;
    }
}




// adapted from http://www.adammil.net/blog/v126_A_More_Efficient_Flood_Fill.html
void floodFill(uint16_t x, uint16_t y, paletteColor_t col)
{
    if (paintState->disp->getPx(x, y) == col)
    {
        // makes no sense to fill with the same color, so just don't
        return;
    }

    _MyFill(x, y, paintState->disp->getPx(x, y), col);
}

void _MyFill(uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill)
{
    // at this point, we know array[y,x] is clear, and we want to move as far as possible to the upper-left. moving
    // up is much more important than moving left, so we could try to make this smarter by sometimes moving to
    // the right if doing so would allow us to move further up, but it doesn't seem worth the complexity
    while(true)
    {
        uint16_t ox = x, oy = y;
        while(y != PAINT_CANVAS_Y_OFFSET && paintState->disp->getPx(x, y-1) == search) y--;
        while(x != PAINT_CANVAS_X_OFFSET && paintState->disp->getPx(x-1, y) == search) x--;
        if(x == ox && y == oy) break;
    }
    MyFillCore(x, y, search, fill);
}


void MyFillCore(uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill)
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
        if(lastRowLength != 0 && paintState->disp->getPx(x, y) != search) // if this is not the first row and the leftmost cell is filled...
        {
            do
            {
                if(--lastRowLength == 0) return; // shorten the row. if it's full, we're done
            } while(paintState->disp->getPx(++x, y) != search); // otherwise, update the starting point of the main scan to match
            sx = x;
        }
        // we also want to handle the opposite case, | **|, where we begin scanning a 2-wide rectangular block and
        // then find on the next row that it has     |***| gotten wider on the left. again, we could handle this
        // with recursion but we'd prefer to adjust x and lastRowLength instead
        else
        {
            for(; x != PAINT_CANVAS_X_OFFSET && paintState->disp->getPx(x-1, y) == search; rowLength++, lastRowLength++)
            {
                paintState->disp->setPx(--x, y, fill); // to avoid scanning the cells twice, we'll fill them and update rowLength here
                // if there's something above the new starting point, handle that recursively. this deals with cases
                // like |* **| when we begin filling from (2,0), move down to (2,1), and then move left to (0,1).
                // the  |****| main scan assumes the portion of the previous row from x to x+lastRowLength has already
                // been filled. adjusting x and lastRowLength breaks that assumption in this case, so we must fix it
                if(y != PAINT_CANVAS_Y_OFFSET && paintState->disp->getPx(x, y-1) == search) _MyFill(x, y-1, search, fill); // use _Fill since there may be more up and left
            }
        }

        // now at this point we can begin to scan the current row in the rectangular block. the span of the previous
        // row from x (inclusive) to x+lastRowLength (exclusive) has already been filled, so we don't need to
        // check it. so scan across to the right in the current row
        for(; sx < PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_WIDTH && paintState->disp->getPx(sx, y) == search; rowLength++, sx++) paintState->disp->setPx(sx, y, fill);
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
                if(paintState->disp->getPx(sx, y) == search) MyFillCore(sx, y, search, fill); // row. the cells up and left must be set so use FillCore
            }
        }
        // alternately, if this row is longer than the previous row, as in the case |*** *| then we must look above
        // the end of the row, i.e at (4,0)                                         |*****|
        else if(rowLength > lastRowLength && y != PAINT_CANVAS_Y_OFFSET) // if this row is longer and we're not already at the top...
        {
            for(int ux=x+lastRowLength; ++ux<sx; ) // sx is the end of the current row
            {
                if(paintState->disp->getPx(ux, y-1) == search) _MyFill(ux, y-1, search, fill); // since there may be clear cells up and left, use _Fill
            }
        }
        lastRowLength = rowLength; // record the new row length
    } while(lastRowLength != 0 && ++y < PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT); // if we get to a full row or to the bottom, we're done
}
/*
void floodFillSub(uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t replace, direction_t skipDirection)
{
    ESP_LOGD("Paint", "Filling (%d, %d)", x, y);

    paintState->disp->setPx(x, y, replace);

    if (skipDirection != WEST && x > PAINT_CANVAS_X_OFFSET && paintState->disp->getPx(x - 1, y) == search)
    {
        floodFillSub(x - 1, y, search, replace, EAST);
    }

    if (skipDirection != NORTH && y > PAINT_CANVAS_Y_OFFSET && paintState->disp->getPx(x, y - 1) == search)
    {
        floodFillSub(x, y - 1, search, replace, SOUTH);
    }

    if (skipDirection != EAST && x < (PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_WIDTH) && paintState->disp->getPx(x + 1, y) == search)
    {
        floodFillSub(x + 1, y, search, replace, WEST);
    }

    if (skipDirection != SOUTH && y < (PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_HEIGHT) && paintState->disp->getPx(x, y + 1) == search)
    {
        floodFillSub(x, y + 1, search, replace, NORTH);
    }
}

void floodFill(uint16_t x, uint16_t y, paletteColor_t col) {
    ESP_LOGD("Paint", "Filling (%d, %d)", x, y);
    floodFillSub(x, y, paintState->disp->getPx(x, y), col, NONE);
}*/

void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col)
{
    disableCursor();

    // TODO encapsulate picking logic better

    switch (paintState->brush)
    {
    case TOOL_PEN_SQUARE:
    {
        plotRectFilled(paintState->disp, x, y, x + paintState->brushWidth, y + paintState->brushWidth, col);
        break;
    }

    case TOOL_PEN_CIRCLE:
    {
        plotCircleFilled(paintState->disp, x, y, paintState->brushWidth, col);
        break;
    }
    case TOOL_BOX_EMPTY:
    case TOOL_BOX_FILL:
    {
        if (paintState->brushState == PICK_START)
        {
            paintState->pickX = x;
            paintState->pickY = y;
            paintState->brushState = PICK_END;
        } else {
            if (paintState->pickX > x)
            {
                paintState->pickX ^= x;
                x ^= paintState->pickX;
                paintState->pickX ^= x;
            }

            if (paintState->pickY > y)
            {
                paintState->pickY ^= y;
                y ^= paintState->pickY;
                paintState->pickY ^= y;
            }
            if (paintState->brush == TOOL_BOX_EMPTY)
            {
                plotRect(paintState->disp, paintState->pickX, paintState->pickY, x + 1, y + 1, col);
            }
            else
            {
                plotRectFilled(paintState->disp, paintState->pickX, paintState->pickY, x + 1, y + 1, col);
            }

            paintState->brushState = PICK_START;
        }
        break;
    }
    case TOOL_CIRCLE_EMPTY:
    case TOOL_CIRCLE_FILL:
    {
        if (paintState->brushState == PICK_START)
        {
            paintState->pickX = x;
            paintState->pickY = y;
            paintState->brushState = PICK_END;
        }
        else
        {
            uint16_t dX = abs(x - paintState->pickX);
            uint16_t dY = abs(y - paintState->pickY);
            uint16_t r = (uint16_t)(sqrt(dX*dX+dY*dY) + 0.5);
            if (paintState->brush == TOOL_CIRCLE_EMPTY)
            {
                plotCircle(paintState->disp, paintState->pickX, paintState->pickY, r, col);
            }
            else
            {
                plotCircleFilled(paintState->disp, paintState->pickX, paintState->pickY, r, col);
            }
            paintState->brushState = PICK_START;
        }
        break;
    }

    case TOOL_ELLIPSE:
    {
        if (paintState->brushState == PICK_START)
        {
            paintState->pickX = x;
            paintState->pickY = y;
            paintState->brushState = PICK_END;
        } else {
            plotEllipseRect(paintState->disp, paintState->pickX, paintState->pickY, x, y, col);

            paintState->brushState = PICK_START;
        }
        break;
    }

    case TOOL_FLOOD_FILL:
    {
        floodFill(x, y, col);
        break;
    }

    case TOOL_LINE:
    {
        if (paintState->brushState == PICK_START)
        {
            ESP_LOGD("Paint", "First pick: (%d, %d)", x, y);
            paintState->pickX = x;
            paintState->pickY = y;
            paintState->brushState = PICK_END;
        }
        else
        {
            ESP_LOGD("Paint", "Second pick: (%d, %d)", x, y);
            plotLine(paintState->disp, paintState->pickX, paintState->pickY, x, y, col, 0);
            paintState->brushState = PICK_START;
            paintState->pickX = 0;
            paintState->pickY = 0;
        }
    }

    case TOOL_CURVE:
    case TOOL_SELECT:
    default:
        break;
    }
    enableCursor();
    paintRenderToolbar();
}

void paintPrevTool()
{
    switch (paintState->brush)
    {
    case TOOL_PEN_SQUARE:
        paintState->brush = TOOL_PEN_CIRCLE;
        break;

    case TOOL_PEN_CIRCLE:
        paintState->brush = TOOL_BOX_EMPTY;
        break;

    case TOOL_BOX_EMPTY:
        paintState->brush = TOOL_BOX_FILL;
        break;

    case TOOL_BOX_FILL:
        paintState->brush = TOOL_CIRCLE_EMPTY;
        break;

    case TOOL_CIRCLE_EMPTY:
        paintState->brush = TOOL_CIRCLE_FILL;
        break;

    case TOOL_CIRCLE_FILL:
        paintState->brush = TOOL_ELLIPSE;
        break;

    case TOOL_ELLIPSE:
        paintState->brush = TOOL_LINE;
        break;

    case TOOL_LINE:
        paintState->brush = TOOL_FLOOD_FILL;
        break;

    case TOOL_FLOOD_FILL:
        paintState->brush = TOOL_PEN_SQUARE;
        break;

    default:
        paintState->brush = TOOL_PEN_SQUARE;
        break;
    }

    paintState->brushState = PICK_START;

    if (paintState->brush == TOOL_PEN_SQUARE || paintState->brush == TOOL_PEN_CIRCLE)
    {
        paintState->toolHoldable = true;
    }
    else
    {
        paintState->toolHoldable = false;
    }
}

void paintNextTool()
{

    switch (paintState->brush)
    {
    case TOOL_PEN_SQUARE:
        paintState->brush = TOOL_FLOOD_FILL;
        break;

    case TOOL_PEN_CIRCLE:
        paintState->brush = TOOL_PEN_SQUARE;
        break;

    case TOOL_BOX_EMPTY:
        paintState->brush = TOOL_PEN_CIRCLE;
        break;

    case TOOL_BOX_FILL:
        paintState->brush = TOOL_BOX_EMPTY;
        break;

    case TOOL_CIRCLE_EMPTY:
        paintState->brush = TOOL_BOX_FILL;
        break;

    case TOOL_CIRCLE_FILL:
        paintState->brush = TOOL_CIRCLE_EMPTY;
        break;

    case TOOL_ELLIPSE:
        paintState->brush = TOOL_CIRCLE_FILL;
        break;

    case TOOL_LINE:
        paintState->brush = TOOL_ELLIPSE;
        break;

    case TOOL_FLOOD_FILL:
        paintState->brush = TOOL_LINE;
        break;

    default:
        paintState->brush = TOOL_PEN_SQUARE;
        break;
    }

    paintState->brushState = PICK_START;

    if (paintState->brush == TOOL_PEN_SQUARE || paintState->brush == TOOL_PEN_CIRCLE)
    {
        paintState->toolHoldable = true;
    }
    else
    {
        paintState->toolHoldable = false;
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
}

void paintMoveCursorRel(int8_t xDiff, int8_t yDiff)
{
    ESP_LOGD("Paint", "aHeld: %d", paintState->aHeld);
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

    ESP_LOGD("Paint", "Cursor moved by (%d, %d) to (%d, %d)", xDiff, yDiff, paintState->cursorX, paintState->cursorY);
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
        ESP_LOGD("Paint", "paletteIndex[%02d] = %02x", (uint8_t)paintState->recentColors[i], i);
        paletteIndex[((uint8_t)paintState->recentColors[i])] = i;
    }

    // Allocate space for the chunk
    uint32_t totalPx = paintState->canvasH * paintState->canvasW;
    uint32_t lastChunkSize = totalPx % PAINT_SAVE_CHUNK_SIZE;
    ESP_LOGD("Paint", "lastChunkSize: %d", lastChunkSize);

    // TODO: Figure out why we need twice as many chunks as expected
    uint8_t chunkCount = ((totalPx) + PAINT_SAVE_CHUNK_SIZE - 1) / PAINT_SAVE_CHUNK_SIZE;
    if ((imgChunk = malloc(sizeof(uint8_t) * PAINT_SAVE_CHUNK_SIZE)) != NULL)
    {
        ESP_LOGD("Paint", "Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        ESP_LOGE("Paint", "malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    disableCursor();

    uint16_t x0, y0, x1, y1;
    // Write all the chunks
    for (uint32_t i = 0; i < chunkCount; i++)
    {
        // build the chunk
        for (uint16_t n = 0; n < PAINT_SAVE_CHUNK_SIZE; n++)
        {
            // calculate the real coordinates given the pixel indices
            // (we store 2 pixels in each byte)
            // that's 100% more pixel, per pixel!
            x0 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2)) % paintState->canvasW;
            y0 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2)) / paintState->canvasW;
            x1 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2 + 1)) % paintState->canvasW;
            y1 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2 + 1)) / paintState->canvasW;

            if (y0 >= PAINT_CANVAS_Y_OFFSET + paintState->canvasH)
            {
                ESP_LOGD("Paint", "Skipping chunk %d at pixel %d", i, n);
                break;
            }

            // this is all probably horribly inefficient
            // TODO: Handle if the image does not end on a chunk boundary
            imgChunk[n] = paletteIndex[(uint8_t)paintState->disp->getPx(x0, y0)] << 4 | paletteIndex[(uint8_t)paintState->disp->getPx(x1, y1)];
        }

        // save the chunk
        snprintf(key, 16, "paint_%02dc%05d", slot, i);
        if (writeNvsBlob(key, imgChunk, PAINT_SAVE_CHUNK_SIZE))
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
    uint8_t chunkCount = ((totalPx) + PAINT_SAVE_CHUNK_SIZE - 1) / PAINT_SAVE_CHUNK_SIZE;
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
        for (uint16_t n = 0; n < PAINT_SAVE_CHUNK_SIZE; n++)
        {
            // calculate the real coordinates given the pixel indices
            // (we store 2 pixels in each byte)
            // that's 100% more pixel, per pixel!
            x0 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2)) % paintState->canvasW;
            y0 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2)) / paintState->canvasW;
            x1 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2 + 1)) % paintState->canvasW;
            y1 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE) + (n * 2 + 1)) / paintState->canvasW;

            if (i == 0)
            {
                ESP_LOGD("Paint", "[%03d]      = %04x", n, imgChunk[n]);
                ESP_LOGD("Paint", "(%03d, %03d) = %02x", x0, y0, (imgChunk[n] >> 4) & 0xF);
                ESP_LOGD("Paint", "(%03d, %03d) = %02x", x1, y1, imgChunk[n] & 0xF);
            }

            // prevent out-of-bounds drawing
            if (y0 >= PAINT_CANVAS_Y_OFFSET + paintState->canvasH)
            {
                break;
            }
            paintState->disp->setPx(x0, y0, paintState->recentColors[imgChunk[n] >> 4]);

            if (y1 >= PAINT_CANVAS_Y_OFFSET + paintState->canvasH)
            {
                break;
            }
            paintState->disp->setPx(x1, y1, paintState->recentColors[imgChunk[n] & 0xF]);
        }
    }

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();

    paintState->wasSaved = false;
}