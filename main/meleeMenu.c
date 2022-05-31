//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "bresenham.h"
#include "meleeMenu.h"

//==============================================================================
// Constant Data
//==============================================================================

// Colors for the border when each row is selected
static const paletteColor_t borderColors[MAX_ROWS] =
{
    c112, c211, c021, c221, c102
};

// X axis offset for each row
static const uint8_t rowOffsets[MAX_ROWS] =
{
    70, 45, 20, 39, 29
};

//==============================================================================
// Function Prototypes
//==============================================================================

static void drawMeleeMenuText(display_t* d, font_t* font, const char* text,
                              int16_t xPos, int16_t yPos, bool isSelected);

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize and return a melee menu
 *
 * @param title  The title to be displayed for this menu. The underlying memory
 *               isn't copied, so this string must persist for the lifetime of
 *               the menu.
 * @param font   The font to draw this menu with (should be mm.font)
 * @param cbFunc The function to call when a menu option is selected. The
 *               argument to the callback will be the same pointer
 * @return meleeMenu_t* A pointer to the newly allocated meleeMenu_t. This must
 *                      be deinitialized when the menu is not used anymore.
 */
meleeMenu_t* initMeleeMenu(const char* title, font_t* font, meleeMenuCb cbFunc)
{
    // Allocate a menu
    meleeMenu_t* newMenu = malloc(sizeof(meleeMenu_t));
    // Clear the memory
    memset(newMenu, 0, sizeof(meleeMenu_t));
    // Save the arguments
    newMenu->title = title;
    newMenu->cbFunc = cbFunc;
    newMenu->font = font;
    // Return the menu
    return newMenu;
}

/**
 * Deinitialize a melee menu
 *
 * @param menu A pointer to the meleeMenu_t to deinitialize.
 */
void deinitMeleeMenu(meleeMenu_t* menu)
{
    free(menu);
}

/**
 * Add a row to a melee menu. This can only add up to MAX_ROWS rows.
 *
 * @param menu  The menu to add a row to
 * @param label The label for this row. The underlying memory isn't copied, so
 *              this string must persist for the lifetime of the menu.
 */
void addRowToMeleeMenu(meleeMenu_t* menu, const char* label)
{
    // Make sure there's space for this row
    if(menu->numRows < MAX_ROWS)
    {
        // Add the row
        menu->rows[menu->numRows] = label;
        menu->numRows++;
    }
}

/**
 * Process button events for the given menu
 *
 * @param menu The menu to process button events for
 * @param btn The button that was pressed
 */
void meleeMenuButton(meleeMenu_t* menu, buttonBit_t btn)
{
    switch(btn)
    {
        case UP:
        {
            // Scroll up, with wraparound
            if(0 == menu->selectedRow)
            {
                menu->selectedRow = menu->numRows - 1;
            }
            else
            {
                menu->selectedRow--;
            }
            break;
        }
        case DOWN:
        {
            // Scroll down, with wraparound
            if(menu->selectedRow == menu->numRows - 1)
            {
                menu->selectedRow = 0;
            }
            else
            {
                menu->selectedRow++;
            }
            break;
        }
        case BTN_A:
        {
            // Call the callback function for the given row
            menu->cbFunc(menu->rows[menu->selectedRow]);
            break;
        }
        default:
        {
            // do nothing
            break;
        }
    }
}

/**
 * Draw a melee menu to a display. This overwrites the entire framebuffer.
 *
 * @param d    The display to draw to
 * @param menu The menu to draw
 */
