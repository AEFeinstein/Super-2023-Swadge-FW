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
    uint32_t roundTime;
} hrRes_t;

//==============================================================================
// Variables
//==============================================================================

hrRes_t* mpr;

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize the result display after a multiplayer match contest
 *
 * @param disp The display to draw to
 * @param font The font to draw with
 * @param roundTime The time the round took, in seconds
 * @param self This swadge's character
 * @param selfKOs This swadge's number of KOs
 * @param selfDmg The amount of damage this swadge did
 * @param other The other swadge's character
 * @param otherKOs The other swadge's number of KOs
 * @param otherDmg The amount of damage the other swadge did
 */
void initFighterMpResult(display_t* disp, font_t* font, uint32_t roundTime,
                         fightingCharacter_t self,  int8_t selfKOs, int16_t selfDmg,
                         fightingCharacter_t other, int8_t otherKOs, int16_t otherDmg)
{
    mpr = calloc(1, sizeof(hrRes_t));

    // Save the display and font pointers
    mpr->disp = disp;
    mpr->font = font;

    // Save the results
    mpr->roundTime = roundTime;

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
    if(NULL != mpr)
    {
        free(mpr);
        mpr = NULL;
    }
}

/**
 * @brief Main loop to draw the display for a home run contest result
 *
 * @param elapsedUs The time elapsed since this was last called
 */
void fighterMpResultLoop(int64_t elapsedUs)
{
    drawBackgroundGrid(mpr->disp);

    // TODO draw results better

    int16_t yOff = 40;
    char text[32];

    sprintf(text, "roundTime: %d", mpr->roundTime);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;

    sprintf(text, "Self: %d", mpr->self);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;

    sprintf(text, "Self Dmg: %d", mpr->selfDmg);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;

    sprintf(text, "Self KOs: %d", mpr->selfKOs);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;

    sprintf(text, "other: %d", mpr->other);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;

    sprintf(text, "other Dmg: %d", mpr->otherDmg);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;

    sprintf(text, "other KOs: %d", mpr->otherKOs);
    drawText(mpr->disp, mpr->font, c555, text, 0, yOff);
    yOff += mpr->font->h + 4;
}
