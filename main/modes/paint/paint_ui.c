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
static const char startMenuClearCanvas[] = "Clear Canvas";
static const char startMenuConfirmUnsaved[] = "Unsaved! OK?";
static const char startMenuExit[] = "Exit";

void drawColorBox(display_t* disp, uint16_t xOffset, uint16_t yOffset, uint16_t w, uint16_t h, paletteColor_t col, bool selected, paletteColor_t topBorder, paletteColor_t bottomBorder)
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
        fillDisplayArea(disp, xOffset, yOffset, xOffset + w / 2, yOffset + h / 2, c111);
        fillDisplayArea(disp, xOffset + w / 2, yOffset, xOffset + w, yOffset + h / 2, c555);
        fillDisplayArea(disp, xOffset, yOffset + h / 2, xOffset + w / 2, yOffset + h, c555);
        fillDisplayArea(disp, xOffset + w / 2, yOffset + h / 2, xOffset + w, yOffset + h, c111);
    }
    else
    {
        fillDisplayArea(disp, xOffset, yOffset, xOffset + w, yOffset + h, col);
    }

    if (topBorder != cTransparent)
    {
        // Top border
        plotLine(disp, xOffset - 1,  yOffset, xOffset + w - 1, yOffset, topBorder, dashLen);
        // Left border
        plotLine(disp, xOffset - 1, yOffset, xOffset - 1, yOffset + h - 1, topBorder, dashLen);
    }

    if (bottomBorder != cTransparent)
    {
        // Bottom border
        plotLine(disp, xOffset, yOffset + h, xOffset + w - 1, yOffset + h, bottomBorder, dashLen);
        // Right border
        plotLine(disp, xOffset + w, yOffset + 1, xOffset + w, yOffset + h - 1, bottomBorder, dashLen);
    }
}

