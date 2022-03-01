/*
 * menu2d.c
 *
 *  Created on: Jul 12, 2020
 *      Author: adam
 */

//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include "esp_timer.h"

#include "btn.h"
#include "menu2d.h"
#include "bresenham.h"

//==============================================================================
// Defines
//==============================================================================

#define ROW_SPACING   3
#define ITEM_SPACING 10

#define BLANK_SPACE_Y  37
#define SELECTED_ROW_Y 46

#define US_PER_PIXEL_X  5000
#define US_PER_PIXEL_Y 10000

//==============================================================================
// Prototypes
//==============================================================================

linkedInfo_t* linkNewNode(cLinkedNode_t** root, uint8_t len, linkedInfo_t info);
void drawRow(menu_t * menu, cLinkedNode_t* row, int16_t yPos, bool shouldDrawBox, uint64_t tElapsedUs);

//==============================================================================
// Functions
//==============================================================================

/**
 * Allocate memory for and initialize a menu struct
 *
 * @param title  The title for this menu. This must be a pointer to static
 *               memory. New memory is NOT allocated for this string
 * @param cbFunc The callback function for when an item is selected
 * @return A pointer to malloc'd memory for this menu
 */
menu_t* initMenu(display_t * disp, const char* title, font_t * titleFont,
                 font_t * menuFont, rgba_pixel_t foregroundColor,
                 rgba_pixel_t backgroundColor, menuCb cbFunc)
{
    // Allocate the memory
    menu_t* menu = (menu_t*)malloc(sizeof(menu_t));

    // Initialize all values
    menu->title = title;
    menu->rows = NULL;
    menu->numRows = 0;
    menu->cbFunc = cbFunc;
    menu->yOffset = 0;
    menu->tLastCallUs = esp_timer_get_time();
    menu->tAccumulatedUs = 0;
    menu->disp = disp;
    menu->tFont = titleFont;
    menu->mFont = menuFont;
    menu->fColor = foregroundColor;
    menu->bColor = backgroundColor;

    // Return the pointer
    return menu;
}

/**
 * Free all memory allocated for a menu struct, including memory
 * for the rows and items
 *
 * @param menu The menu to to free
 */
void deinitMenu(menu_t* menu)
{
    // For each row
    while(menu->numRows--)
    {
        // For each item in the row
        while(menu->rows->d.row.numItems--)
        {
            // Free the item, iterate to the next item
            cLinkedNode_t* next = menu->rows->d.row.items->next;
            free(menu->rows->d.row.items);
            menu->rows->d.row.items = next;
        }
        // Then free the row and iterate to the next
        cLinkedNode_t* next = menu->rows->next;
        free(menu->rows);
        menu->rows = next;
    }
    // Finally free the whole menu
    free(menu);
}

/**
 * Helper function to link a new node in the circular linked list. This can
 * either be a new row in the menu or a new entry in a row.
 *
 * @param root A pointer to a cLinkedNode_t pointer to link the new node to
 * @param len  The number of items in root
 * @param info The info to link in the list
 * @return The newly linked node
 */
linkedInfo_t* linkNewNode(cLinkedNode_t** root, uint8_t len, linkedInfo_t info)
{
    // If root points to a null pointer
    if(NULL == (*root))
    {
        // malloc a new node
        (*root) = (cLinkedNode_t*)malloc(sizeof(cLinkedNode_t));

        // Link it up
        (*root)->next = (*root);
        (*root)->prev = (*root);

        // And store the information
        (*root)->d = info;

        // Return a pointer to the new node
        return &((*root)->d);
    }
    else
    {
        // If root doesn't point to a null pointer, iterate to the end of the rows
        cLinkedNode_t* row = (*root);
        while(--len)
        {
            row = row->next;
        }

        // Save what will be the next and prev rows
        cLinkedNode_t* newNext = row->next;
        cLinkedNode_t* newPrev = row;

        // Allocate the new row, and move to it
        row->next = (cLinkedNode_t*)malloc(sizeof(cLinkedNode_t));
        row = row->next;

        // Link everything together
        row->prev = newPrev;
        row->next = newNext;
        newPrev->next = row;
        newNext->prev = row;

        // And store the information
        row->d = info;

        // Return a pointer to the new node
        return &(row->d);
    }
}

