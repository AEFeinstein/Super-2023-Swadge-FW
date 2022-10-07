#include "paint_draw.h"

#include <string.h>

#include "paint_ui.h"
#include "paint_brush.h"
#include "paint_nvs.h"
#include "paint_util.h"
#include "mode_paint.h"

paintDraw_t* paintState;

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

brush_t brushes[] =
{
    { .name = "Square Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawSquarePen, .iconName = "square_pen" },
    { .name = "Circle Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawCirclePen, .iconName = "circle_pen" },
    { .name = "Line",       .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawLine, .iconName = "line" },
    { .name = "Bezier Curve", .mode = PICK_POINT, .maxPoints = 4, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCurve, .iconName = "curve" },
    { .name = "Rectangle",  .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawRectangle, .iconName = "rect" },
    { .name = "Filled Rectangle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledRectangle, .iconName = "rect_filled" },
    { .name = "Circle",     .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCircle, .iconName = "circle" },
    { .name = "Filled Circle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledCircle, .iconName = "circle_filled" },
    { .name = "Ellipse",    .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawEllipse, .iconName = "ellipse" },
    { .name = "Polygon",    .mode = PICK_POINT_LOOP, .maxPoints = 16, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawPolygon, .iconName = "polygon" },
    { .name = "Squarewave", .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawSquareWave, .iconName = "squarewave" },
    { .name = "Paint Bucket", .mode = PICK_POINT, .maxPoints = 1, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawPaintBucket, .iconName = "paint_bucket" },
};

const char activeIconStr[] = "%s_active.wsg";
const char inactiveIconStr[] = "%s_inactive.wsg";

brush_t* firstBrush = brushes;
brush_t* lastBrush = brushes + sizeof(brushes) / sizeof(brushes[0]) - 1;

void paintDrawScreenSetup(display_t* disp)
{
    paintState = calloc(sizeof(paintDraw_t), 1);
    paintState->disp = disp;
    paintState->canvas.disp = disp;

    loadFont(PAINT_TOOLBAR_FONT, &(paintState->toolbarFont));
    loadFont(PAINT_SAVE_MENU_FONT, &(paintState->saveMenuFont));
    paintState->clearScreen = true;


    // Set up the brush icons
    char iconName[32];
    for (brush_t* brush = firstBrush; brush <= lastBrush; brush++)
    {
        snprintf(iconName, sizeof(iconName), activeIconStr, brush->iconName);
        if (!loadWsg(iconName, &brush->iconActive))
        {
            PAINT_LOGE("Loading icon %s failed!!!", iconName);
        }

        snprintf(iconName, sizeof(iconName), inactiveIconStr, brush->iconName);
        if (!loadWsg(iconName, &brush->iconInactive))
        {
            PAINT_LOGE("Loading icon %s failed!!!", iconName);
        }
    }

    // Setup the margins
    // Top: Leave room for the toolbar text plus padding above and below it, plus 1px for canvas border
    paintState->marginTop = paintState->toolbarFont.h + 2 * PAINT_TOOLBAR_TEXT_PADDING_Y + 1;
    // Left: Leave room for the color boxes, their margins, their borders, and the canvas border
    paintState->marginLeft = PAINT_COLORBOX_W + PAINT_COLORBOX_MARGIN_X * 2 + 2 + 1;
    // Bottom: Leave room for the progress bar, plus 1px of separation, and canvas border
    paintState->marginBottom = 10 + 1 + 1;
    // Right: We just need to stay away from the rounded corner, so like, 12px?
    paintState->marginRight = 12;

    paintLoadIndex(&paintState->index);

    if (paintGetAnySlotInUse(paintState->index))
    {
        // If there's a saved image, load that
        paintState->selectedSlot = paintGetRecentSlot(paintState->index);
        paintState->doLoad = true;
    }
    else
    {
        // Set up a blank canvas with the default size
        paintState->canvas.w = PAINT_DEFAULT_CANVAS_WIDTH;
        paintState->canvas.h = PAINT_DEFAULT_CANVAS_HEIGHT;

        // Automatically position the canvas in the center of the drawable area at the max scale that will fit
        paintPositionDrawCanvas();

        // load the default palette
        memcpy(paintState->canvas.palette, defaultPalette, sizeof(defaultPalette) / sizeof(paletteColor_t));
        getArtist()->fgColor = paintState->canvas.palette[0];
        getArtist()->bgColor = paintState->canvas.palette[1];
    }

    // Init the cursors for each artist
    // TODO only do one for singleplayer?
    for (uint8_t i = 0; i < sizeof(paintState->artist) / sizeof(paintState->artist[0]); i++)
    {
        initCursor(&paintState->artist[i].cursor, &paintState->canvas, &cursorBoxWsg);
        initPxStack(&paintState->artist[i].pickPoints);
        paintState->artist[i].brushDef = firstBrush;
        paintState->artist[i].brushWidth = firstBrush->minSize;
    }

    paintState->disp->clearPx();

    // Clear the LEDs
    // Might not be necessary here
    paintUpdateLeds();

    PAINT_LOGI("It's paintin' time! Canvas is %d x %d pixels!", paintState->canvas.w, paintState->canvas.h);
}

void paintDrawScreenCleanup(void)
{
    for (brush_t* brush = firstBrush; brush <= lastBrush; brush++)
    {
        freeWsg(&brush->iconActive);
        freeWsg(&brush->iconInactive);
    }

    for (uint8_t i = 0; i < sizeof(paintState->artist) / sizeof(paintState->artist[0]) ;i++)
    {
        deinitCursor(&paintState->artist[i].cursor);
        freePxStack(&paintState->artist[i].pickPoints);
    }

    freeFont(&paintState->saveMenuFont);
    freeFont(&paintState->toolbarFont);
    free(paintState);
}

void paintPositionDrawCanvas(void)
{
    // Calculate the highest scale that will fit on the screen
    uint8_t scale = paintGetMaxScale(paintState->disp, paintState->canvas.w, paintState->canvas.h, paintState->marginLeft + paintState->marginRight, paintState->marginTop + paintState->marginBottom);

    paintState->canvas.xScale = scale;
    paintState->canvas.yScale = scale;

    paintState->canvas.x = paintState->marginLeft + (paintState->canvas.disp->w - paintState->marginLeft - paintState->marginRight - paintState->canvas.w * paintState->canvas.xScale) / 2;
    paintState->canvas.y = paintState->marginTop + (paintState->canvas.disp->h - paintState->marginTop - paintState->marginBottom - paintState->canvas.h * paintState->canvas.yScale) / 2;
}

void paintDrawScreenMainLoop(int64_t elapsedUs)
{
    if (paintState->clearScreen)
    {
        paintClearCanvas(&paintState->canvas, getArtist()->bgColor);
        paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);
        paintUpdateLeds();
        showCursor(getCursor(), &paintState->canvas);
        paintState->unsaved = false;
        paintState->clearScreen = false;
    }

    if (paintState->doSave || paintState->doLoad)
    {
        paintState->saveInProgress = true;

        if (paintState->doSave)
        {
            hideCursor(getCursor(), &paintState->canvas);
            paintHidePickPoints();
            paintSave(&paintState->index, &paintState->canvas, paintState->selectedSlot);
            paintDrawPickPoints();
            showCursor(getCursor(), &paintState->canvas);
        }
        else
        {
            if (paintGetSlotInUse(paintState->index, paintState->selectedSlot))
            {
                // Load from the selected slot if it's been used
                hideCursor(getCursor(), &paintState->canvas);
                paintClearCanvas(&paintState->canvas, getArtist()->bgColor);
                paintLoadDimensions(&paintState->canvas, paintState->selectedSlot);
                paintPositionDrawCanvas();
                paintLoad(&paintState->index, &paintState->canvas, paintState->selectedSlot);
                paintSetRecentSlot(&paintState->index, paintState->selectedSlot);

                getArtist()->fgColor = paintState->canvas.palette[0];
                getArtist()->bgColor = paintState->canvas.palette[1];

                showCursor(getCursor(), &paintState->canvas);
                paintUpdateLeds();
            }
            else
            {
                // If the slot hasn't been used yet, just clear the screen
                paintState->clearScreen = true;
            }
        }

        paintState->unsaved = false;
        paintState->doSave = false;
        paintState->doLoad = false;
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
        paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);
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
                paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);

                paintState->firstMove = false;
            }
        }
    }

    paintDrawPickPoints();

    drawCursor(getCursor(), &paintState->canvas);
}