void drawMeleeMenu(display_t* d, meleeMenu_t* menu)
{
    // Draw a dim blue background with a grey grid
    for(int16_t y = 0; y < d->h; y++)
    {
        for(int16_t x = 0; x < d->w; x++)
        {
            if(((x % 12) == 0) || ((y % 12) == 0))
            {
                d->setPx(x, y, c111); // Grid
            }
            else
            {
                d->setPx(x, y, c001); // Background
            }
        }
    }

    // Draw the title and note where it ends
    int16_t textEnd = drawText(d, menu->font, c222, menu->title, 33, 25);

    // The width of the border
#define BORDER_WIDTH 7
    // The gap between display edge and border
#define BORDER_GAP  24

    // Draw a border, on the right
    fillDisplayArea(d,
                    BORDER_GAP,                BORDER_GAP + menu->font->h + 3,
                    BORDER_GAP + BORDER_WIDTH, d->h - BORDER_GAP,
                    borderColors[menu->selectedRow]);
    // Then the left
    fillDisplayArea(d,
                    d->w - BORDER_GAP - BORDER_WIDTH, BORDER_GAP,
                    d->w - BORDER_GAP,                d->h - BORDER_GAP,
                    borderColors[menu->selectedRow]);
    // At the bottom
    fillDisplayArea(d,
                    BORDER_GAP,        d->h - BORDER_GAP - BORDER_WIDTH,
                    d->w - BORDER_GAP, d->h - BORDER_GAP,
                    borderColors[menu->selectedRow]);
    // Right of title
    fillDisplayArea(d,
                    textEnd + 1,                BORDER_GAP,
                    textEnd + 1 + BORDER_WIDTH, BORDER_GAP + menu->font->h + 3,
                    borderColors[menu->selectedRow]);
    // Below title
    fillDisplayArea(d,
                    BORDER_GAP,                 BORDER_GAP + menu->font->h + 3,
                    textEnd + 1 + BORDER_WIDTH, BORDER_GAP + menu->font->h + 3 + BORDER_WIDTH,
                    borderColors[menu->selectedRow]);
    // Top right of the title
    fillDisplayArea(d,
                    textEnd + 1,       BORDER_GAP,
                    d->w - BORDER_GAP, BORDER_GAP + BORDER_WIDTH,
                    borderColors[menu->selectedRow]);

    // Draw the entries
    int16_t yIdx = 37;
    for(uint8_t row = 0; row < menu->numRows; row++)
    {
        drawMeleeMenuText(d, menu->font, menu->rows[row],
                          rowOffsets[row], (yIdx += (menu->font->h + 7)),
                          (row == menu->selectedRow));
    }
}

/**
 * Draw text with a boundary and filled background to a Melee style menu
 *
 * @param d    The display to draw to
 * @param font The font to use
 * @param text The text for this box
 * @param xPos The X position of the text. Note, this is not the position of the
 *             boundary and filled background
 * @param yPos The Y position of the text. Note, this is not the position of the
 *             boundary and filled background
 * @param isSelected true if this is the selected item. This will draw with
 *                   different colors
 */
static void drawMeleeMenuText(display_t* d, font_t* font, const char* text,
                              int16_t xPos, int16_t yPos, bool isSelected)
{
    // Boundary color is the same for all entries
    paletteColor_t boundaryColor = c321;

    // Figure out the text width to draw around it
    int16_t tWidth = textWidth(font, text);

    // Top line
    plotLine(d,
             xPos - 3,          yPos - 3,
             xPos + tWidth + 1, yPos - 3,
             boundaryColor);
    // Bottom line
    plotLine(d,
             xPos - 8,          yPos + font->h + 2,
             xPos + tWidth + 1, yPos + font->h + 2,
             boundaryColor);
    // Left side doodad
    plotLine(d,
             xPos -  3, yPos -  3,
             xPos - 13, yPos + 14,
             boundaryColor);
    plotLine(d,
             xPos - 13, yPos + 15,
             xPos -  8, yPos + font->h + 2,
             boundaryColor);
    // Right side semi-circle
    int16_t radius = (font->h + 6) / 2;
    plotCircleQuadrants(d,
                        xPos + tWidth, yPos - 3 + radius, radius,
                        true, false, false, true,
                        boundaryColor);

    // Text and fill colors are different if selected
    paletteColor_t textColor = c431;
    paletteColor_t fillColor = c000;
    if(isSelected)
    {
        textColor = c000;
        fillColor = c540;
    }

    // Fill the label shape
    oddEvenFill(d,
                xPos - 13, yPos - 2,
                xPos + tWidth + radius + 1, yPos + font->h + 2,
                boundaryColor, fillColor);

    // Draw the text
    drawText(d, font, textColor, text, xPos, yPos);
}