/**
 * Add a row to the menu
 *
 * @param menu The menu to add a row to
 */
void addRowToMenu(menu_t* menu)
{
    // Make a new row
    linkedInfo_t newRow;
    newRow.row.items = NULL;
    newRow.row.numItems = 0;
    newRow.row.xOffset = 0;
    newRow.row.tAccumulatedUs = 0;

    // Link the new row
    linkNewNode(&(menu->rows), menu->numRows, newRow);
    menu->numRows++;
}

/**
 * Add a new item to the row which was last added to the menu
 *
 * @param menu The menu to add an item to
 * @param name The name of this item. This must be a pointer to static memory.
 *             New memory is NOT allocated for this string. This pointer will
 *             be returned via the callback when the item is selected
 * @return A pointer to the newly created item
 */
linkedInfo_t* addItemToRow(menu_t* menu, const char* name)
{
    // Iterate to the end of the rows
    cLinkedNode_t* row = menu->rows;
    uint8_t len = menu->numRows;
    while(--len)
    {
        row = row->next;
    }

    // Make a new item
    linkedInfo_t newItem;
    newItem.item.name = name;

    // Link the new item and return a pointer to it
    linkedInfo_t* linkedItem = linkNewNode(&(row->d.row.items), row->d.row.numItems, newItem);
    row->d.row.numItems++;
    return linkedItem;
}

/**
 * Remove an item from the menu. This will iterate through all rows and all
 * items and remove the first item whose name matches the given pointer
 *
 * @param menu The menu to remove an item from
 * @param name A pointer to the name of an item to remove. Must match the
 *             pointer used to add the item
 */
void removeItemFromMenu(menu_t* menu, const char* name)
{
    cLinkedNode_t* row = menu->rows;
    for(int rowIdx = 0; rowIdx < menu->numRows; rowIdx++)
    {
        rowInfo_t * rowInfo = &(row->d.row);
        cLinkedNode_t* item = rowInfo->items;
        for(uint8_t itemIdx = 0; itemIdx < rowInfo->numItems; itemIdx++)
        {
            itemInfo_t * itemInfo = &(item->d.item);
            // Comparing pointers, not strings
            if(itemInfo->name == name)
            {
                // Relink the previous node
                item->next->prev = item->prev;
                // Relink the next node
                item->prev->next = item->next;

                // Decrement the item count
                rowInfo->numItems--;
                // If this row started with this item
                if(rowInfo->items == item)
                {
                    // Point it to the next one instead
                    rowInfo->items = item->next;
                }

                // Free this node
                free(item);

                // Return now that an item was removed
                return;
            }
            item = item->next;
        }
        row = row->next;
    }
}

/**
 * Draw a single row of the menu to the OLED
 *
 * @param row The row to draw
 * @param yPos The Y position of the row to draw
 * @param shouldDrawBox True if this is the selected row, false otherwise
 * @param tElapsedUs The time elapsed since this row was last drawn
 */
