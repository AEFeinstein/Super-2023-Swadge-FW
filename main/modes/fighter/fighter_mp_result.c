//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <stdio.h>

#include "display.h"
#include "nvs_manager.h"
#include "musical_buzzer.h"

#include "fighter_mp_result.h"
#include "fighter_records.h"
#include "fighter_menu.h"
#include "meleeMenu.h"

//==============================================================================
// Defines
//==============================================================================

#define X_MARGIN 20
#define Y_MARGIN 8

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
    uint32_t roundTimeMs;
    bool jingleIsPlaying;
    fightingGameType_t type;
} hrRes_t;

//==============================================================================
// Variables
//==============================================================================

hrRes_t* mpr;

const song_t fVictoryJingle =
{
    .notes =
    {
        {.note = D_4, .timeMs = 206},
        {.note = A_SHARP_2, .timeMs = 103},
        {.note = E_4, .timeMs = 59},
        {.note = SILENCE, .timeMs = 43},
        {.note = F_4, .timeMs = 59},
        {.note = SILENCE, .timeMs = 147},
        {.note = A_4, .timeMs = 206},
        {.note = A_2, .timeMs = 114},
        {.note = SILENCE, .timeMs = 92},
        {.note = A_2, .timeMs = 114},
        {.note = SILENCE, .timeMs = 92},
        {.note = A_4, .timeMs = 103},
        {.note = B_4, .timeMs = 103},
        {.note = C_SHARP_5, .timeMs = 103},
        {.note = A_4, .timeMs = 103},
        {.note = D_5, .timeMs = 114},
        {.note = SILENCE, .timeMs = 92},
        {.note = D_2, .timeMs = 103},
        {.note = D_6, .timeMs = 103},
        {.note = F_SHARP_6, .timeMs = 103},
        {.note = G_6, .timeMs = 103},
        {.note = A_6, .timeMs = 103},
        {.note = D_6, .timeMs = 103},
        {.note = D_7, .timeMs = 103},
        {.note = SILENCE, .timeMs = 310},
        {.note = D_2, .timeMs = 103},
    },
    .numNotes = 26,
    .shouldLoop = false
};

