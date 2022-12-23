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

#define MAX_ROWS 65535
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
    uint16_t numRows;
    uint16_t numRowsAllocated;
    uint16_t firstRowOnScreen;
    uint16_t selectedRow;
    bool allowLEDControl;
    bool usePerRowXOffsets;

    bool animating;
    bool wrapping;
    int16_t animateSpeed;
    int16_t animateOffset;
    uint16_t animateStartRow;

    // these are dumb and should not exist please put them out of all our misery
    uint16_t lastFirstRow;
    uint16_t lastSelectedRow;
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