void paintRenderToolbar(paintArtist_t* artist, paintCanvas_t* canvas, paintDraw_t* paintState, const brush_t* firstBrush, const brush_t* lastBrush)
{
    //////// Background

    // Fill top bar
    fillDisplayArea(canvas->disp, 0, 0, canvas->disp->w, canvas->y, PAINT_TOOLBAR_BG);

    // Fill left side bar
    fillDisplayArea(canvas->disp, 0, 0, canvas->x, canvas->disp->h, PAINT_TOOLBAR_BG);

    // Fill right bar, if there's room
    if (canvas->x + canvas->w * canvas->xScale < canvas->disp->w)
    {
        fillDisplayArea(canvas->disp, canvas->x + canvas->w * canvas->xScale, 0, canvas->disp->w, canvas->disp->h, PAINT_TOOLBAR_BG);
    }

    // Fill bottom bar, if there's room
    if (canvas->y + canvas->h * canvas->yScale < canvas->disp->h)
    {
        fillDisplayArea(canvas->disp, 0, canvas->y + canvas->h * canvas->yScale, canvas->disp->w, canvas->disp->h, PAINT_TOOLBAR_BG);


        // Draw a black rectangle under where the exit progress bar will be so it can be seen
        fillDisplayArea(canvas->disp, 0, canvas->disp->h - 11, canvas->disp->w, canvas->disp->h, c000);
    }


    // Draw border around canvas
    plotRect(canvas->disp, canvas->x - 1, canvas->y - 1, canvas->x + canvas->w * canvas->xScale + 1, canvas->y + canvas->h * canvas->yScale + 1, c000);


    //////// Draw the active FG/BG colors and the color palette


    //////// Active Colors
    // Draw the background color, then draw the foreground color overlapping it and offset by half in both directions
    drawColorBox(canvas->disp, PAINT_ACTIVE_COLOR_X, PAINT_ACTIVE_COLOR_Y, PAINT_COLORBOX_W, PAINT_COLORBOX_H, artist->bgColor, false, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
    drawColorBox(canvas->disp, PAINT_ACTIVE_COLOR_X + PAINT_COLORBOX_W / 2, PAINT_ACTIVE_COLOR_Y + PAINT_COLORBOX_H / 2, PAINT_COLORBOX_W, PAINT_COLORBOX_H, artist->fgColor, false, cTransparent, PAINT_COLORBOX_SHADOW_BOTTOM);

    uint16_t colorBoxX = PAINT_COLORBOX_MARGIN_X + (paintState->canvas.x - 1 - PAINT_COLORBOX_W - PAINT_COLORBOX_MARGIN_X * 2 - 2) / 2;


    //////// Recent Colors (palette)
    for (int i = 0; i < PAINT_MAX_COLORS; i++)
    {
        drawColorBox(canvas->disp, colorBoxX, PAINT_COLORBOX_Y + i * (PAINT_COLORBOX_MARGIN_TOP + PAINT_COLORBOX_H), PAINT_COLORBOX_W, PAINT_COLORBOX_H, canvas->palette[i], paintState->buttonMode == BTN_MODE_SELECT && paintState->paletteSelect == i, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
    }


    uint16_t textX = 30, textY = (paintState->canvas.y - 1 - 2 * PAINT_TOOLBAR_TEXT_PADDING_Y) / 2;
    uint16_t maxIconBottom = 0;

    if (paintState->saveMenu == HIDDEN)
    {
        //////// Tools

        // Draw the brush icons
        uint16_t iconOffset = 30;
        brush_t* curBrush;
        for (curBrush = firstBrush; curBrush <= lastBrush; curBrush++)
        {
            wsg_t* brushIcon = (curBrush == artist->brushDef) ? &curBrush->iconActive : &curBrush->iconInactive;
            uint16_t iconY = (paintState->canvas.y - 1 - brushIcon->h) / 2;

            if (iconY + brushIcon->h > maxIconBottom)
            {
                maxIconBottom = iconY + brushIcon->h;
            }

            // if this is the active brush, draw the FG color underneath it first
            if (curBrush == artist->brushDef)
            {
                fillDisplayArea(canvas->disp, iconOffset, iconY, iconOffset + brushIcon->w, iconY + brushIcon->h, (paintState->buttonMode == BTN_MODE_SELECT) ? canvas->palette[paintState->paletteSelect] : artist->fgColor);
            }

            drawWsg(canvas->disp, brushIcon, iconOffset, iconY, false, false, 0);

            iconOffset += brushIcon->w + 1;
        }

        // Draw the brush size, if applicable and not constant
        char text[16];
        uint16_t textW;
        textY = maxIconBottom + TOOL_INFO_TEXT_MARGIN_Y;
        if (artist->brushDef->minSize > 0 && artist->brushDef->maxSize > 0 && artist->brushDef->minSize != artist->brushDef->maxSize)
        {
            snprintf(text, sizeof(text), "%d", artist->brushWidth);
            textW = textWidth(&paintState->toolbarFont, text);
            drawText(canvas->disp, &paintState->toolbarFont, c000, text, canvas->x + canvas->w * canvas->xScale + 2 + (canvas->disp->w - canvas->x - canvas->w * canvas->xScale - 2 - textW) / 2, textY);
        }

        textY += paintState->toolbarFont.h + TOOL_INFO_TEXT_MARGIN_Y;
        if (artist->brushDef->mode == PICK_POINT && artist->brushDef->maxPoints > 1)
        {
            // Draw the number of picks made / total
            snprintf(text, sizeof(text), "%zu/%d", pxStackSize(&artist->pickPoints), artist->brushDef->maxPoints);
            textW = textWidth(&paintState->toolbarFont, text);
            drawText(paintState->disp, &paintState->toolbarFont, c000, text, canvas->x + canvas->w * canvas->xScale + 2 + (canvas->disp->w - canvas->x - canvas->w * canvas->xScale - 2 - textW) / 2, textY);
        }
        else if (artist->brushDef->mode == PICK_POINT_LOOP && artist->brushDef->maxPoints > 1)
        {
            // Draw the number of remaining picks
            uint8_t maxPicks = artist->brushDef->maxPoints;

            if (pxStackSize(&artist->pickPoints) + 1 == maxPicks - 1)
            {
                snprintf(text, sizeof(text), "Last");
            }
            else
            {
                snprintf(text, sizeof(text), "%zu", maxPicks - pxStackSize(&artist->pickPoints) - 1);
            }

            textW = textWidth(&paintState->toolbarFont, text);
            drawText(canvas->disp, &paintState->toolbarFont, c000, text, canvas->x + canvas->w * canvas->xScale + 2 + (canvas->disp->w - canvas->x - canvas->w * canvas->xScale - 2 - textW) / 2, textY);
        }
    }
    else if (paintState->saveMenu == PICK_SLOT_SAVE || paintState->saveMenu == PICK_SLOT_LOAD)
    {
        bool saving = paintState->saveMenu == PICK_SLOT_SAVE;
        // Draw "Save" / "Load"
        drawText(canvas->disp, &paintState->saveMenuFont, c000, saving ? startMenuSave : startMenuLoad, textX, textY);

        // Draw the slot number
        char text[16];
        snprintf(text, sizeof(text), (saving && paintGetSlotInUse(paintState->index, paintState->selectedSlot)) ? startMenuSlotUsed : startMenuSlot, paintState->selectedSlot + 1);
        drawText(canvas->disp, &paintState->saveMenuFont, c000, text, 160, textY);
    }
    else if (paintState->saveMenu == CONFIRM_OVERWRITE)
    {
        // Draw "Overwrite?"
        drawText(canvas->disp, &paintState->saveMenuFont, c000, startMenuOverwrite, textX, textY);

        // Draw "Yes" / "No"
        drawText(canvas->disp, &paintState->saveMenuFont, c000, paintState->saveMenuBoolOption ? startMenuYes : startMenuNo, 160, textY);
    }
    else if (paintState->saveMenu == CLEAR)
    {
        drawText(canvas->disp, &paintState->saveMenuFont, c000, startMenuClearCanvas, textX, textY);
    }
    else if (paintState->saveMenu == CONFIRM_CLEAR || paintState->saveMenu == CONFIRM_EXIT || paintState->saveMenu == CONFIRM_UNSAVED)
    {
        // Draw "Unsaved! OK?"
        drawText(canvas->disp, &paintState->saveMenuFont, c000, startMenuConfirmUnsaved, textX, textY);

        // Draw "Yes" / "No"
        drawText(canvas->disp, &paintState->saveMenuFont, c000, paintState->saveMenuBoolOption ? startMenuYes : startMenuNo, 180, textY);
    }
    else if (paintState->saveMenu == EXIT)
    {
        drawText(canvas->disp, &paintState->saveMenuFont, c000, startMenuExit, textX, textY);
    }
}

void paintClearCanvas(const paintCanvas_t* canvas, paletteColor_t bgColor)
{
    fillDisplayArea(canvas->disp, canvas->x, canvas->y, canvas->x + canvas->w * canvas->xScale, canvas->y + canvas->h * canvas->yScale, bgColor);
}

void initCursor(paintCursor_t* cursor, paintCanvas_t* canvas, const wsg_t* sprite)
{
    cursor->sprite = sprite;
    cursor->spriteOffsetX = canvas->xScale / 2 - sprite->w / 2;
    cursor->spriteOffsetY = canvas->yScale / 2 - sprite->h / 2;

    cursor->show = false;
    cursor->x = 0;
    cursor->y = 0;

    cursor->redraw = true;

    initPxStack(&cursor->underPxs);
}

void deinitCursor(paintCursor_t* cursor)
{
    freePxStack(&cursor->underPxs);
}

void setCursorSprite(paintCursor_t* cursor, paintCanvas_t* canvas, const wsg_t* sprite)
{
    undrawCursor(cursor, canvas);

    cursor->sprite = sprite;
    cursor->spriteOffsetX = canvas->xScale / 2 - sprite->w / 2;
    cursor->spriteOffsetY = canvas->yScale / 2 - sprite->h / 2;

    cursor->redraw = true;

    drawCursor(cursor, canvas);
}

/// @brief Undraws the cursor and removes its pixels from the stack
/// @param cursor The cursor to hide
/// @param canvas The canvas to hide the cursor pixels from
void undrawCursor(paintCursor_t* cursor, paintCanvas_t* canvas)
{
    while (popPx(&cursor->underPxs, canvas->disp));

    cursor->redraw = true;
}

/// @brief Hides the cursor without removing its stored pixels from the stack
/// @param cursor The cursor to hide
/// @param canvas The canvas to hide the cursor pixels from
void hideCursor(paintCursor_t* cursor, paintCanvas_t* canvas)
{
    if (cursor->show)
    {
        undrawCursor(cursor, canvas);

        cursor->show = false;
        cursor->redraw = true;
    }
}

/// @brief Shows the cursor without saving the pixels under it
/// @param cursor The cursor to show
/// @param canvas The canvas to draw the cursor on
void showCursor(paintCursor_t* cursor, paintCanvas_t* canvas)
{
    if (!cursor->show)
    {
        cursor->show = true;
        cursor->redraw = true;
        drawCursor(cursor, canvas);
    }
}

/// @brief If not hidden, draws the cursor on the canvas and saves the pixels for later. If hidden, does nothing.
/// @param cursor The cursor to draw
/// @param canvas The canvas to draw it on and save the pixels from
void drawCursor(paintCursor_t* cursor, paintCanvas_t* canvas)
{
    if (cursor->show && cursor->redraw)
    {
        // Undraw the previous cursor pixels, if there are any
        undrawCursor(cursor, canvas);
        paintDrawWsgTemp(canvas->disp, cursor->sprite, &cursor->underPxs, canvasToDispX(canvas, cursor->x) + cursor->spriteOffsetX, canvasToDispY(canvas, cursor->y) + cursor->spriteOffsetY, getContrastingColor);
        cursor->redraw = false;
    }
}

/// @brief Moves the cursor by the given relative x and y offsets, staying within the canvas bounds. Does not draw.
/// @param cursor The cursor to be moved
/// @param canvas The canvas for the cursor bounds
/// @param xDiff The relative X offset to move the cursor
/// @param yDiff The relative Y offset to move the cursor
void moveCursorRelative(paintCursor_t* cursor, paintCanvas_t* canvas, int16_t xDiff, int16_t yDiff)
{
    int16_t newX, newY;

    newX = cursor->x + xDiff;
    newY = cursor->y + yDiff;

    if (newX >= canvas->w)
    {
        newX = canvas->w - 1;
    }
    else if (newX < 0)
    {
        newX = 0;
    }

    if (newY >= canvas->h)
    {
        newY = canvas->h - 1;
    } else if (newY < 0)
    {
        newY = 0;
    }

    // Only update the position if it would be different from the current position.
    // TODO: Does this actually matter?
    if (newX != cursor->x || newY != cursor->y)
    {
        cursor->redraw = true;
        cursor->x = newX;
        cursor->y = newY;
    }
}