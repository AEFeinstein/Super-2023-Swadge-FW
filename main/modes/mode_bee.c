//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "spiffs_txt.h"
#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "mode_bee.h"
#include "mode_main_menu.h"
#include "musical_buzzer.h"
#include "esp_random.h"

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
void beeFreeRandomText(void);

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t font;
    int64_t tElapsedUs;
    int8_t scrollMod;
    int32_t yOffset;
    char** text;
    uint16_t textLines;
    paletteColor_t textColor;
    int32_t cachedPos;
    int16_t cachedIdx, cachedHeight;
} bee_t;

typedef struct 
{
    const char * fname;
    paletteColor_t color;
} copypasta_t;

//==============================================================================
// Variables
//==============================================================================

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

const copypasta_t texts[] = 
{
    {.fname = "findthemall.txt",           .color = c505},
    {.fname = "alice.txt",                 .color = c425},
    {.fname = "festival.txt",              .color = c550},
    {.fname = "fitness.txt",               .color = c035},
    {.fname = "astleyparadox.txt",         .color = c555},
    {.fname = "blueeee.txt",               .color = c225},
    {.fname = "hypothetical.txt",          .color = c522},
    {.fname = "tolerancetest.txt",         .color = c415},
    {.fname = "tips.txt",                  .color = c530},
    {.fname = "godihategaminglaptops.txt", .color = c152},
    {.fname = "homegoblin.txt",            .color = c330},
    {.fname = "flareon.txt",               .color = c510},
    {.fname = "whoasked.txt",              .color = c454},
    {.fname = "banned.txt",                .color = c500},
    {.fname = "morbiustime.txt",           .color = c150},
    {.fname = "mikuoop.txt",               .color = c115},
    {.fname = "halfApress.txt",            .color = c035},
    {.fname = "codeOfConduct.txt",         .color = c555},
    {.fname = "hunter2.txt",               .color = c050},
    {.fname = "touchgrass.txt",            .color = c020},
    {.fname = "kensama.txt",               .color = c300},
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
    // Free any text first, just in case
    beeFreeRandomText();

    // Set initial variables
    bee->yOffset = bee->disp->h;
    bee->tElapsedUs = 0;
    bee->scrollMod = 1;

    bee->cachedIdx = 0;
    bee->cachedPos = 0;
    bee->cachedHeight = 0;

    // Pick random text
    uint8_t textIdx = esp_random() % ARRAY_SIZE(texts);

    // Load the text in one big string
    char * lText = loadTxt(texts[textIdx].fname);

    // Save the color
    bee->textColor = texts[textIdx].color;

    // Count the lines
    bee->textLines = 0;
    char * tmp = lText;
    do
    {
        if(*tmp == '\n')
        {
            bee->textLines++;
        }
    } while('\0' != *tmp++);

    // Allocate pointers for all the lines
    bee->text = (char **)heap_caps_calloc(bee->textLines, sizeof(char *), MALLOC_CAP_SPIRAM);

    // Split the text into lines
    tmp = lText;
    for(uint32_t i = 0; i < bee->textLines; i++)
    {
        // Find the end of the line
        char * newlineLoc = strchr(tmp, '\n');
        // Find the line length
        int32_t lineLen = ((intptr_t)newlineLoc - (intptr_t)tmp) + 1;
        // Allocate space for this line and copy it
        bee->text[i] = heap_caps_calloc(1, lineLen + 1, MALLOC_CAP_SPIRAM);
        memcpy(bee->text[i], tmp, lineLen);
        // Move to the next line
        tmp = newlineLoc + 1;
    }

    // Free the loaded text
    free(lText);
}

/**
 * @brief Free all currently allocated text
 * 
 */
void beeFreeRandomText(void)
{
    // If there is text to free
    if(NULL != bee->text)
    {
        // Free each line
        for(uint32_t i = 0; i < bee->textLines; i++)
        {
            free(bee->text[i]);
        }
        // Free pointers to the lines
        free(bee->text);

        // Null this for safety
        bee->text = NULL;
    }
}

/**
 * Exit the bee mode, free memory
 */
void beeExitMode(void)
{
    beeFreeRandomText();
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

        if (bee->scrollMod < 0)
        {
            if (bee->cachedIdx > 0)
            {
                bee->cachedIdx--;
                bee->cachedHeight = textHeight(&bee->font, bee->text[bee->cachedIdx], bee->disp->w - 26, INT16_MAX);
                bee->cachedPos -= bee->cachedHeight;
            }
        }

        // Clear first
        bee->disp->clearPx();

        // Draw names until the cursor is off the screen
        int32_t yPos = bee->cachedPos;
        int16_t idx = bee->cachedIdx;
        while((yPos + bee->yOffset) < bee->disp->h)
        {
            if((yPos + bee->yOffset) >= -((idx == bee->cachedIdx && bee->cachedHeight != 0) ? (bee->cachedHeight) : (textHeight(&bee->font, bee->text[idx], bee->disp->w - 26, INT16_MAX))))
            {
                // If the names have scrolled back to the start, reset the scroll vars
                if(0 == (yPos + bee->yOffset) && 0 == idx)
                {
                    bee->cachedIdx = 0;
                    bee->cachedPos = 0;
                    bee->cachedHeight = 0;
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
                // make things go a wee bit faster next loop
                if (idx != bee->cachedIdx || bee->cachedHeight == 0)
                {
                    bee->cachedHeight = textHeight(&bee->font, bee->text[idx], bee->disp->w - 26, INT16_MAX);
                }
                bee->cachedIdx = idx;
                bee->cachedPos = yPos;

                // Add the entire height of the text to yPos, to simulate drawing it above the screen
                yPos += bee->cachedHeight;
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
            case LEFT:
            case RIGHT:
            {
                break;
            }
            case START:
            {
                switchToSwadgeMode(&modeMainMenu);
                break;
            }
            case BTN_A:
            case BTN_B:
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
