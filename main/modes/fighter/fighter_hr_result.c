#include <stdio.h>

#include "display.h"
#include "fighter_hr_result.h"

// TODO struct and alloc these
fightingCharacter_t _character;
vector_t _sbVel;
vector_t _sbPos;
int32_t _grav;
display_t* _disp;
font_t* _font;
int64_t _tAccumUs;
wsg_t _sbi;
int32_t rotDeg = 0;
int32_t _finalXpos = 0;

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
void initFighterHrResult(display_t* disp, font_t* font,
                         fightingCharacter_t character, vector_t pos, vector_t vel, int32_t gravity,
                         int32_t platformEndX)
{
    printf("[%d, %d], [%d, %d]\n", pos.x, pos.y, vel.x, vel.y);
    // Save the display and foint pointers
    _disp = disp;
    _font = font;

    // Save the character
    _character = character;

    // Load the sandbag sprite
    loadWsg("sbi.wsg", &_sbi);

    // Calculate final distance iteratively, based on inputs
    // While the sandbag hasn't landed yet
    int32_t lBound = (_disp->h - _sbi.h) << SF;
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
    _finalXpos = (pos.x - platformEndX) >> (SF - 1);

    // X velocity is variable, 8000 is the max
    _sbVel.x = 8000;

    // If the X velocity is negative
    if(vel.x < 0)
    {
        // Start in the bottom right
        _sbPos.x = (disp->w - _sbi.w) << SF;
        // Negate X velocity
        _sbVel.x = -_sbVel.x;
    }
    else
    {
        // Start in the bottom left
        _sbPos.x = 0;
    }
    // Start at the bottom
    _sbPos.y = (disp->h - _sbi.h) << SF;

    // This velocity & gravity combo go from the bottom of the screen to the top
    _sbVel.y = -30000;
    _grav = 8192;
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
        int32_t lBound = (_disp->h - _sbi.h) << SF;
        if(_sbPos.y < lBound || _sbVel.y < 0)
        {
            vector_t v0 = _sbVel;

            // Now that we have X velocity, find the new X position
            int32_t deltaX = (((_sbVel.x + v0.x) * FRAME_TIME_MS) >> (SF + 1));
            _sbPos.x += deltaX;

            // Fighter is in the air, so there will be a new Y
            _sbVel.y = v0.y + ((_grav * FRAME_TIME_MS) >> SF);
            // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
            if(_sbVel.y > 60 << SF)
            {
                _sbVel.y = 60 << SF;
            }
            // Now that we have Y velocity, find the new Y position
            int32_t deltaY = (((_sbVel.y + v0.y) * FRAME_TIME_MS) >> (SF + 1));
            _sbPos.y += deltaY;

            // Update rotation animation
            rotDeg = (rotDeg + 18) % 360;
        }
        else
        {
            // Can't go lower than here
            _sbPos.y = lBound;
            _sbVel.y = 0;
        }

        // Draw stuff
        drawWsg(_disp, &_sbi, _sbPos.x >> SF, _sbPos.y >> SF, false, false, rotDeg);
        char str[64];
        sprintf(str, "%dm", _finalXpos);
        drawText(_disp, _font, c555, str, 0, 64);
    }
}