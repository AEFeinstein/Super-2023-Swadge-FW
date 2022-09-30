#include "paint_draw.h"

#include "paint_brush.h"


static const char startMenuSave[] = "Save";
static const char startMenuLoad[] = "Load";
static const char startMenuSlot[] = "Slot %d";
static const char startMenuSlotUsed[] = "Slot %d (!)";
static const char startMenuOverwrite[] = "Overwrite?";
static const char startMenuYes[] = "Yes";
static const char startMenuNo[] = "No";


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