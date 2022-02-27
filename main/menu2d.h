/*
 * menu2d.h
 *
 *  Created on: Jul 12, 2020
 *      Author: adam
 */

#ifndef _MENU_2D_H_
#define _MENU_2D_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include "display.h"

//==============================================================================
// Structs, Unions, and Typedefs
//==============================================================================

typedef void (*menuCb)(const char*);

struct cLinkedNode;

typedef struct
{
    struct cLinkedNode* items;
    uint8_t numItems;
    int8_t xOffset;
    uint32_t tAccumulatedUs;
} rowInfo_t;

typedef struct
{
    const char* name;
} itemInfo_t;

typedef union
{
    rowInfo_t row;
    itemInfo_t item;
} linkedInfo_t;

typedef struct cLinkedNode
{
    linkedInfo_t d;
    struct cLinkedNode* prev;
    struct cLinkedNode* next;
} cLinkedNode_t;

typedef struct
{
    const char* title;
    cLinkedNode_t* rows;
    uint8_t numRows;
    menuCb cbFunc;
    int8_t yOffset;
    uint64_t tLastCallUs;
    uint64_t tAccumulatedUs;
    display_t * disp;
    font_t * tFont;
    font_t * mFont;
    rgba_pixel_t fColor;
    rgba_pixel_t bColor;
} menu_t;

//==============================================================================
// Function Prototypes
//==============================================================================

menu_t* initMenu(display_t * disp, const char* title, font_t * titleFont,
                 font_t * menuFont, rgba_pixel_t foregroundColor,
                 rgba_pixel_t backgroundColor, menuCb cbFunc);
void deinitMenu(menu_t* menu);
void addRowToMenu(menu_t* menu);
linkedInfo_t* addItemToRow(menu_t* menu, const char* name);
void removeItemFromMenu(menu_t* menu, const char* name);
void drawMenu(menu_t* menu);
void menuButton(menu_t* menu, int btn);

#endif
