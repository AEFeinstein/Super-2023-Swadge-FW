#include "paint_draw.h"

#include "paint_ui.h"
#include "paint_brush.h"
#include "paint_nvs.h"
#include "paint_util.h"

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

void paintDrawScreenSetup(void)
{
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


void paintDrawScreenMainLoop(int64_t elapsedUs)
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
                paintLoad(paintState->selectedSlot, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_SCALE, PAINT_CANVAS_SCALE);
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
                    do
                    {
                        // Switch to the previous slot, wrapping back to the end
                        paintState->selectedSlot = (paintState->selectedSlot == 0) ? PAINT_SAVE_SLOTS - 1 : paintState->selectedSlot - 1;
                    }
                    // If we're loading, and there's actually a slot we can load from, skip empty slots until we find one that is in use
                    while (!paintState->isSaveSelected && paintGetAnySlotInUse() && !paintGetSlotInUse(paintState->selectedSlot));
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
                    do
                    {
                        paintState->selectedSlot = (paintState->selectedSlot + 1) % PAINT_SAVE_SLOTS;
                    }
                    // If we're loading, and there's actually a slot we can load from, skip empty slots until we find one that is in use
                    while (!paintState->isSaveSelected && paintGetAnySlotInUse() && !paintGetSlotInUse(paintState->selectedSlot));
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

void setCursor(const wsg_t* cursorWsg)
{
    restoreCursorPixels();
    paintState->cursorWsg = cursorWsg;
    paintRenderCursor();
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
