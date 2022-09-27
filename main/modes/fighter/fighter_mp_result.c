//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <stdio.h>

#include "display.h"
#include "nvs_manager.h"

#include "fighter_mp_result.h"
#include "fighter_records.h"
#include "meleeMenu.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t* font;
    fightingCharacter_t self;
    int8_t selfKOs;
    int16_t selfDmg;
    fightingCharacter_t other;
    int8_t otherKOs;
    int16_t otherDmg;
} hrRes_t;

//==============================================================================
// Variables
//==============================================================================

hrRes_t* mpr;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 * @param disp
 * @param font
 */
void initFighterMpResult(display_t* disp, font_t* font, uint32_t roundTime,
                         fightingCharacter_t self,  int8_t selfKOs, int16_t selfDmg,
                         fightingCharacter_t other, int8_t otherKOs, int16_t otherDmg)
{
    mpr = calloc(1, sizeof(hrRes_t));

    // Save the display and foint pointers
    mpr->disp = disp;
    mpr->font = font;

    // Save the results
    mpr->self = self;
    mpr->selfDmg = selfDmg;
    mpr->selfKOs = selfKOs;

    mpr->other = other;
    mpr->otherDmg = otherDmg;
    mpr->otherKOs = otherKOs;

    // TODO tally match result
    // mpr->isNewRecord = checkHomerunRecord(mpr->character, mpr->finalXpos);
}

/**
 * @brief Deinitialize the result display after a home run contest
 */
void deinitFighterMpResult(void)
{
    free(mpr);
}

/**
 * @brief Main loop to draw the display for a home run contest result
 *
 * @param elapsedUs The time elapsed since this was last called
 */
void fighterMpResultLoop(int64_t elapsedUs)
{
    drawBackgroundGrid(mpr->disp);
    printf("mpres\n");

    // TODO draw results
}
