//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "spiffs_txt.h"
#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "mode_copypasta.h"
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

void copyPastaEnterMode(display_t* disp);
void copyPastaSetRandomText(void);
void copyPastaExitMode(void);
void copyPastaMainLoop(int64_t elapsedUs);
void copyPastaButtonCb(buttonEvt_t* evt);
void copyPastaFreeRandomText(void);

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
} copyPasta_t;

typedef struct 
{
    const char * fname;
    paletteColor_t color;
} copypasta_t;

//==============================================================================
// Variables
//==============================================================================

copyPasta_t* copyPasta;

swadgeMode modeCopyPasta =
{
    .modeName = "CopyPastas",
    .fnEnterMode = copyPastaEnterMode,
    .fnExitMode = copyPastaExitMode,
    .fnMainLoop = copyPastaMainLoop,
    .fnButtonCallback = copyPastaButtonCb,
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
 * Enter the copyPasta mode, allocate and initialize memory
 */
void copyPastaEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    copyPasta = (copyPasta_t*)calloc(1, sizeof(copyPasta_t));

    // Save a pointer to the display
    copyPasta->disp = disp;

    // Load some fonts
    loadFont("ibm_vga8.font", &copyPasta->font);

    copyPastaSetRandomText();
}

void copyPastaSetRandomText(void)
{
    // Free any text first, just in case
    copyPastaFreeRandomText();

    // Set initial variables
    copyPasta->yOffset = copyPasta->disp->h;
    copyPasta->tElapsedUs = 0;
    copyPasta->scrollMod = 1;

    copyPasta->cachedIdx = 0;
    copyPasta->cachedPos = 0;
    copyPasta->cachedHeight = 0;

    // Pick random text
    uint8_t textIdx = esp_random() % ARRAY_SIZE(texts);

    // Load the text in one big string
    char * lText = loadTxt(texts[textIdx].fname);

    // Save the color
    copyPasta->textColor = texts[textIdx].color;

    // Count the lines
    copyPasta->textLines = 0;
    char * tmp = lText;
    do
    {
        if(*tmp == '\n')
        {
            copyPasta->textLines++;
        }
    } while('\0' != *tmp++);

    // Allocate pointers for all the lines
    copyPasta->text = (char **)heap_caps_calloc(copyPasta->textLines, sizeof(char *), MALLOC_CAP_SPIRAM);

    // Split the text into lines
    tmp = lText;
    for(uint32_t i = 0; i < copyPasta->textLines; i++)
    {
        // Find the end of the line
        char * newlineLoc = strchr(tmp, '\n');
        // Find the line length
        int32_t lineLen = ((intptr_t)newlineLoc - (intptr_t)tmp) + 1;
        // Allocate space for this line and copy it
        copyPasta->text[i] = heap_caps_calloc(1, lineLen + 1, MALLOC_CAP_SPIRAM);
        memcpy(copyPasta->text[i], tmp, lineLen);
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
void copyPastaFreeRandomText(void)
{
    // If there is text to free
    if(NULL != copyPasta->text)
    {
        // Free each line
        for(uint32_t i = 0; i < copyPasta->textLines; i++)
        {
            free(copyPasta->text[i]);
        }
        // Free pointers to the lines
        free(copyPasta->text);

        // Null this for safety
        copyPasta->text = NULL;
    }
}

/**
 * Exit the copyPasta mode, free memory
 */
void copyPastaExitMode(void)
{
    copyPastaFreeRandomText();
    freeFont(&copyPasta->font);
    free(copyPasta);
}

/**
 * Main copyPasta loop, draw some scrolling copyPastas
 *
 * @param elapsedUs The time elapsed since the last call
 */
void copyPastaMainLoop(int64_t elapsedUs)
{
    copyPasta->tElapsedUs += elapsedUs;

    // If enough time has passed, translate and redraw text
    uint32_t updateTime = 100000 / ABS(copyPasta->scrollMod);
    if(copyPasta->tElapsedUs > updateTime)
    {
        copyPasta->tElapsedUs -= updateTime;

        // This static var tracks the vertical scrolling offset
        copyPasta->yOffset -= (copyPasta->scrollMod > 0) ? 1 : -1;

        if (copyPasta->scrollMod < 0)
        {
            if (copyPasta->cachedIdx > 0)
            {
                copyPasta->cachedIdx--;
                copyPasta->cachedHeight = textHeight(&copyPasta->font, copyPasta->text[copyPasta->cachedIdx], copyPasta->disp->w - 26, INT16_MAX);
                copyPasta->cachedPos -= copyPasta->cachedHeight;
            }
        }

        // Clear first
        copyPasta->disp->clearPx();

        // Draw names until the cursor is off the screen
        int32_t yPos = copyPasta->cachedPos;
        int16_t idx = copyPasta->cachedIdx;
        while((yPos + copyPasta->yOffset) < copyPasta->disp->h)
        {
            if((yPos + copyPasta->yOffset) >= -((idx == copyPasta->cachedIdx && copyPasta->cachedHeight != 0) ? (copyPasta->cachedHeight) : (textHeight(&copyPasta->font, copyPasta->text[idx], copyPasta->disp->w - 26, INT16_MAX))))
            {
                // If the names have scrolled back to the start, reset the scroll vars
                if(0 == (yPos + copyPasta->yOffset) && 0 == idx)
                {
                    copyPasta->cachedIdx = 0;
                    copyPasta->cachedPos = 0;
                    copyPasta->cachedHeight = 0;
                    copyPasta->yOffset = 0;
                    yPos = 0;
                }

                int16_t textX = 13, textY = (yPos + copyPasta->yOffset);

                // Draw the text
                drawTextWordWrap(copyPasta->disp, &copyPasta->font, copyPasta->textColor, copyPasta->text[idx],
                         &textX, &textY, copyPasta->disp->w - 13, copyPasta->disp->h + copyPasta->font.h);

                // Add the height of the drawn text plus one line to yPos
                yPos = textY - copyPasta->yOffset + copyPasta->font.h + LINE_SPACING;
            }
            else
            {
                // make things go a wee bit faster next loop
                if (idx != copyPasta->cachedIdx || copyPasta->cachedHeight == 0)
                {
                    copyPasta->cachedHeight = textHeight(&copyPasta->font, copyPasta->text[idx], copyPasta->disp->w - 26, INT16_MAX);
                }
                copyPasta->cachedIdx = idx;
                copyPasta->cachedPos = yPos;

                // Add the entire height of the text to yPos, to simulate drawing it above the screen
                yPos += copyPasta->cachedHeight;
            }

            // Always update the idx and cursor position, even if the text wasn't drawn
            idx = (idx + 1) % copyPasta->textLines;
        }
    }
}

/**
 * @brief copyPasta button callback, either speed up, reverse, or exit copyPastas
 *
 * @param evt The button event
 */
void copyPastaButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            {
                // Scroll faster
                copyPasta->scrollMod = 4;
                break;
            }
            case DOWN:
            {
                // Scroll faster, backwards
                copyPasta->scrollMod = -4;
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
                copyPastaSetRandomText();
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
                copyPasta->scrollMod = 1;
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