void paintSaveModePrevItem(void)
{
    switch (paintState->saveMenu)
    {
        case HIDDEN:
        break;

        case PICK_SLOT_SAVE:
        case CONFIRM_OVERWRITE:
            paintState->saveMenu = EXIT;
        break;

        case PICK_SLOT_LOAD:
        case CONFIRM_UNSAVED:
            paintState->saveMenu = PICK_SLOT_SAVE;
        break;

        case CLEAR:
        case CONFIRM_CLEAR:
            paintState->saveMenu = PICK_SLOT_LOAD;
        break;

        case EXIT:
        case CONFIRM_EXIT:
            paintState->saveMenu = CLEAR;
        break;
    }

    paintState->saveMenuBoolOption = false;

    // If we're selecting "Load", then make sure we can actually load a slot
    if (paintState->saveMenu == PICK_SLOT_LOAD)
    {
        // If no slots are in use, skip again
        if (!paintGetAnySlotInUse(paintState->index))
        {
            paintState->saveMenu = PICK_SLOT_SAVE;
        }
        else if (!paintGetSlotInUse(paintState->index, paintState->selectedSlot))
        {
            // Otherwise, make sure the selected slot is in use
            paintState->selectedSlot = paintGetNextSlotInUse(paintState->index, paintState->selectedSlot);
        }
    }
}