void drawRow(menu_t * menu, cLinkedNode_t* row, int16_t yPos, bool shouldDrawBox, uint64_t tElapsedUs)
{
    // Get a pointer to the items for this row
    cLinkedNode_t* items = row->d.row.items;

    // If there are multiple items
    if(row->d.row.numItems > 1)
    {
        row->d.row.tAccumulatedUs += tElapsedUs;
        // Get the X position for the selected item, centering it
        int16_t xPos = row->d.row.xOffset + ((menu->disp->w - textWidth(menu->mFont, items->d.item.name)) / 2);
        int16_t xPosBox = xPos;

        // Then work backwards to make sure the entire row is drawn
        while(xPos > 0)
        {
            // Iterate backwards
            items = items->prev;
            // Adjust the x pos
            xPos -= (textWidth(menu->mFont, items->d.item.name) + ITEM_SPACING + 1);
        }

        // Then draw items until we're off the OLED
        while(xPos < menu->disp->w)
        {
            // Check if this text should be boxed
            bool drawBox = (shouldDrawBox && (xPos == xPosBox) && row->d.row.items == items);

            // Plot the text
            int16_t xPosS = xPos;
            xPos = drawText(menu->disp, menu->mFont, menu->fColor,
                            items->d.item.name, xPos, yPos);

            // If this is the selected item, draw a box around it
            if (drawBox)
            {
                plotRect(menu->disp, xPosS - 2, yPos - 2, xPos, yPos + menu->mFont->h + 1, menu->fColor);
            }

            // Add some space between items
            xPos += ITEM_SPACING;

            // Iterate to the next item
            items = items->next;
        }

        // If the offset is nonzero, move it towards zero
        while(row->d.row.tAccumulatedUs > US_PER_PIXEL_X)
        {
            row->d.row.tAccumulatedUs -= US_PER_PIXEL_X;
            if(row->d.row.xOffset < 0)
            {
                row->d.row.xOffset++;
            }
            else if(row->d.row.xOffset > 0)
            {
                row->d.row.xOffset--;
            }
        }
    }
    else
    {
        // If there's only one item, just plot it
        int16_t xPosS = (menu->disp->w - textWidth(menu->mFont, items->d.item.name)) / 2;
        int16_t xPosF = drawText(menu->disp, menu->mFont, menu->fColor,
                                 items->d.item.name, xPosS, yPos);

        // If this is the selected item, draw a box around it
        if(shouldDrawBox)
        {
            plotRect(menu->disp, xPosS -  2, yPos - 2, xPosF, yPos + menu->mFont->h + 1, menu->fColor);
        }
    }
}

/**
 * Draw the menu to the OLED. The menu has animations for smooth scrolling,
 * so it is recommended this function be called at least every 20ms
 *
 * @param menu The menu to draw
 */
void drawMenu(menu_t* menu)
{
    uint64_t tNowUs = esp_timer_get_time();
    uint64_t tElapsedUs = tNowUs - menu->tLastCallUs;
    menu->tLastCallUs = tNowUs;
    menu->tAccumulatedUs += tElapsedUs;

    // First clear the OLED
    if(1 == menu->numRows)
    {
        fillDisplayArea(menu->disp, 0, menu->disp->h - menu->mFont->h - 4, menu->disp->w, menu->disp->h, menu->bColor);
    }
    else
    {
        fillDisplayArea(menu->disp, 0, 0, menu->disp->w, menu->disp->h, menu->bColor);
    }

    // Start with the seleted row to be drawn
    int16_t yPos = menu->yOffset + SELECTED_ROW_Y;
    cLinkedNode_t* row = menu->rows;

    if(1 == menu->numRows)
    {
        yPos = menu->disp->h - menu->mFont->h - 2;
    }
    else
    {
        // Work backwards to draw all prior rows on the OLED
        while(yPos + menu->mFont->h >= BLANK_SPACE_Y)
        {
            row = row->prev;
            yPos -= (menu->mFont->h + ROW_SPACING);
        }
    }

    // Draw rows until you run out of space on the OLED
    bool boxDrawn = false;
    while(yPos < menu->disp->h)
    {
        // Check if a box should be drawn
        bool shouldDrawBox = (row == menu->rows) && (menu->yOffset == 0) && (row->d.row.xOffset == 0);

        // Draw the row
        drawRow(menu, row, yPos,
                !boxDrawn && shouldDrawBox,
                tElapsedUs);
        
        // Only draw one box
        if(shouldDrawBox)
        {
            boxDrawn = true;
        }

        // Move to the next row
        row = row->next;
        yPos += menu->mFont->h + ROW_SPACING;
    }

    if(1 != menu->numRows)
    {
        // If the offset is nonzero, move it towards zero
        while(menu->tAccumulatedUs > US_PER_PIXEL_Y)
        {
            menu->tAccumulatedUs -= US_PER_PIXEL_Y;
            if(menu->yOffset < 0)
            {
                menu->yOffset++;
            }
            if(menu->yOffset > 0)
            {
                menu->yOffset--;
            }
        }

        // Clear the top 37 pixels of the OLED
        fillDisplayArea(menu->disp, 0, 0, menu->disp->w, BLANK_SPACE_Y, menu->bColor);

        // Draw the title, centered
        int16_t titleOffset = (menu->disp->w - textWidth(menu->tFont, menu->title)) / 2;
        drawText(menu->disp, menu->tFont, menu->fColor, menu->title, titleOffset, 8);
    }
}

