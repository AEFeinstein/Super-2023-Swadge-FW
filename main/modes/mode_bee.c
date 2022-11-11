//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "mode_bee.h"
#include "mode_main_menu.h"
#include "musical_buzzer.h"
#include "esp_random.h"
#include "secret_text.h"

//==============================================================================
// Defines
//==============================================================================

#define LINE_SPACING 1

#define ABS(X) (((X) < 0) ? -(X) : (X))

//==============================================================================
// Functions Prototypes
//==============================================================================

void beeEnterMode(display_t* disp);
void beeSetRandomText(void);
void beeExitMode(void);
void beeMainLoop(int64_t elapsedUs);
void beeButtonCb(buttonEvt_t* evt);

//==============================================================================
// Variables
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t font;
    int64_t tElapsedUs;
    int8_t scrollMod;
    int32_t yOffset;
    const char** text;
    uint16_t textLines;
    paletteColor_t textColor;
} bee_t;

bee_t* bee;

swadgeMode modeBee =
{
    .modeName = "CopyPastas",
    .fnEnterMode = beeEnterMode,
    .fnExitMode = beeExitMode,
    .fnMainLoop = beeMainLoop,
    .fnButtonCallback = beeButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
    .overrideUsb = false
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Enter the bee mode, allocate and initialize memory
 */
void beeEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    bee = (bee_t*)calloc(1, sizeof(bee_t));

    // Save a pointer to the display
    bee->disp = disp;

    // Load some fonts
    loadFont("ibm_vga8.font", &bee->font);

    beeSetRandomText();
}

void beeSetRandomText(void)
{
    // Set initial variables
    bee->yOffset = bee->disp->h;
    bee->tElapsedUs = 0;
    bee->scrollMod = 1;

    uint8_t numTexts;
    const textOption* texts = getTextOpts(&numTexts);

    uint8_t textNum = esp_random() % numTexts;
    bee->text = texts[textNum].text;
    bee->textLines = texts[textNum].lines;
    bee->textColor = texts[textNum].color;
}

/**
 * Exit the bee mode, free memory
 */
void beeExitMode(void)
{
    freeFont(&bee->font);
    free(bee);
}

/**
 * Main bee loop, draw some scrolling bee stuff
 *
 * @param elapsedUs The time elapsed since the last call
 */
void beeMainLoop(int64_t elapsedUs)
{
    bee->tElapsedUs += elapsedUs;

    // If enough time has passed, translate and redraw text
    uint32_t updateTime = 100000 / ABS(bee->scrollMod);
    if(bee->tElapsedUs > updateTime)
    {
        bee->tElapsedUs -= updateTime;

        // This static var tracks the vertical scrolling offset
        bee->yOffset -= (bee->scrollMod > 0) ? 1 : -1;

        // Clear first
        bee->disp->clearPx();

        // Draw names until the cursor is off the screen
        int32_t yPos = 0;
        int16_t idx = 0;
        while((yPos + bee->yOffset) < bee->disp->h)
        {
            // Only draw names with negative offsets if they're a little on screen
            if((yPos + bee->yOffset) >= -textHeight(&bee->font, bee->text[idx], bee->disp->w - 26, INT16_MAX))
            {
                // If the names have scrolled back to the start, reset the scroll vars
                if(0 == (yPos + bee->yOffset) && 0 == idx)
                {
                    bee->yOffset = 0;
                    yPos = 0;
                }

                int16_t textX = 13, textY = (yPos + bee->yOffset);

                // Draw the text
                drawTextWordWrap(bee->disp, &bee->font, bee->textColor, bee->text[idx],
                         &textX, &textY, bee->disp->w - 13, bee->disp->h + bee->font.h);

                // Add the height of the drawn text plus one line to yPos
                yPos = textY - bee->yOffset + bee->font.h + LINE_SPACING;
            }
            else
            {
                // Add the entire height of the text to yPos, to simulate drawing it above the screen
                yPos += textHeight(&bee->font, bee->text[idx], bee->disp->w - 26, INT16_MAX);
            }

            // Always update the idx and cursor position, even if the text wasn't drawn
            idx = (idx + 1) % bee->textLines;
        }
    }
}

/**
 * @brief Bee button callback, either speed up, reverse, or exit bee stuff
 *
 * @param evt The button event
 */
void beeButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            {
                // Scroll faster
                bee->scrollMod = 4;
                break;
            }
            case DOWN:
            {
                // Scroll faster, backwards
                bee->scrollMod = -4;
                break;
            }
            case BTN_A:
            case BTN_B:
            case LEFT:
            case RIGHT:
            case START:
            {
                break;
            }
            case SELECT:
            {
                beeSetRandomText();
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
                bee->scrollMod = 1;
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