void paintSaveModeNextItem(void)
{
    switch (paintState->saveMenu)
    {
        case HIDDEN:
        break;

        case PICK_SLOT_SAVE:
        case CONFIRM_OVERWRITE:
            paintState->saveMenu = PICK_SLOT_LOAD;
        break;

        case PICK_SLOT_LOAD:
        case CONFIRM_UNSAVED:
            paintState->saveMenu = CLEAR;
        break;

        case CLEAR:
        case CONFIRM_CLEAR:
            paintState->saveMenu = EXIT;
        break;

        case EXIT:
        case CONFIRM_EXIT:
            paintState->saveMenu = PICK_SLOT_SAVE;
        break;
    }

    paintState->saveMenuBoolOption = false;

    // If we're selecting "Load", then make sure we can actually load a slot
    if (paintState->saveMenu == PICK_SLOT_LOAD)
    {
        // If no slots are in use, skip again
        if (!paintGetAnySlotInUse(paintState->index))
        {
            paintState->saveMenu = CLEAR;
        }
        else if (!paintGetSlotInUse(paintState->index, paintState->selectedSlot))
        {
            // Otherwise, make sure the selected slot is in use
            paintState->selectedSlot = paintGetNextSlotInUse(paintState->index, paintState->selectedSlot);
        }
    }
}

void paintSaveModePrevOption(void)
{
    switch (paintState->saveMenu)
    {
        case PICK_SLOT_SAVE:
            paintState->selectedSlot = PREV_WRAP(paintState->selectedSlot, PAINT_SAVE_SLOTS);
        break;

        case PICK_SLOT_LOAD:
            paintState->selectedSlot = paintGetPrevSlotInUse(paintState->index, paintState->selectedSlot);
        break;

        case CONFIRM_OVERWRITE:
        case CONFIRM_UNSAVED:
        case CONFIRM_CLEAR:
        case CONFIRM_EXIT:
            // Just flip the state
            paintState->saveMenuBoolOption = !paintState->saveMenuBoolOption;
        break;

        case HIDDEN:
        case CLEAR:
        case EXIT:
        // Do nothing, there are no options here
        break;
    }
}

