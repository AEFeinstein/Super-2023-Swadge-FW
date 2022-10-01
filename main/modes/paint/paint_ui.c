#include "paint_ui.h"

#include "bresenham.h"

#include "paint_common.h"
#include "paint_util.h"
#include "paint_nvs.h"

static const char startMenuSave[] = "Save";
static const char startMenuLoad[] = "Load";
static const char startMenuSlot[] = "Slot %d";
static const char startMenuSlotUsed[] = "Slot %d (!)";
static const char startMenuOverwrite[] = "Overwrite?";
static const char startMenuYes[] = "Yes";
static const char startMenuNo[] = "No";

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

void paintRenderAll(void)
{
    paintRenderToolbar();
    paintRenderCursor();
}

void paintClearCanvas(void)
{
    fillDisplayArea(paintState->disp, PAINT_CANVAS_X_OFFSET, PAINT_CANVAS_Y_OFFSET, PAINT_CANVAS_X_OFFSET + paintState->canvasW * PAINT_CANVAS_SCALE, PAINT_CANVAS_Y_OFFSET + paintState->canvasH * PAINT_CANVAS_SCALE, paintState->bgColor);
}
