#include "paint_ui.h"

#include <malloc.h>

#include "bresenham.h"

#include "paint_common.h"
#include "paint_util.h"
#include "paint_nvs.h"

static const char startMenuSave[] = "Save";
static const char startMenuLoad[] = "Load";
static const char startMenuSlot[] = "Slot %d";
static const char startMenuOverwrite[] = "Overwrite?";
static const char startMenuYes[] = "Yes";
static const char startMenuNo[] = "No";
static const char startMenuClearCanvas[] = "Clear Canvas";
static const char startMenuConfirmUnsaved[] = "Unsaved! OK?";
static const char startMenuExit[] = "Exit";
static const char startMenuEditPalette[] = "Edit Palette";

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
    }


    // Draw border around canvas
    plotRect(canvas->disp, canvas->x - 1, canvas->y - 1, canvas->x + canvas->w * canvas->xScale + 1, canvas->y + canvas->h * canvas->yScale + 1, c000);


    //////// Draw the active FG/BG colors and the color palette


    //////// Active Colors
    // Draw the background color, then draw the foreground color overlapping it and offset by half in both directions
    drawColorBox(canvas->disp, PAINT_ACTIVE_COLOR_X, PAINT_ACTIVE_COLOR_Y, PAINT_COLORBOX_W, PAINT_COLORBOX_H, artist->bgColor, false, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
    drawColorBox(canvas->disp, PAINT_ACTIVE_COLOR_X + PAINT_COLORBOX_W / 2, PAINT_ACTIVE_COLOR_Y + PAINT_COLORBOX_H / 2, PAINT_COLORBOX_W, PAINT_COLORBOX_H, artist->fgColor, false, cTransparent, PAINT_COLORBOX_SHADOW_BOTTOM);

    uint16_t colorBoxX = PAINT_COLORBOX_MARGIN_X + (paintState->canvas.x - 1 - PAINT_COLORBOX_W - PAINT_COLORBOX_MARGIN_X * 2 - 2) / 2;
    uint16_t colorBoxY = PAINT_ACTIVE_COLOR_Y + PAINT_COLORBOX_W + PAINT_COLORBOX_W / 2 + 1 + PAINT_COLORBOX_MARGIN_TOP;

    // vertically center the color boxes in the available space
    colorBoxY = colorBoxY + (canvas->disp->h - PAINT_COLORBOX_MARGIN_TOP - (PAINT_MAX_COLORS * (PAINT_COLORBOX_MARGIN_TOP + PAINT_COLORBOX_H)) - colorBoxY - PAINT_COLORBOX_MARGIN_TOP) / 2;


    //////// Recent Colors (palette)
    for (int i = 0; i < PAINT_MAX_COLORS; i++)
    {
        drawColorBox(canvas->disp, colorBoxX, colorBoxY + i * (PAINT_COLORBOX_MARGIN_TOP + PAINT_COLORBOX_H), PAINT_COLORBOX_W, PAINT_COLORBOX_H, canvas->palette[i], false, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
    }

    if (paintState->buttonMode == BTN_MODE_SELECT || paintState->buttonMode == BTN_MODE_PALETTE)
    {
        // Draw a slightly bigger color box for the selected color
        drawColorBox(canvas->disp, colorBoxX - 3, colorBoxY + paintState->paletteSelect * (PAINT_COLORBOX_MARGIN_TOP + PAINT_COLORBOX_H) - 3, PAINT_COLORBOX_W + 6, PAINT_COLORBOX_H + 6, canvas->palette[paintState->paletteSelect], true, PAINT_COLORBOX_SHADOW_TOP, PAINT_COLORBOX_SHADOW_BOTTOM);
    }


    uint16_t textX = 30, textY = (paintState->canvas.y - 1 - 2 * PAINT_TOOLBAR_TEXT_PADDING_Y) / 2;

    // Up/down arrow logic, for all the options that have text at the top
    if (paintState->saveMenu != HIDDEN && paintState->saveMenu != COLOR_PICKER)
    {
        // Up arrow 1px above center of text
        drawWsg(canvas->disp, &paintState->smallArrowWsg, textX, textY + paintState->saveMenuFont.h / 2 - paintState->smallArrowWsg.h - 1, false, false, 0);

        // Down arrow 1px below center of text
        drawWsg(canvas->disp, &paintState->smallArrowWsg, textX, textY + paintState->saveMenuFont.h / 2 + 1, false, false, 180);

        // Move the text over to accomodate the arrow, plus 4px spacing
        textX += paintState->smallArrowWsg.w + 4;
    }

    if (paintState->saveMenu == HIDDEN)
    {
        //////// Tools

        // Draw the brush icons
        uint16_t iconOffset = 30;
        const brush_t* curBrush;
        for (curBrush = firstBrush; curBrush <= lastBrush; curBrush++)
        {
            const wsg_t* brushIcon = (curBrush == artist->brushDef) ? &curBrush->iconActive : &curBrush->iconInactive;
            uint16_t iconY = (paintState->canvas.y - 1 - brushIcon->h) / 2;

            uint16_t maxIconBottom = 0;
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

        textX = canvas->x;
        textY = canvas->disp->h - paintState->toolbarFont.h - 4;

        // Draw the brush name
        textX = drawText(canvas->disp, &paintState->toolbarFont, c000, artist->brushDef->name, textX, textY);

        if (artist->brushDef->minSize != artist->brushDef->maxSize)
        {
            if (artist->brushWidth == 0)
            {
                snprintf(text, sizeof(text), "Auto");
            }
            else
            {
                snprintf(text, sizeof(text), "%d", artist->brushWidth);
            }

            textX += 4;
            // Draw the icon on the text's baseline
            drawWsg(canvas->disp, &paintState->brushSizeWsg, textX, textY + paintState->toolbarFont.h - paintState->brushSizeWsg.h, false, false, 0);
            textX += paintState->brushSizeWsg.w + 1;
            textX = drawText(canvas->disp, &paintState->toolbarFont, c000, text, textX, textY);
        }

        if (artist->brushDef->mode == PICK_POINT && artist->brushDef->maxPoints > 1)
        {
            // Draw the number of picks made / total
            snprintf(text, sizeof(text), "%zu/%d", pxStackSize(&artist->pickPoints), artist->brushDef->maxPoints);

            textX += 4;
            drawWsg(canvas->disp, &paintState->picksWsg, textX, textY + paintState->toolbarFont.h - paintState->picksWsg.h, false, false, 0);
            textX += paintState->picksWsg.w + 1;
            drawText(paintState->disp, &paintState->toolbarFont, c000, text, textX, textY);
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

            textX += 4;
            drawWsg(canvas->disp, &paintState->picksWsg, textX, textY + paintState->toolbarFont.h - paintState->picksWsg.h, false, false, 0);
            textX += paintState->picksWsg.w + 1;
            drawText(canvas->disp, &paintState->toolbarFont, c000, text, textX, textY);
        }
    }
    else if (paintState->saveMenu == PICK_SLOT_SAVE || paintState->saveMenu == PICK_SLOT_LOAD)
    {
        bool saving = paintState->saveMenu == PICK_SLOT_SAVE;
        // Draw "Save" / "Load"
        drawText(canvas->disp, &paintState->saveMenuFont, c000, saving ? startMenuSave : startMenuLoad, textX, textY);

        const wsg_t* fileIcon = NULL;

        if (saving)
        {
            fileIcon = paintGetSlotInUse(paintState->index, paintState->selectedSlot) ? &paintState->overwriteWsg : &paintState->newfileWsg;
        }

        // Draw the slot number
        char text[16];
        snprintf(text, sizeof(text), startMenuSlot, paintState->selectedSlot + 1);
        uint16_t textW = textWidth(&paintState->saveMenuFont, text);

        // Text goes all the way to the right, minus 13px for the corner, then the arrow, arrow spacing, [icon and spacing,] and the text itself
        textX = canvas->disp->w - 13 - paintState->bigArrowWsg.w - 4 - (fileIcon ? fileIcon->w + 4 : 0) - textW;
        drawText(canvas->disp, &paintState->saveMenuFont, c000, text, textX, textY);

        if (fileIcon)
        {
            drawWsgSimpleFast(canvas->disp, fileIcon, textX + textW + 4, textY);
        }

        // Left arrow
        drawWsg(canvas->disp, &paintState->bigArrowWsg, textX - 4 - paintState->bigArrowWsg.w, textY + (paintState->saveMenuFont.h - paintState->bigArrowWsg.h) / 2, false, false, 270);
        // Right arrow
        drawWsg(canvas->disp, &paintState->bigArrowWsg, textX + textW + 4 + (fileIcon ? fileIcon->w + 4 : 0), textY + (paintState->saveMenuFont.h - paintState->bigArrowWsg.h) / 2, false, false, 90);

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
    else if (paintState->saveMenu == EDIT_PALETTE)
    {
        drawText(canvas->disp, &paintState->saveMenuFont, c000, startMenuEditPalette, textX, textY);
    }
    else if (paintState->saveMenu == COLOR_PICKER)
    {
        paintRenderColorPicker(artist, canvas, paintState);
    }
}

uint16_t paintRenderGradientBox(paintCanvas_t* canvas, char channel, paletteColor_t col, uint16_t x, uint16_t y, uint16_t barW, uint16_t h, bool selected)
{
    uint16_t channelVal;
    switch (channel)
    {
        case 'r': channelVal = col / 36;      break;
        case 'g': channelVal = (col / 6) % 6; break;
        case 'b': channelVal = col % 6;       break;
        default:  channelVal = 0;             break;
    }

    // draw the color bar... under the text box?
    for (uint8_t i = 0; i < 6; i++)
    {
        uint16_t r = channel == 'r' ? i : (col / 36);
        uint16_t g = channel == 'g' ? i : (col / 6) % 6;
        uint16_t b = channel == 'b' ? i : (col % 6);

        fillDisplayArea(canvas->disp, x + i * barW, y, x + i * barW + barW, y + h + 1, r * 36 + g * 6 + b);
    }

    // Draw a bigger box for the active color in this segment
    fillDisplayArea(canvas->disp, x + channelVal * barW - 1, y - 1, x + channelVal * barW + barW + 1, y + h + 2, col);

    // Border around selected segment, ~if this channel is selected~
    if (selected)
    {
        // Inner border
        plotRect(canvas->disp, x + channelVal * barW - 2, y - 2, x + channelVal * barW + barW + 2, y + h + 3, c000);

        // Top
        plotLine(canvas->disp, x + channelVal * barW - 2, y - 3, x + channelVal * barW + barW + 1, y - 3, c555, 0);
        // Left
        plotLine(canvas->disp, x + channelVal * barW - 3, y - 2, x + channelVal * barW - 3, y + h + 2, c555, 0);
        // Right
        plotLine(canvas->disp, x + channelVal * barW + barW + 2, y - 2, x + channelVal * barW + barW + 2, y + h + 2, c555, 0);
        // Bottom
        plotLine(canvas->disp, x + channelVal * barW - 2, y + h + 3, x + channelVal * barW + barW + 1, y + h + 3, c555, 0);
    }

    // return total width of box
    return 6 * barW + 2;
}

void paintRenderColorPicker(paintArtist_t* artist, paintCanvas_t* canvas, paintDraw_t* paintState)
{
    bool rCur = false, bCur = false, gCur = false;

    if (paintState->editPaletteCur == &paintState->editPaletteR)
    {
        // R selected
        rCur = true;
    }
    else if (paintState->editPaletteCur == &paintState->editPaletteG)
    {
        // G selected
        gCur = true;
    }
    else
    {
        // B selected
        bCur = true;
    }

    // Draw 3 color gradient bars, each showing what the color would be if it were changed
    uint16_t barOffset = canvas->x, barMargin = 4;
    uint16_t barY = paintState->smallFont.h + 2 + 2;
    uint16_t barH = canvas->y - barY - 2 - 2 - 1;

    uint16_t textW = textWidth(&paintState->smallFont, "Red");
    uint16_t barWidth = paintRenderGradientBox(canvas, 'r', paintState->newColor, barOffset, barY, PAINT_COLOR_PICKER_BAR_W, barH, rCur);
    drawText(canvas->disp, &paintState->smallFont, c000, "Red", barOffset + (barWidth - textW) / 2, 1);
    barOffset += barWidth + barMargin;

    textW = textWidth(&paintState->smallFont, "Green");
    barWidth = paintRenderGradientBox(canvas, 'g', paintState->newColor, barOffset, barY, PAINT_COLOR_PICKER_BAR_W, barH, gCur);
    drawText(canvas->disp, &paintState->smallFont, c000, "Green", barOffset + (barWidth - textW) / 2, 1);
    barOffset += barWidth + barMargin;

    textW = textWidth(&paintState->smallFont, "Blue");
    barWidth = paintRenderGradientBox(canvas, 'b', paintState->newColor, barOffset, barY, PAINT_COLOR_PICKER_BAR_W, barH, bCur);
    drawText(canvas->disp, &paintState->smallFont, c000, "Blue", barOffset + (barWidth - textW) / 2, 1);
    barOffset += barWidth + barMargin;

    char hexCode[16];
    snprintf(hexCode, sizeof(hexCode), "#%02X%02X%02X", paintState->editPaletteR * 51, paintState->editPaletteG * 51, paintState->editPaletteB * 51);

    textW = textWidth(&paintState->toolbarFont, hexCode);

    uint16_t hexW = canvas->x + canvas->w * canvas->xScale - barOffset;
    // Make sure the color box is wide enough for the hex text
    if (hexW < textW + 4)
    {
        hexW = textW + 4;
    }

    // Draw a color box the same height as the gradient bars, extendng at least to the end of the canva
    drawColorBox(canvas->disp, barOffset, barY, hexW, barH, paintState->newColor, false, c000, c000);

    // Draw the hex code for the color centered (vertically + horizontally) in the box
    drawText(canvas->disp, &paintState->toolbarFont, getContrastingColorBW(paintState->newColor), hexCode, barOffset + (hexW - textW) / 2, barY + (barH - paintState->toolbarFont.h) / 2);
}

void paintClearCanvas(const paintCanvas_t* canvas, paletteColor_t bgColor)
{
    fillDisplayArea(canvas->disp, canvas->x, canvas->y, canvas->x + canvas->w * canvas->xScale, canvas->y + canvas->h * canvas->yScale, bgColor);
}

// Generates a cursor sprite that's a box
void paintGenerateCursorSprite(wsg_t* cursorWsg, const paintCanvas_t* canvas)
{
    cursorWsg->w = canvas->xScale + 2;
    cursorWsg->h = canvas->yScale + 2;
    cursorWsg->px = malloc(sizeof(paletteColor_t) * cursorWsg->w * cursorWsg->h);

    paletteColor_t pxVal;
    for (uint16_t x = 0; x < cursorWsg->w; x++)
    {
        for (uint16_t y = 0; y < cursorWsg->h; y++)
        {
            if (x == 0 || x == cursorWsg->w - 1 || y == 0 || y == cursorWsg->h - 1)
            {
                pxVal = c000;
            }
            else
            {
                pxVal = cTransparent;
            }
            cursorWsg->px[y * cursorWsg->w + x] = pxVal;
        }
    }
}

void paintFreeCursorSprite(wsg_t* cursorWsg)
{
    if (cursorWsg->px != NULL)
    {
        free(cursorWsg->px);
        cursorWsg->px = NULL;
        cursorWsg->w = 0;
        cursorWsg->h = 0;
    }
}

void initCursor(paintCursor_t* cursor, paintCanvas_t* canvas, const wsg_t* sprite)
{
    cursor->sprite = sprite;

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
    cursor->redraw = true;

    drawCursor(cursor, canvas);
}

void setCursorOffset(paintCursor_t* cursor, int16_t x, int16_t y)
{
    cursor->spriteOffsetX = x;
    cursor->spriteOffsetY = y;
    cursor->redraw = true;
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
    bool cursorIsNearEdge = (canvasToDispX(canvas, cursor->x) + cursor->spriteOffsetX < canvas->x || canvasToDispX(canvas, cursor->x) + cursor->spriteOffsetX + cursor->sprite->w > canvas->x + canvas->w * canvas->xScale || canvasToDispY(canvas, cursor->y) + cursor->spriteOffsetY < canvas->y || canvasToDispY(canvas, cursor->y) + cursor->spriteOffsetY + cursor->sprite->h > canvas->y + canvas->h * canvas->yScale);
    if (cursor->show && (cursor->redraw || cursorIsNearEdge))
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

void moveCursorAbsolute(paintCursor_t* cursor, paintCanvas_t* canvas, uint16_t x, uint16_t y)
{
    if (x < canvas->w && y < canvas->h) {
        cursor->redraw = true;
        cursor->x = x;
        cursor->y = y;
    }
}