void paintSaveModeNextOption(void)
{
    switch (paintState->saveMenu)
    {
        case PICK_SLOT_SAVE:
            paintState->selectedSlot = NEXT_WRAP(paintState->selectedSlot, PAINT_SAVE_SLOTS);
        break;

        case PICK_SLOT_LOAD:
            paintState->selectedSlot = paintGetNextSlotInUse(paintState->index, paintState->selectedSlot);
        break;

        case CONFIRM_OVERWRITE:
        case CONFIRM_UNSAVED:
        case CONFIRM_CLEAR:
        case CONFIRM_EXIT:
            // Just flip the state
            paintState->saveMenuBoolOption = !paintState->saveMenuBoolOption;
        break;

        case HIDDEN:
        case CLEAR:
        case EXIT:
        // Do nothing, there are no options here
        break;
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
                switch (paintState->saveMenu)
                {
                    case PICK_SLOT_SAVE:
                    {
                        if (paintGetSlotInUse(paintState->index, paintState->selectedSlot))
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_OVERWRITE;
                        }
                        else
                        {
                            paintState->doSave = true;
                        }
                        break;
                    }

                    case PICK_SLOT_LOAD:
                    {
                        if (paintState->unsaved)
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_UNSAVED;
                        }
                        else
                        {
                            paintState->doLoad = true;
                        }
                        break;
                    }

                    case CONFIRM_OVERWRITE:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintState->doSave = true;
                            paintState->saveMenu = HIDDEN;
                        }
                        else
                        {
                            paintState->saveMenu = PICK_SLOT_SAVE;
                        }
                        break;
                    }

                    case CONFIRM_UNSAVED:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintState->doLoad = true;
                            paintState->saveMenu = HIDDEN;
                        }
                        else
                        {
                            paintState->saveMenu = PICK_SLOT_LOAD;
                        }
                        break;
                    }

                    case CONFIRM_CLEAR:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintState->clearScreen = true;
                            paintState->saveMenu = HIDDEN;
                        }
                        else
                        {
                            paintState->saveMenu = CLEAR;
                        }
                        break;
                    }

                    case CONFIRM_EXIT:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintReturnToMainMenu();
                        }
                        else
                        {
                            paintState->saveMenu = EXIT;
                        }
                        break;
                    }

                    case CLEAR:
                    {
                        if (paintState->unsaved)
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_CLEAR;
                        }
                        else
                        {
                            paintState->clearScreen = true;
                            paintState->saveMenu = HIDDEN;
                        }
                        break;
                    }
                    case EXIT:
                    {
                        if (paintState->unsaved)
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_EXIT;
                        }
                        else
                        {
                            paintReturnToMainMenu();
                        }
                        break;
                    }
                    case HIDDEN:
                    {
                        paintState->buttonMode = BTN_MODE_DRAW;
                        break;
                    }
                }
                break;
            }

            case UP:
            {
                paintSaveModePrevItem();
                break;
            }

            case DOWN:
            case SELECT:
            {

                paintSaveModeNextItem();
                break;
            }

            case LEFT:
            {
                paintSaveModePrevOption();
                break;
            }

            case RIGHT:
            {
                paintSaveModeNextOption();
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

void paintDrawScreenButtonCb(const buttonEvt_t* evt)
{
    switch (paintState->buttonMode)
    {
        case BTN_MODE_DRAW:
        {
            paintDrawModeButtonCb(evt);
            break;
        }

        case BTN_MODE_SELECT:
        {
            paintSelectModeButtonCb(evt);
            break;
        }

        case BTN_MODE_SAVE:
        {
            paintSaveModeButtonCb(evt);
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
                    paintState->saveMenu = PICK_SLOT_SAVE;
                    paintState->redrawToolbar = true;
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
                paintState->saveMenuBoolOption = false;
                // TODO: Why did this work? I'm pretty sure this should be BTN_MODE_SELECT
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
        isLastPick = pxStackSize(&getArtist()->pickPoints) + 1 == getArtist()->brushDef->maxPoints - 1;
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

        paintState->unsaved = true;
        getArtist()->brushDef->fnDraw(&paintState->canvas, canvasPickPoints, pickCount, getArtist()->brushWidth, col);

        free(canvasPickPoints);
    }

    showCursor(getCursor(), &paintState->canvas);
    paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);
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

    // Undraw and hide any stored temporary pixels
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