const song_t fLossJingle =
{
    .notes =
    {
        {.note = F_5, .timeMs = 209},
        {.note = D_4, .timeMs = 186},
        {.note = A_4, .timeMs = 197},
        {.note = F_5, .timeMs = 209},
        {.note = E_5, .timeMs = 197},
        {.note = C_SHARP_4, .timeMs = 197},
        {.note = A_4, .timeMs = 197},
        {.note = E_5, .timeMs = 209},
        {.note = D_5, .timeMs = 197},
        {.note = A_SHARP_3, .timeMs = 197},
        {.note = G_4, .timeMs = 197},
        {.note = D_5, .timeMs = 209},
        {.note = C_SHARP_5, .timeMs = 244},
        {.note = A_SHARP_4, .timeMs = 133},
        {.note = A_4, .timeMs = 331},
        {.note = G_SHARP_4, .timeMs = 255},
        {.note = A_4, .timeMs = 81},
        {.note = G_SHARP_4, .timeMs = 63},
        {.note = A_4, .timeMs = 959},
    },
    .numNotes = 19,
    .shouldLoop = false
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize the result display after a multiplayer match contest
 *
 * @param disp The display to draw to
 * @param font The font to draw with
 * @param roundTimeMs The time the round took, in milliseconds
 * @param self This swadge's character
 * @param selfKOs This swadge's number of KOs
 * @param selfDmg The amount of damage this swadge did
 * @param other The other swadge's character
 * @param otherKOs The other swadge's number of KOs
 * @param otherDmg The amount of damage the other swadge did
 * @param type The type of game played
 */
void initFighterMpResult(display_t* disp, font_t* font, uint32_t roundTimeMs,
                         fightingCharacter_t self,  int8_t selfKOs, int16_t selfDmg,
                         fightingCharacter_t other, int8_t otherKOs, int16_t otherDmg,
                         fightingGameType_t type)
{
    mpr = calloc(1, sizeof(hrRes_t));

    // Save the display and font pointers
    mpr->disp = disp;
    mpr->font = font;

    // Save the results
    mpr->roundTimeMs = roundTimeMs;

    mpr->self = self;
    mpr->selfDmg = selfDmg;
    mpr->selfKOs = selfKOs;

    mpr->other = other;
    mpr->otherDmg = otherDmg;
    mpr->otherKOs = otherKOs;

    mpr->type = type;

   if(MULTIPLAYER == type)
    {
        // Save the result, only for multiplayer matches
        saveMpResult(self, selfKOs > otherKOs);
    }
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
    if(!mpr->jingleIsPlaying)
    {
        mpr->jingleIsPlaying = true;
        if(mpr->selfKOs > mpr->otherKOs)
        {
            buzzer_play_sfx(&fVictoryJingle);
        }
        else
        {
            buzzer_play_sfx(&fLossJingle);
        }
    }

    drawBackgroundGrid(mpr->disp);

    // Text colors
    paletteColor_t headerColor = c540;
    paletteColor_t entryColor = c431;
    paletteColor_t statColor = c321;

    int16_t yOff = 8;
    int16_t tWidth = 0;
    char text[32];
    const char dmgStr[] = "Dmg Dealt";
    const char koStr[] = "KOs";

    if(LOCAL_VS == mpr->type)
    {
        // Title
        if(mpr->selfKOs > mpr->otherKOs)
        {
            sprintf(text, "Player 1 Wins!");
        }
        else
        {
            sprintf(text, "Player 2 Wins!");
        }
    }
    else
    {
        // Title
        if(mpr->selfKOs > mpr->otherKOs)
        {
            sprintf(text, "You Win!");
        }
        else
        {
            sprintf(text, "Sorry Bud");
        }
    }

    tWidth = textWidth(mpr->font, text);
    drawText(mpr->disp, mpr->font, headerColor, text, (mpr->disp->w - tWidth) / 2, yOff);
    yOff += mpr->font->h + Y_MARGIN;

    // Round time
    sprintf(text, "%u:%02u.%03u", (mpr->roundTimeMs / 60000), (mpr->roundTimeMs / 1000) % 60, mpr->roundTimeMs % 1000);
    tWidth = textWidth(mpr->font, text);
    drawText(mpr->disp, mpr->font, statColor, text, (mpr->disp->w - tWidth) / 2, yOff);
    yOff += mpr->font->h + Y_MARGIN;

    // This swadge
    sprintf(text, "%s", charNames[mpr->self]);
    drawText(mpr->disp, mpr->font, entryColor, text, X_MARGIN, yOff);

    yOff += mpr->font->h + Y_MARGIN;

    sprintf(text, "%s", koStr);
    drawText(mpr->disp, mpr->font, statColor, text, X_MARGIN, yOff);

    sprintf(text, "%d", mpr->selfKOs);
    tWidth = textWidth(mpr->font, text);
    drawText(mpr->disp, mpr->font, statColor, text, mpr->disp->w - X_MARGIN - tWidth, yOff);

    yOff += mpr->font->h + Y_MARGIN;

    sprintf(text, "%s", dmgStr);
    drawText(mpr->disp, mpr->font, statColor, text, X_MARGIN, yOff);

    sprintf(text, "%d%%", mpr->selfDmg);
    tWidth = textWidth(mpr->font, text);
    drawText(mpr->disp, mpr->font, statColor, text, mpr->disp->w - X_MARGIN - tWidth, yOff);

    yOff += mpr->font->h + Y_MARGIN;

    // The other swadge
    sprintf(text, "%s", charNames[mpr->other]);
    drawText(mpr->disp, mpr->font, entryColor, text, X_MARGIN, yOff);

    yOff += mpr->font->h + Y_MARGIN;

    sprintf(text, "%s", koStr);
    drawText(mpr->disp, mpr->font, statColor, text, X_MARGIN, yOff);

    sprintf(text, "%d", mpr->otherKOs);
    tWidth = textWidth(mpr->font, text);
    drawText(mpr->disp, mpr->font, statColor, text, mpr->disp->w - X_MARGIN - tWidth, yOff);

    yOff += mpr->font->h + Y_MARGIN;

    sprintf(text, "%s", dmgStr);
    drawText(mpr->disp, mpr->font, statColor, text, X_MARGIN, yOff);

    sprintf(text, "%d%%", mpr->otherDmg);
    tWidth = textWidth(mpr->font, text);
    drawText(mpr->disp, mpr->font, statColor, text, mpr->disp->w - X_MARGIN - tWidth, yOff);
}
