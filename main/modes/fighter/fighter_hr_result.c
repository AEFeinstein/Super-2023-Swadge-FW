#include <stdio.h>

#include "display.h"
#include "fighter_hr_result.h"

// TODO struct and alloc these
fightingCharacter_t _character;
vector_t sbVel;
vector_t sbPos;
int32_t _grav;
display_t* _disp;
font_t* _font;
int64_t _tAccumUs;
wsg_t _sbi;
int32_t rotDeg = 0;

/**
 * @brief Initialize the result display after a home run contest
 *
 * @param disp
 * @param font
 * @param character
 * @param pos
 * @param vel
 * @param gravity
 */
void initFighterHrResult(display_t* disp, font_t* font, fightingCharacter_t character, vector_t pos, vector_t vel,
                         int32_t gravity)
{
    _character = character;
    sbVel = vel;
    sbPos = pos;
    _disp = disp;
    _font = font;
    _grav = gravity;

    loadWsg("sbi.wsg", &_sbi);
}

/**
 * @brief Deinitialize the result display after a home run contest
 */
void deinitFighterHrResult(void)
{
    freeWsg(&_sbi);
}

/**
 * @brief Main loop to draw the display for a home run contest result
 *
 * @param elapsedUs The time elapsed since this was last called
 */
void fighterHrResultLoop(int64_t elapsedUs)
{
    // Keep track of time and only calculate frames every FRAME_TIME_MS
    _tAccumUs += elapsedUs;
    if (_tAccumUs > (FRAME_TIME_MS * 1000))
    {
        _tAccumUs -= (FRAME_TIME_MS * 1000);

        // If the sandbag hasn't landed yet
        if(sbPos.y < (_disp->h << SF))
        {
            vector_t v0 = sbVel;

            // Now that we have X velocity, find the new X position
            int32_t deltaX = (((sbVel.x + v0.x) * FRAME_TIME_MS) >> (SF + 1));
            sbPos.x += deltaX;

            // Fighter is in the air, so there will be a new Y
            sbVel.y = v0.y + ((_grav * FRAME_TIME_MS) >> SF);
            // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
            if(sbVel.y > 60 << SF)
            {
                sbVel.y = 60 << SF;
            }
            // Now that we have Y velocity, find the new Y position
            int32_t deltaY = (((sbVel.y + v0.y) * FRAME_TIME_MS) >> (SF + 1));
            sbPos.y += deltaY;
        }
        else
        {
            // Can't go lower than here
            sbPos.y = (_disp->h << SF);
        }

        // Update rotation animation
        rotDeg = (rotDeg + 9) % 360;

        // Draw stuff
        _disp->clearPx();
        drawWsg(_disp, &_sbi, sbPos.x >> SF, sbPos.y >> SF, false, false, rotDeg);
        char str[64];
        sprintf(str, "%d (%d, %d)", _character, sbPos.x >> SF, sbPos.y >> SF);
        drawText(_disp, _font, c555, str, 0, 64);
    }
}