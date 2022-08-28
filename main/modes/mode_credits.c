//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "mode_credits.h"
#include "mode_main_menu.h"

//==============================================================================
// Defines
//==============================================================================

#define ABS(X) (((X) < 0) ? -(X) : (X))

//==============================================================================
// Functions Prototypes
//==============================================================================

void creditsEnterMode(display_t* disp);
void creditsExitMode(void);
void creditsMainLoop(int64_t elapsedUs);
void creditsButtonCb(buttonEvt_t* evt);

//==============================================================================
// Variables
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t radiostars;
    int64_t tElapsedUs;
    int8_t scrollMod;
    int16_t yOffset;
} credits_t;

credits_t* credits;

swadgeMode modeCredits =
{
    .modeName = "Credits",
    .fnEnterMode = creditsEnterMode,
    .fnExitMode = creditsExitMode,
    .fnMainLoop = creditsMainLoop,
    .fnButtonCallback = creditsButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

// Everyone's here
static const char* creditNames[] =
{
    "Adam Feinstein"
};

// Must be same length as creditNames
static const paletteColor_t creditColors[] =
{
    c555
};


//==============================================================================
// Functions
//==============================================================================

/**
 * Enter the credits mode, allocate and initialize memory
 */
void creditsEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    credits = (credits_t*)calloc(1, sizeof(credits_t));

    // Save a pointer to the display
    credits->disp = disp;

    // Load some fonts
    loadFont("radiostars.font", &credits->radiostars);

    // Set initial variables
    credits->yOffset = disp->h;
    credits->tElapsedUs = 0;
    credits->scrollMod = 1;
}

/**
 * Exit the credits mode, free memory
 */
void creditsExitMode(void)
{
    freeFont(&credits->radiostars);
    free(credits);
}

/**
 * Main credits loop, draw some scrolling credits
 *
 * @param elapsedUs The time elapsed since the last call
 */
void creditsMainLoop(int64_t elapsedUs)
{
    credits->tElapsedUs += elapsedUs;

    // If enough time has passed, translate and redraw text
    uint32_t updateTime = 100000 / ABS(credits->scrollMod);
    if(credits->tElapsedUs > updateTime)
    {
        credits->tElapsedUs -= updateTime;

        // This static var tracks the vertical scrolling offset
        credits->yOffset -= (credits->scrollMod > 0) ? 1 : -1;

        // Clear first
        credits->disp->clearPx();

        // Draw names until the cursor is off the screen
        int16_t yPos = 0;
        int16_t idx = 0;
        while((yPos + credits->yOffset) < credits->disp->h)
        {
            // Only draw names with negative offsets if they're a little on screen
            if((yPos + credits->yOffset) >= -credits->radiostars.h)
            {
                // If the names have scrolled back to the start, reset the scroll vars
                if(0 == (yPos + credits->yOffset) && 0 == idx)
                {
                    credits->yOffset = 0;
                    yPos = 0;
                }

                // Center and draw the text
                int16_t tWidth = textWidth(&credits->radiostars, creditNames[idx]);
                drawText(credits->disp, &credits->radiostars, creditColors[idx], creditNames[idx],
                         (credits->disp->w - tWidth) / 2, (yPos + credits->yOffset));
            }

            // Always update the idx and cursor position, even if the text wasn't drawn
            idx = (idx + 1) % ARRAY_SIZE(creditNames);
            yPos += credits->radiostars.h + 3;
        }
    }
}

/**
 * @brief Credits button callback, either speed up, reverse, or exit credits
 *
 * @param evt The button event
 */
void creditsButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            {
                // Scroll faster
                credits->scrollMod = 4;
                break;
            }
            case DOWN:
            {
                // Scroll faster, backwards
                credits->scrollMod = -4;
                break;
            }
            case BTN_A:
            case BTN_B:
            {
                // Exit
                switchToSwadgeMode(&modeMainMenu);
                break;
            }
            case LEFT:
            case RIGHT:
            case START:
            case SELECT:
            {
                break;
            }
        }
    }
    else
    {
        switch (evt->button)
        {
            case UP:
            case DOWN:
            {
                // Resume normal scrolling
                credits->scrollMod = 1;
                break;
            }
            case BTN_A:
            case BTN_B:
            case LEFT:
            case RIGHT:
            case START:
            case SELECT:
            {
                // Do nothing
                break;
            }
        }
    }
}
