//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <stdio.h>

#include "display.h"
#include "nvs_manager.h"

#include "fighter_hr_result.h"
#include "fighter_records.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t* font;
    wsg_t sbi;
    vector_t sbVel;
    vector_t sbPos;
    fightingCharacter_t character;
    int64_t tAccumUs;
    int32_t grav;
    int32_t rotDeg;
    int32_t finalXpos;
    bool isNewRecord;
} hrRes_t;

//==============================================================================
// Variables
//==============================================================================

hrRes_t* hrr;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize the result display after a home run contest
 *
 * @param disp
 * @param font
 * @param character
 * @param pos
 * @param vel
 * @param gravity
 * @param platformStartX
 * @param platformEndX
 */
void initFighterHrResult(display_t* disp, font_t* font,
                         fightingCharacter_t character, vector_t pos, vector_t vel, int32_t gravity,
                         int32_t platformStartX, int32_t platformEndX)
{
    hrr = calloc(1, sizeof(hrRes_t));

    // Save the display and foint pointers
    hrr->disp = disp;
    hrr->font = font;

    // Save the character
    hrr->character = character;

    // Load the sandbag sprite
    loadWsg("sbi.wsg", &hrr->sbi);

    // Calculate final distance iteratively, based on inputs
    // While the sandbag hasn't landed yet
    int32_t lBound = (hrr->disp->h - hrr->sbi.h) << SF;
    while(pos.y < lBound || vel.y < 0)
    {
        vector_t v0 = vel;

        // Now that we have X velocity, find the new X position
        int32_t deltaX = (((vel.x + v0.x) * FRAME_TIME_MS) >> (SF + 1));
        pos.x += deltaX;

        // Fighter is in the air, so there will be a new Y
        vel.y = v0.y + ((gravity * FRAME_TIME_MS) >> SF);
        // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
        if(vel.y > 60 << SF)
        {
            vel.y = 60 << SF;
        }
        // Now that we have Y velocity, find the new Y position
        int32_t deltaY = (((vel.y + v0.y) * FRAME_TIME_MS) >> (SF + 1));
        pos.y += deltaY;
    }

    // Save the final distance
    if(pos.x > platformEndX)
    {
        // Sandbag is to the left of the platform
        hrr->finalXpos = (pos.x - platformEndX) >> (SF - 1);
    }
    else if (pos.x + (hrr->sbi.w >> SF) < platformStartX)
    {
        // Sandbag is to the right of the platform
        hrr->finalXpos = ((pos.x + (hrr->sbi.w >> SF)) - platformStartX) >> (SF - 1);
    }
    else
    {
        // Sandbag is on the platform
        hrr->finalXpos = 0;
    }

    // Check NVM if this is a high score. Get the key first
    hrr->isNewRecord = checkHomerunRecord(hrr->character, hrr->finalXpos);

    // X velocity is variable, 8000 is the max
    hrr->sbVel.x = 8000;

    // If the sandbag ended up left of the platform
    if(hrr->finalXpos < 0)
    {
        // Start in the bottom right
        hrr->sbPos.x = (disp->w - hrr->sbi.w) << SF;
        // Negate X velocity
        hrr->sbVel.x = -hrr->sbVel.x;
    }
    else if(hrr->finalXpos > 0)
    {
        // If it ended o right of the platform, start in the bottom left
        hrr->sbPos.x = 0;
    }
    else
    {
        // No movement, start in the middle
        hrr->sbPos.x = ((disp->w - hrr->sbi.w) / 2) << SF;
    }
    // Start at the bottom
    hrr->sbPos.y = (disp->h - hrr->sbi.h) << SF;

    if(0 != hrr->finalXpos)
    {
        // This velocity & gravity combo go from the bottom of the screen to the top
        hrr->sbVel.y = -30000;
        hrr->grav = 8192;
    }
    else
    {
        // Sandbag on a platform isn't moving
        hrr->sbVel.y = 0;
        hrr->grav = 0;
    }
}

/**
 * @brief Deinitialize the result display after a home run contest
 */
void deinitFighterHrResult(void)
{
    if(NULL != hrr)
    {
        freeWsg(&hrr->sbi);
        free(hrr);
        hrr = NULL;
    }
}

/**
 * @brief Main loop to draw the display for a home run contest result
 *
 * @param elapsedUs The time elapsed since this was last called
 */
void fighterHrResultLoop(int64_t elapsedUs)
{
    // Keep track of time and only calculate frames every FRAME_TIME_MS
    hrr->tAccumUs += elapsedUs;
    if (hrr->tAccumUs > (FRAME_TIME_MS * 1000))
    {
        hrr->tAccumUs -= (FRAME_TIME_MS * 1000);

        // If the sandbag hasn't landed yet
        int32_t lBound = (hrr->disp->h - hrr->sbi.h) << SF;
        if(hrr->sbPos.y < lBound || hrr->sbVel.y < 0)
        {
            vector_t v0 = hrr->sbVel;

            // Now that we have X velocity, find the new X position
            int32_t deltaX = (((hrr->sbVel.x + v0.x) * FRAME_TIME_MS) >> (SF + 1));
            hrr->sbPos.x += deltaX;

            // Fighter is in the air, so there will be a new Y
            hrr->sbVel.y = v0.y + ((hrr->grav * FRAME_TIME_MS) >> SF);
            // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
            if(hrr->sbVel.y > 60 << SF)
            {
                hrr->sbVel.y = 60 << SF;
            }
            // Now that we have Y velocity, find the new Y position
            int32_t deltaY = (((hrr->sbVel.y + v0.y) * FRAME_TIME_MS) >> (SF + 1));
            hrr->sbPos.y += deltaY;

            // Update rotation animation
            hrr->rotDeg = (hrr->rotDeg + 18) % 360;
        }
        else
        {
            // Can't go lower than here
            hrr->sbPos.y = lBound;
            hrr->sbVel.y = 0;
        }
    }

    // Draw sandbag
    drawWsg(hrr->disp, &hrr->sbi, hrr->sbPos.x >> SF, hrr->sbPos.y >> SF, false, false, hrr->rotDeg);

    // Draw centered text
    char str[32];
    sprintf(str, "%dm", hrr->finalXpos);
    drawText(hrr->disp, hrr->font, c555, str,
             (hrr->disp->w - textWidth(hrr->font, str)) / 2,
             (hrr->disp->h - hrr->font->h) / 2);

    // Draw "New Record!", maybe
    if(hrr->isNewRecord)
    {
        const char newRecordStr[] = "New Record!";
        drawText(hrr->disp, hrr->font, c555, newRecordStr,
                 (hrr->disp->w - textWidth(hrr->font, newRecordStr)) / 2,
                 ((hrr->disp->h - hrr->font->h) / 2) + 4 + hrr->font->h);
    }
}
