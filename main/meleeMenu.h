#ifndef _MELEE_MENU_H_
#define _MELEE_MENU_H_

//==============================================================================
// Includes
//==============================================================================

#include "display.h"
#include "btn.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_ROWS 255
#define MAX_ROWS_ON_SCREEN 5
#define NUM_ROW_COLORS_AND_OFFSETS 6

//==============================================================================
// Typedefs
//==============================================================================

typedef void (*meleeMenuCb)(const char*);

typedef struct
{
    const char** rows;
    const char* title;
    font_t* font;
    meleeMenuCb cbFunc;
    uint8_t numRows;
    uint8_t numRowsAllocated;
    uint8_t firstRowOnScreen;
    uint8_t selectedRow;
    uint8_t allowLEDControl;
    uint8_t usePerRowXOffsets;
} meleeMenu_t;

//==============================================================================
// Function Prototypes
//==============================================================================

meleeMenu_t* initMeleeMenu(const char* title, font_t* font, meleeMenuCb cbFunc);
void resetMeleeMenu(meleeMenu_t* menu, const char* title, meleeMenuCb cbFunc);
int addRowToMeleeMenu(meleeMenu_t* menu, const char* label);
void deinitMeleeMenu(meleeMenu_t* menu);
void drawMeleeMenu(display_t* d, meleeMenu_t* menu);
void meleeMenuButton(meleeMenu_t* menu, buttonBit_t btn);
void drawBackgroundGrid(display_t * d);

#endif