/**
 * This is called to process button presses. When a button is pressed, the
 * selected row or item immediately changes. An offset is set in the X or Y
 * direction so the menu doesn't appear to move, then the menu is smoothly
 * animated to the final position
 *
 * @param menu The menu to process a button for
 * @param btn  The button that was pressed
 */
void menuButton(menu_t* menu, int btn)
{
    // If the menu is in motion, ignore this button press
    if((menu->yOffset != 0) || (menu->rows->d.row.xOffset != 0))
    {
        return;
    }

    if(menu->numRows == 1 && (btn == 3 || btn == 1))
    {
        // If there is one row, ignore up & down buttons
        return;
    }

    switch(btn)
    {
    case UP:
    {
        // Up pressed, move to the prior row and set a negative offset
        menu->rows = menu->rows->prev;
        menu->yOffset = -(menu->mFont->h + ROW_SPACING);
        break;
    }
    case DOWN:
    {
        // Down pressed, move to the next row and set a positive offset
        menu->rows = menu->rows->next;
        menu->yOffset = (menu->mFont->h + ROW_SPACING);
        break;
    }
    case LEFT:
    {
        // Left pressed, only change if there are multiple items in this row
        if(menu->rows->d.row.numItems > 1)
        {
            // To properly center the word, measure both old and new centered words
            uint8_t oldWordWidth = textWidth(menu->mFont, menu->rows->d.row.items->d.item.name);
            // Move to the previous item
            menu->rows->d.row.items = menu->rows->d.row.items->prev;
            uint8_t newWordWidth = textWidth(menu->mFont, menu->rows->d.row.items->d.item.name);
            // Set the offset to smootly animate from the old, centered word to the new centered word
            menu->rows->d.row.xOffset = -(newWordWidth + ITEM_SPACING + ((oldWordWidth - newWordWidth - 1) / 2));
        }
        break;
    }
    case RIGHT:
    {
        // Right pressed, only change if there are multiple items in this row
        if(menu->rows->d.row.numItems > 1)
        {
            // To properly center the word, measure both old and new centered words
            uint8_t oldWordWidth = textWidth(menu->mFont, menu->rows->d.row.items->d.item.name);
            // Move to the next item
            menu->rows->d.row.items = menu->rows->d.row.items->next;
            uint8_t newWordWidth = textWidth(menu->mFont, menu->rows->d.row.items->d.item.name);
            // Set the offset to smootly animate from the old, centered word to the new centered word
            menu->rows->d.row.xOffset = oldWordWidth + ITEM_SPACING + ((newWordWidth - oldWordWidth - 1) / 2);
        }
        break;
    }
    case BTN_A:
    {
        // Select pressed. Tell the host mode what item was selected
        if(NULL != menu->cbFunc)
        {
            menu->cbFunc(menu->rows->d.row.items->d.item.name);
        }
        break;
    }
    default:
    {
        break;
    }
    }
}
