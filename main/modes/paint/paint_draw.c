#include "paint_draw.h"

#include <string.h>

#include "paint_ui.h"
#include "paint_brush.h"
#include "paint_nvs.h"
#include "paint_util.h"

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

static paletteColor_t defaultPalette[] =
{
    c000, // black
    c555, // white
    c222, // light gray
    c444, // dark gray
    c500, // red
    c550, // yellow
    c050, // green
    c055, // cyan

    c005, // blue
    c530, // orange?
    c505, // fuchsia
    c503, // pink
    c350, // lime green
    c035, // sky blue
    c522, // salmon
    c103, // dark blue
};

const brush_t brushes[] =
{
    { .name = "Square Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawSquarePen },
    { .name = "Circle Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawCirclePen },
    { .name = "Line",       .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawLine },
    { .name = "Bezier Curve", .mode = PICK_POINT, .maxPoints = 4, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCurve },
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

const brush_t* firstBrush = brushes;
const brush_t* lastBrush = brushes + sizeof(brushes) / sizeof(brushes[0]) - 1;

void paintDrawScreenSetup(void)
{
    paintState->screen = PAINT_DRAW;
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

    // Set up the canvas with defaults
    // TODO move this into a function for getting a clean canvas
    // TODO also get rid of hardcoded offsets and dynamically position the canvas out of the way of the UI
    paintState->canvas.disp = paintState->disp;

    paintState->canvas.xScale = PAINT_CANVAS_SCALE;
    paintState->canvas.yScale = PAINT_CANVAS_SCALE;

    paintState->canvas.w = PAINT_CANVAS_WIDTH;
    paintState->canvas.h = PAINT_CANVAS_HEIGHT;

    paintState->canvas.x = PAINT_CANVAS_X_OFFSET;
    paintState->canvas.y = PAINT_CANVAS_Y_OFFSET;

    // load the default palette
    memcpy(paintState->canvas.palette, defaultPalette, sizeof(defaultPalette) / sizeof(paletteColor_t));

    // Init the cursors for each artist
    // TODO only do one for singleplayer?
    for (uint8_t i = 0; i < sizeof(paintState->artist) / sizeof(paintState->artist[0]); i++)
    {
        initCursor(&paintState->artist[i].cursor, &paintState->canvas, &cursorBoxWsg);
        initPxStack(&paintState->artist[i].pickPoints);
        paintState->artist[i].brushDef = firstBrush;
        paintState->artist[i].brushWidth = firstBrush->minSize;

        // set the fg/bg colors to the first 2 palette colors
        paintState->artist[i].fgColor = paintState->canvas.palette[0];
        paintState->artist[i].bgColor = paintState->canvas.palette[1];
    }

    paintState->disp->clearPx();

    // Clear the LEDs
    // Might not be necessary here
    paintUpdateLeds();

    PAINT_LOGI("It's paintin' time! Canvas is %d x %d pixels!", paintState->canvas.w, paintState->canvas.h);
}

void paintDrawScreenCleanup(void)
{
    for (uint8_t i = 0; i < sizeof(paintState->artist) / sizeof(paintState->artist[0]) ;i++)
    {
        deinitCursor(&paintState->artist[i].cursor);
        freePxStack(&paintState->artist[i].pickPoints);
    }
}


void paintDrawScreenMainLoop(int64_t elapsedUs)
{
    if (paintState->clearScreen)
    {
        paintClearCanvas(&paintState->canvas, getArtist()->bgColor);
        paintRenderToolbar(getArtist(), &paintState->canvas);
        paintUpdateLeds();
        showCursor(getCursor(), &paintState->canvas);
        paintState->clearScreen = false;
    }

    if (paintState->doSave)
    {
        paintState->saveInProgress = true;
        if (paintState->isSaveSelected)
        {
            hideCursor(getCursor(), &paintState->canvas);
            paintSave(&paintState->canvas, paintState->selectedSlot);
            showCursor(getCursor(), &paintState->canvas);
        }
        else
        {
            if (paintGetSlotInUse(paintGetRecentSlot()))
            {
                // Load from the selected slot if it's been used
                hideCursor(getCursor(), &paintState->canvas);
                paintClearCanvas(&paintState->canvas, getArtist()->bgColor);
                paintLoad(&paintState->canvas, paintState->selectedSlot);
                showCursor(getCursor(), &paintState->canvas);
                paintUpdateLeds();
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
        paintUpdateLeds();
        paintState->recolorPickPoints = false;
    }


    if (paintState->redrawToolbar)
    {
        paintRenderToolbar(getArtist(), &paintState->canvas);
        paintState->redrawToolbar = false;
    }
    else
    {
        // Don't remember why we only do this when redrawToolbar is true
        // Oh, it's because `paintState->redrawToolbar` is mostly only set in select mode unless you press B?
        if (paintState->aHeld)
        {
            paintDoTool(getCursor()->x, getCursor()->y, getArtist()->fgColor);

            if (getArtist()->brushDef->mode != HOLD_DRAW)
            {
                paintState->aHeld = false;
            }
        }

        if (paintState->moveX || paintState->moveY)
        {
            paintState->btnHoldTime += elapsedUs;
            if (paintState->firstMove || paintState->btnHoldTime >= BUTTON_REPEAT_TIME)
            {

                moveCursorRelative(getCursor(), &paintState->canvas, paintState->moveX, paintState->moveY);
                paintRenderToolbar(getArtist(), &paintState->canvas);

                paintState->firstMove = false;
            }
        }
    }

    paintDrawPickPoints();

    drawCursor(getCursor(), &paintState->canvas);
}

void paintSaveModeButtonCb(const buttonEvt_t* evt)
{
    if (evt->down)
    {
        //////// Save menu button down
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
                    // But if no slots are in use, don't switch to "load"
                    paintState->isSaveSelected = !paintState->isSaveSelected || !paintGetAnySlotInUse();

                    // TODO move this and friends into a function
                    while (!paintState->isSaveSelected && paintGetAnySlotInUse() && !paintGetSlotInUse(paintState->selectedSlot))
                    {
                        paintState->selectedSlot = (paintState->selectedSlot + 1) % PAINT_SAVE_SLOTS;
                    }
                }
                break;
            }

            case LEFT:
            {
                if (paintState->saveMenu == PICK_SLOT_SAVE_LOAD)
                {
                    // Previous save slot
                    if (paintState->isSaveSelected)
                    {
                        // Just get the previous slot
                        paintState->selectedSlot = PREV_WRAP(paintState->selectedSlot, PAINT_SAVE_SLOTS);
                    }
                    else
                    {
                        paintState->selectedSlot = paintGetPrevSlotInUse(paintState->selectedSlot);
                    }
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
                    if (paintState->isSaveSelected)
                    {
                        paintState->selectedSlot = NEXT_WRAP(paintState->selectedSlot, PAINT_SAVE_SLOTS);
                    }
                    else
                    {
                        paintState->selectedSlot = paintGetNextSlotInUse(paintState->selectedSlot);
                    }
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
    else
    {
        //////// Save mode button release
        if (evt->button == START)
        {
            // Exit save menu
            paintState->saveMenu = HIDDEN;
            paintState->buttonMode = BTN_MODE_DRAW;
            paintState->redrawToolbar = true;
        }
    }
}

void paintSelectModeButtonCb(const buttonEvt_t* evt)
{
    if (!evt->down)
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
}

void paintDrawModeButtonCb(const buttonEvt_t* evt)
{
    if (evt->down)
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
                swap(&getArtist()->fgColor, &getArtist()->bgColor);
                swap(&paintState->canvas.palette[0], &paintState->canvas.palette[1]);

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
    else
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
}

void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col)
{
    hideCursor(getCursor(), &paintState->canvas);
    bool drawNow = false;
    bool isLastPick = false;

    // Determine if this is the last pick for the tool
    // This is so we don't draw a pick-marker that will be immediately removed
    switch (getArtist()->brushDef->mode)
    {
        case PICK_POINT:
        isLastPick = (pxStackSize(&getArtist()->pickPoints) + 1 == getArtist()->brushDef->maxPoints);
        break;

        case PICK_POINT_LOOP:
        isLastPick = pxStackSize(&getArtist()->pickPoints) + 1 == MAX_PICK_POINTS - 1 || pxStackSize(&getArtist()->pickPoints) + 1 == getArtist()->brushDef->maxPoints - 1;
        break;

        case HOLD_DRAW:
        break;

        case INSTANT:
        break;

        default:
        break;
    }

    pushPxScaled(&getArtist()->pickPoints, paintState->disp, getCursor()->x, getCursor()->y, paintState->canvas.x, paintState->canvas.y, paintState->canvas.xScale, paintState->canvas.yScale);

    if (getArtist()->brushDef->mode == HOLD_DRAW)
    {
        drawNow = true;
    }
    else if (getArtist()->brushDef->mode == PICK_POINT || getArtist()->brushDef->mode == PICK_POINT_LOOP)
    {
        PAINT_LOGD("pick[%02zu] = SCR(%03d, %03d) / CNV(%d, %d)", pxStackSize(&getArtist()->pickPoints), CNV2SCR_X(x), CNV2SCR_Y(y), x, y);

        // Save the pixel underneath the selection, then draw a temporary pixel to mark it
        // But don't bother if this is the last pick point, since it will never actually be seen

        if (getArtist()->brushDef->mode == PICK_POINT_LOOP)
        {
            pxVal_t firstPick, lastPick;
            if (pxStackSize(&getArtist()->pickPoints) > 1 && getPx(&getArtist()->pickPoints, 0, &firstPick) && peekPx(&getArtist()->pickPoints, &lastPick) && firstPick.x == lastPick.x && firstPick.y == lastPick.y)
            {
                // If this isn't the first pick, and it's in the same position as the first pick, we're done!
                drawNow = true;
            }
            else if (isLastPick)
            {
                // Special case: If we're on the next-to-last possible point, we have to add the start again as the last point
                pushPx(&getArtist()->pickPoints, paintState->disp, firstPick.x, firstPick.y);

                drawNow = true;
            }
        }
        // only for non-loop brushes
        else if (pxStackSize(&getArtist()->pickPoints) == getArtist()->brushDef->maxPoints)
        {
            drawNow = true;
        }
    }
    else if (getArtist()->brushDef->mode == INSTANT)
    {
        drawNow = true;
    }

    if (drawNow)
    {
        // Allocate an array of point_t for the canvas pick points
        size_t pickCount = pxStackSize(&getArtist()->pickPoints);
        point_t* canvasPickPoints = malloc(sizeof(point_t) * pickCount);

        // Convert the pick points into an array of canvas-coordinates
        paintConvertPickPointsScaled(&getArtist()->pickPoints, &paintState->canvas, canvasPickPoints);

        while (popPxScaled(&getArtist()->pickPoints, paintState->disp, paintState->canvas.xScale, paintState->canvas.yScale));

        getArtist()->brushDef->fnDraw(&paintState->canvas, canvasPickPoints, pickCount, getArtist()->brushWidth, col);

        free(canvasPickPoints);
    }

    showCursor(getCursor(), &paintState->canvas);
    paintRenderToolbar(getArtist(), &paintState->canvas);
}


void paintSetupTool(void)
{
    // Reset the brush params
    if (getArtist()->brushWidth < getArtist()->brushDef->minSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->minSize;
    }
    else if (getArtist()->brushWidth > getArtist()->brushDef->maxSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->maxSize;
    }

    // Remove any stored temporary pixels
    while (popPxScaled(&getArtist()->pickPoints, paintState->disp, paintState->canvas.xScale, paintState->canvas.yScale));
}

void paintPrevTool(void)
{
    if (getArtist()->brushDef == firstBrush)
    {
        getArtist()->brushDef = lastBrush;
    }
    else
    {
        getArtist()->brushDef--;
    }

    paintSetupTool();
}

void paintNextTool(void)
{
    if (getArtist()->brushDef == lastBrush)
    {
        getArtist()->brushDef = firstBrush;
    }
    else
    {
        getArtist()->brushDef++;
    }

    paintSetupTool();
}

void paintDecBrushWidth()
{
    if (getArtist()->brushWidth == 0 || getArtist()->brushWidth <= getArtist()->brushDef->minSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->minSize;
    }
    else
    {
        getArtist()->brushWidth--;
    }
}

void paintIncBrushWidth()
{
    getArtist()->brushWidth++;

    if (getArtist()->brushWidth > getArtist()->brushDef->maxSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->maxSize;
    }
}

void paintUpdateRecents(uint8_t selectedIndex)
{
    getArtist()->fgColor = paintState->canvas.palette[selectedIndex];

    for (uint8_t i = selectedIndex; i > 0; i--)
    {
        paintState->canvas.palette[i] = paintState->canvas.palette[i-1];
    }
    paintState->canvas.palette[0] = getArtist()->fgColor;

    paintUpdateLeds();

    // If there are any pick points, update their color to reduce confusion
    paintDrawPickPoints();
}

void paintUpdateLeds(void)
{
    uint32_t rgb = paletteToRGB(getArtist()->fgColor);
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        paintState->leds[i].b = (rgb >>  0) & 0xFF;
        paintState->leds[i].g = (rgb >>  8) & 0xFF;
        paintState->leds[i].r = (rgb >> 16) & 0xFF;
    }

    setLeds(paintState->leds, NUM_LEDS);
}

void paintDrawPickPoints(void)
{
    pxVal_t point;
    for (size_t i = 0; i < pxStackSize(&getArtist()->pickPoints); i++)
    {
        if (getPx(&getArtist()->pickPoints, i, &point))
        {
            PAINT_LOGD("Drawing pick point[%zu] @ (%d, %d) == %d", i, point.x, point.y, getArtist()->fgColor);
            plotRectFilled(paintState->disp, point.x, point.y, point.x + paintState->canvas.xScale + 1, point.y + paintState->canvas.yScale + 1, getArtist()->fgColor);
        }
    }
}

void paintHidePickPoints(void)
{
    pxVal_t point;
    for (size_t i = 0; i < pxStackSize(&getArtist()->pickPoints); i++)
    {
        if (getPx(&getArtist()->pickPoints, i, &point))
        {
            plotRectFilled(paintState->disp, point.x, point.y, point.x + paintState->canvas.xScale + 1, point.y + paintState->canvas.yScale + 1, point.col);
        }
    }
}

paintArtist_t* getArtist(void)
{
    // TODO: Take player order into account
    return paintState->artist;
}

paintCursor_t* getCursor(void)
{
    // TODO Take player order into account
    return &paintState->artist->cursor;
}
