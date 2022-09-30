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

#include "mode_main_menu.h"
#include "swadge_util.h"

#include "mode_paint.h"
#include "paint_common.h"
#include "paint_util.h"

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

static const char paintTitle[] = "MFPaint";
static const char menuOptDraw[] = "Draw";
static const char menuOptGallery[] = "Gallery";
static const char menuOptReceive[] = "Receive";
static const char menuOptExit[] = "Exit";

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

// Util function declarations

void paintInitialize(void);

void drawColorBox(uint16_t xOffset, uint16_t yOffset, uint16_t w, uint16_t h, paletteColor_t col, bool selected, paletteColor_t topBorder, paletteColor_t bottomBorder);

// Tool util functions
void paintSetupTool(void);
void paintPrevTool(void);
void paintNextTool(void);
void paintDecBrushWidth(void);
void paintIncBrushWidth(void);

// Loading / Saving Functions
void paintLoadIndex(void);
void paintSaveIndex(void);
void paintResetStorage(void);
bool paintGetSlotInUse(uint8_t slot);
void paintSetSlotInUse(uint8_t slot);
bool paintGetAnySlotInUse(void);
uint8_t paintGetRecentSlot(void);
void paintSetRecentSlot(uint8_t slot);
void paintSave(uint8_t slot);
void paintLoad(uint8_t slot, uint16_t xTr, uint16_t yTr, uint16_t xScale, uint16_t yScale);

void floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t col);
void _floodFill(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);
void _floodFillInner(display_t* disp, uint16_t x, uint16_t y, paletteColor_t search, paletteColor_t fill);

// PixelStack functions

// Generic util functions

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
        paintDrawScreenMainLoop(elapsedUs);
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
        paintSaveIndex();
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

bool paintGetAnySlotInUse(void)
{
    return (paintState->index & ((1 << PAINT_SAVE_SLOTS) - 1)) != 0;
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

void paintLoad(uint8_t slot, uint16_t xTr, uint16_t yTr, uint16_t xScale, uint16_t yScale)
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
            // calculate the canvas coordinates given the pixel indices
            x0 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) % paintState->canvasW;
            y0 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) / paintState->canvasW;
            x1 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) % paintState->canvasW;
            y1 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) / paintState->canvasW;

            setPxScaled(paintState->disp, x0, y0, paintState->recentColors[imgChunk[n] >> 4], xTr, yTr, xScale, yScale);
            setPxScaled(paintState->disp, x1, y1, paintState->recentColors[imgChunk[n] & 0xF], xTr, yTr, xScale, yScale);
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