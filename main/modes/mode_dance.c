/*
 * mode_dance.c
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_sleep.h>

#include "hdw-tft.h"
#include "mode_dance.h"
#include "embeddedout.h"
#include "bresenham.h"
#include "swadge_util.h"
#include "settingsManager.h"
#include "linked_list.h"
#include "nvs_manager.h"

/*============================================================================
 * Typedefs
 *==========================================================================*/

typedef void (*ledDance)(uint32_t, uint32_t, bool);

typedef struct
{
    ledDance func;
    uint32_t arg;
    char* name;
} ledDanceArg;

#define RGB_2_ARG(r,g,b) ((((r)&0xFF) << 16) | (((g)&0xFF) << 8) | (((b)&0xFF)))
#define ARG_R(arg) (((arg) >> 16)&0xFF)
#define ARG_G(arg) (((arg) >>  8)&0xFF)
#define ARG_B(arg) (((arg) >>  0)&0xFF)

#define DANCE_SPEED_MULT 8
#define DANCE_NORMAL_SPEED_INDEX 5

// Sleep the TFT after 5s
#define TFT_TIMEOUT_US 5000000

/*============================================================================
 * Prototypes
 *==========================================================================*/

uint32_t danceRand(uint32_t upperBound);
void danceRedrawScreen(void);
void selectNextDance(void);
void selectPrevDance(void);
void danceBatteryCb(uint32_t vBatt);

/*============================================================================
 * Variables
 *==========================================================================*/
const char ledDancesExitText[] = "Exit: Start + Select";

static const uint8_t danceSpeeds[] =
{
    64, // 1/8x
    48, // 1/6x
    32, // 1/4x
    24, // 1/3x
    16, // 1/2x
    8, // 1x
    6, // 1.5x
    4, // 2x
    2, // 4x
};

static const ledDanceArg ledDances[] =
{
    {.func = danceComet, .arg = RGB_2_ARG(0, 0, 0),    .name = "Comet RGB"},
    {.func = danceComet, .arg = RGB_2_ARG(0xFF, 0, 0), .name = "Comet R"},
    {.func = danceComet, .arg = RGB_2_ARG(0, 0xFF, 0), .name = "Comet G"},
    {.func = danceComet, .arg = RGB_2_ARG(0, 0, 0xFF), .name = "Comet B"},
    {.func = danceRise,  .arg = RGB_2_ARG(0, 0, 0),    .name = "Rise RGB"},
    {.func = danceRise,  .arg = RGB_2_ARG(0xFF, 0, 0), .name = "Rise R"},
    {.func = danceRise,  .arg = RGB_2_ARG(0, 0xFF, 0), .name = "Rise G"},
    {.func = danceRise,  .arg = RGB_2_ARG(0, 0, 0xFF), .name = "Rise B"},
    {.func = dancePulse, .arg = RGB_2_ARG(0, 0, 0),    .name = "Pulse RGB"},
    {.func = dancePulse, .arg = RGB_2_ARG(0xFF, 0, 0), .name = "Pulse R"},
    {.func = dancePulse, .arg = RGB_2_ARG(0, 0xFF, 0), .name = "Pulse G"},
    {.func = dancePulse, .arg = RGB_2_ARG(0, 0, 0xFF), .name = "Pulse B"},
    {.func = danceSharpRainbow,  .arg = 0, .name = "Rainbow Sharp"},
    {.func = danceSmoothRainbow, .arg = 20000, .name = "Rainbow Slow"},
    {.func = danceSmoothRainbow, .arg =  4000, .name = "Rainbow Fast"},
    {.func = danceRainbowSolid,  .arg = 0, .name = "Rainbow Solid"},
    {.func = danceFire, .arg = RGB_2_ARG(0xFF, 51, 0), .name = "Fire R"},
    {.func = danceFire, .arg = RGB_2_ARG(0, 0xFF, 51), .name = "Fire G"},
    {.func = danceFire, .arg = RGB_2_ARG(51, 0, 0xFF), .name = "Fire B"},
    {.func = danceBinaryCounter, .arg = 0, .name = "Binary"},
    {.func = dancePoliceSiren,   .arg = 0, .name = "Siren"},
    {.func = dancePureRandom,    .arg = 0, .name = "Random LEDs"},
    {.func = danceChristmas,     .arg = 1, .name = "Holiday 1"},
    {.func = danceChristmas,     .arg = 0, .name = "Holiday 2"},
    {.func = danceFlashlight,    .arg = 0, .name = "Flashlight"},
    {.func = danceNone,          .arg = 0, .name = "None"},
    {.func = danceRandomDance,   .arg = 0, .name = "Shuffle All"},
};

typedef struct {
    const ledDanceArg* dance;
    bool enable;
} ledDanceOpt_t;

typedef struct portableDance_t {
    // List of dances to loop through
    ledDanceOpt_t dances[sizeof(ledDances) / sizeof(ledDanceArg)];

    // Set when dance needs to be reset
    bool resetDance;

    uint8_t danceIndex;

    // If non-NULL, the ddance index will be saved/loaded from this nvs key
    const char* nvsKey;
} portableDance_t;

typedef struct
{
    display_t* disp;

    uint8_t danceIdx;
    uint8_t danceSpeed;

    bool resetDance;
    bool blankScreen;

    uint64_t buttonPressedTimer;

    font_t infoFont;
    wsg_t arrow;
} danceMode_t;


swadgeMode modeDance =
{
    .modeName = "Light Dances",
    .fnEnterMode = danceEnterMode,
    .fnExitMode = danceExitMode,
    .fnMainLoop = danceMainLoop,
    .fnButtonCallback = danceButtonCb,
    .fnTouchCallback = danceTouchCb,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
    .fnBatteryCallback = danceBatteryCb,
    .overrideUsb = false
};

void portableDanceLoadSetting(portableDance_t* dance);

danceMode_t* danceState;

/*============================================================================
 * Functions
 *==========================================================================*/

/*
 * Portable Dance Functionality
 */
portableDance_t* initPortableDance(const char* nvsKey)
{
    portableDance_t* dance = calloc(1 ,sizeof(portableDance_t));
    for (uint8_t i = 0; i < sizeof(dance->dances) / sizeof(dance->dances[0]); i++)
    {
        dance->dances[i].dance = ledDances + i;
        dance->dances[i].enable = true;
    }

    dance->resetDance = true;

    if (nvsKey != NULL)
    {
        dance->nvsKey = nvsKey;
        portableDanceLoadSetting(dance);
    }

    return dance;
}

void freePortableDance(portableDance_t* dance)
{
    if (dance != NULL)
    {
        free(dance);
    }
}

void portableDanceMainLoop(portableDance_t* dance, int64_t elapsedUs)
{
    dance->dances[dance->danceIndex].dance->func((int32_t)elapsedUs, dance->dances[dance->danceIndex].dance->arg, dance->resetDance);
    dance->resetDance = false;
}

void portableDanceLoadSetting(portableDance_t* dance)
{
    int32_t danceIndex = 0;
    if (!readNvs32(dance->nvsKey, &danceIndex))
    {
        writeNvs32(dance->nvsKey, danceIndex);
    }

    if (danceIndex < 0)
    {
        danceIndex = 0;
    }
    else if (danceIndex >= getNumDances())
    {
        danceIndex = getNumDances() - 1;
    }

    dance->danceIndex = (uint8_t)danceIndex;
}

bool portableDanceSetByName(portableDance_t* dance, const char* danceName)
{
    for (uint8_t i = 0; i < getNumDances(); i++)
    {
        if (!strcmp(dance->dances[i].dance->name, danceName))
        {
            dance->danceIndex = i;
            dance->resetDance = true;

            if (dance->nvsKey != NULL)
            {
                writeNvs32(dance->nvsKey, dance->danceIndex);
            }
            return true;
        }
    }
    return false;
}

void portableDanceNext(portableDance_t* dance)
{
    uint8_t originalIndex = dance->danceIndex;

    do
    {
        if (dance->danceIndex + 1 >= getNumDances())
        {
            dance->danceIndex = 0;
        }
        else
        {
            dance->danceIndex++;
        }
    } while (!dance->dances[dance->danceIndex].enable && dance->danceIndex != originalIndex);

    dance->resetDance = true;

    if (dance->nvsKey != NULL)
    {
        writeNvs32(dance->nvsKey, dance->danceIndex);
    }
}

void portableDancePrev(portableDance_t* dance)
{
    uint8_t originalIndex = dance->danceIndex;
    do
    {
        if (dance->danceIndex == 0)
        {
            dance->danceIndex = getNumDances() - 1;
        }
        else
        {
            dance->danceIndex--;
        }
    } while (!dance->dances[dance->danceIndex].enable && dance->danceIndex != originalIndex);

    dance->resetDance = true;

    if (dance->nvsKey != NULL)
    {
        writeNvs32(dance->nvsKey, dance->danceIndex);
    }
}

bool portableDanceDisableDance(portableDance_t* dance, const char* danceName)
{
    for (uint8_t i = 0; i < getNumDances(); i++)
    {
        if (!strcmp(dance->dances[i].dance->name, danceName))
        {
            dance->dances[i].enable = false;
            return true;
        }
    }

    return false;
}

const char* portableDanceGetName(portableDance_t* dance)
{
    return dance->dances[dance->danceIndex].dance->name;
}


void danceEnterMode(display_t* disp)
{
    danceState = calloc(1, sizeof(danceMode_t));

    danceState->disp = disp;

    danceState->danceIdx = 0;
    danceState->danceSpeed = DANCE_NORMAL_SPEED_INDEX;

    danceState->resetDance = true;
    danceState->blankScreen = false;

    danceState->buttonPressedTimer = 0;

    loadFont("mm.font", &(danceState->infoFont));
    loadWsg("arrow21.wsg", &danceState->arrow);
}

void danceExitMode(void)
{
    if(danceState->blankScreen)
    {
        // Turn the screen on
        enableTFTBacklight();
        setTFTBacklight(getTftIntensity());
    }
    freeFont(&(danceState->infoFont));
    freeWsg(&danceState->arrow);
    free(danceState);
    danceState = NULL;
}

void danceMainLoop(int64_t elapsedUs)
{
    ledDances[danceState->danceIdx].func(elapsedUs * DANCE_SPEED_MULT / danceSpeeds[danceState->danceSpeed], ledDances[danceState->danceIdx].arg, danceState->resetDance);

    dancePollTouch();

    // If the screen is blank
    if(danceState->blankScreen)
    {
        // If a button has been pressed recently
        if(danceState->buttonPressedTimer < TFT_TIMEOUT_US)
        {
            // Turn the screen on
            enableTFTBacklight();
            setTFTBacklight(getTftIntensity());
            danceState->blankScreen = false;
            // Draw to it
            danceRedrawScreen();
        }
    }
    else
    {
        // Check if it should be blanked
        danceState->buttonPressedTimer += elapsedUs;
        if (danceState->buttonPressedTimer >= TFT_TIMEOUT_US)
        {
            disableTFTBacklight();
            danceState->blankScreen = true;
        }
        else
        {
            // Screen is not blank, draw to it
            danceRedrawScreen();
        }
    }

    danceState->resetDance = false;

    // Only sleep with a blank screen, otherwise the screen flickers
    if(danceState->blankScreen)
    {
        // Light sleep for 4ms. The longer the sleep, the choppier the LED animations
        // 4ms looks pretty good, though some LED timers do run faster (like 'rise')
        esp_sleep_enable_timer_wakeup(4000);
        esp_light_sleep_start();
    }
}

/**
 * @brief TODO
 *
 * @param vBatt
 */
void danceBatteryCb(uint32_t vBatt)
{
    // ESP_LOGI("BAT", "%lld %d", esp_timer_get_time(), vBatt);
}

void danceButtonCb(buttonEvt_t* evt)
{
    // Reset this on any button event
    danceState->buttonPressedTimer = 0;

    // This button press will wake the display, so don't process it
    if(danceState->blankScreen)
    {
        return;
    }

    if (evt->down)
    {
        switch(evt->button)
        {
            case UP:
            {
                incLedBrightness();
                break;
            }

            case DOWN:
            {
                decLedBrightness();
                break;
            }

            case LEFT:
            case BTN_B:
            {
                selectPrevDance();
                break;
            }

            case RIGHT:
            case BTN_A:
            {
                selectNextDance();
                break;
            }

            case SELECT:
            case START:
            {
                // Unused
                break;
            }
        }
    }
}

void danceTouchCb(touch_event_t* evt)
{
    dancePollTouch();
}

void dancePollTouch(void)
{
    int32_t centroid, intensity;
    if (getTouchCentroid(&centroid, &intensity))
    {
        uint8_t index = ((centroid * (sizeof(danceSpeeds) / sizeof(*danceSpeeds) - 1) + 512) / 1024);

        danceState->danceSpeed = index;
        danceState->buttonPressedTimer = 0;
    }
}

/**
 * @brief Blanks and redraws the entire screen
 */
void danceRedrawScreen(void)
{
    danceState->disp->clearPx();

    if (!danceState->blankScreen)
    {
        // Draw the name, perfectly centered
        int16_t yOff = (danceState->disp->h - danceState->infoFont.h) / 2;
        uint16_t width = textWidth(&(danceState->infoFont), ledDances[danceState->danceIdx].name);
        drawText(danceState->disp, &(danceState->infoFont), c555,
                 ledDances[danceState->danceIdx].name,
                 (danceState->disp->w - width) / 2,
                 yOff);
        // Draw some arrows
        drawWsg(danceState->disp, &danceState->arrow,
                ((danceState->disp->w - width) / 2) - 8 - danceState->arrow.w, yOff,
                false, false, 270);
        drawWsg(danceState->disp, &danceState->arrow,
                ((danceState->disp->w - width) / 2) + width + 8, yOff,
                false, false, 90);



        // Draw the brightness at the top
        char text[18];
        snprintf(text, sizeof(text), "Brightness: %d", getLedBrightness());
        width = textWidth(&(danceState->infoFont), text);
        yOff = 16;
        drawText(danceState->disp, &(danceState->infoFont), c555,
                 text,
                 (danceState->disp->w - width) / 2,
                 yOff);
        // Draw some arrows
        drawWsg(danceState->disp, &danceState->arrow,
                ((danceState->disp->w - width) / 2) - 8 - danceState->arrow.w, yOff,
                false, false, 0);
        drawWsg(danceState->disp, &danceState->arrow,
                ((danceState->disp->w - width) / 2) + width + 8, yOff,
                false, false, 180);

        // Draw the speed below the brightness
        yOff += danceState->infoFont.h + 16;
        if (danceSpeeds[danceState->danceSpeed] > DANCE_SPEED_MULT)
        {
            snprintf(text, sizeof(text), "Y~X: Speed: 1/%dx", danceSpeeds[danceState->danceSpeed] / DANCE_SPEED_MULT);
        }
        else
        {
            snprintf(text, sizeof(text), "Y~X: Speed: %dx",  DANCE_SPEED_MULT / danceSpeeds[danceState->danceSpeed]);
        }
        width = textWidth(&(danceState->infoFont), text);
        drawText(danceState->disp, &(danceState->infoFont), c555,
                text,
                (danceState->disp->w - width) / 2,
                yOff);

        // Draw text to show how to exit at the bottom
        width = textWidth(&(danceState->infoFont), ledDancesExitText);
        yOff = danceState->disp->h - danceState->infoFont.h - 16;
        drawText(danceState->disp, &(danceState->infoFont), c555,
                 ledDancesExitText,
                 (danceState->disp->w - width) / 2,
                 yOff);
    }
}

/** @return The number of different dances
 */
uint8_t getNumDances(void)
{
    return (sizeof(ledDances) / sizeof(ledDances[0]));
}

/**
 * @brief Get the Dance Name
 *
 * @param idx  The index of the dance
 * @return the dance name
 */
// char* getDanceName(uint8_t idx)
// {
//     return ledDances[idx].name;
// }

/** This is called to clear specific dance variables
 */
// void danceClearVars(uint8_t idx)
// {
//     // Reset the specific dance
//     ledDances[idx].func(0, ledDances[idx].arg, true);
// }

/** Get a random number from a range.
 *
 * This isn't true-random, unless bound is a power of 2. But it's close enough?
 * The problem is that rand() returns a number between [0, 2^64), and the
 * size of the range may not be even divisible by bound
 *
 * For what it's worth, this is what Arduino's random() does. It lies!
 *
 * @param bound An upper bound of the random range to return
 * @return A number in the range [0,bound), which does not include bound
 */
uint32_t danceRand(uint32_t bound)
{
    if(bound == 0)
    {
        return 0;
    }
    return rand() % bound;
}

/**
 * Rotate a single white LED around the swadge
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceComet(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static int32_t ledCount = 0;
    static uint8_t rainbow = 0;
    static int32_t msCount = 0;
    static uint32_t tAccumulated = 0;
    static led_t leds[NUM_LEDS] = {{0}};

    if(reset)
    {
        ledCount = 0;
        rainbow = 0;
        msCount = 80;
        tAccumulated = 2000;
        // TODO: is this right? had to transpose last 2 args
        memset(leds, 0, sizeof(leds));
        return;
    }

    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 2000)
    {
        tAccumulated -= 2000;
        for(uint8_t i = 0; i < NUM_LEDS; i++)
        {
            if(leds[i].r > 0)
            {
                leds[i].r--;
            }
            if(leds[i].g > 0)
            {
                leds[i].g--;
            }
            if(leds[i].b > 0)
            {
                leds[i].b--;
            }
        }
        msCount++;

        if(msCount % 10 == 0)
        {
            rainbow++;
        }

        if(msCount >= 80)
        {
            if(0 == arg)
            {
                int32_t color = EHSVtoHEXhelper(rainbow, 0xFF, 0xFF, false);
                leds[ledCount].r = (color >>  0) & 0xFF;
                leds[ledCount].g = (color >>  8) & 0xFF;
                leds[ledCount].b = (color >> 16) & 0xFF;
            }
            else
            {
                leds[ledCount].r = ARG_R(arg);
                leds[ledCount].g = ARG_G(arg);
                leds[ledCount].b = ARG_B(arg);
            }
            ledCount = (ledCount + 1) % NUM_LEDS;
            msCount = 0;
        }
        ledsUpdated = true;
    }

    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * Blink all LEDs red for on for 500ms, then off for 500ms
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void dancePulse(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static uint8_t ledVal = 0;
    static uint8_t randColor = 0;
    static bool goingUp = true;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledVal = 0;
        randColor = 0;
        goingUp = true;
        tAccumulated = 5000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 5000)
    {
        tAccumulated -= 5000;

        if(goingUp)
        {
            ledVal++;
            if(255 == ledVal)
            {
                goingUp = false;
            }
        }
        else
        {
            ledVal--;
            if(0 == ledVal)
            {
                goingUp = true;
                randColor = danceRand(256);
            }
        }

        for (int i = 0; i < NUM_LEDS; i++)
        {
            if(0 == arg)
            {
                int32_t color = EHSVtoHEXhelper(randColor, 0xFF, 0xFF, false);
                leds[i].r = (ledVal * ((color >>  0) & 0xFF) >> 8);
                leds[i].g = (ledVal * ((color >>  8) & 0xFF) >> 8);
                leds[i].b = (ledVal * ((color >> 16) & 0xFF) >> 8);
            }
            else
            {
                leds[i].r = (ledVal * ARG_R(arg)) >> 8;
                leds[i].g = (ledVal * ARG_G(arg)) >> 8;
                leds[i].b = (ledVal * ARG_B(arg)) >> 8;
            }
        }
        ledsUpdated = true;
    }

    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * Rotate a single white LED around the swadge
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceRise(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static int16_t levels[NUM_LEDS / 2] = {0, -256, -512};
    static bool rising[NUM_LEDS / 2] = {true, true, true};
    static uint8_t angle = 0;
    static uint32_t tAccumulated = 0;
    static uint8_t ledRemap[NUM_LEDS] = {1, 0, 2, 3, 4, 5, 7, 6};

    if(reset)
    {
        for(uint8_t i = 0; i < NUM_LEDS / 2; i++)
        {
            levels[i] = i * -256;
            rising[i] = true;
        }
        angle = 0;
        tAccumulated = 800;
        return;
    }

    bool ledsUpdated = false;
    led_t leds[NUM_LEDS] = {{0}};

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 800)
    {
        tAccumulated -= 800;

        if(true == rising[0] && 0 == levels[0])
        {
            angle = danceRand(256);
        }

        for(uint8_t i = 0; i < NUM_LEDS / 2; i++)
        {
            if(rising[i])
            {
                levels[i]++;
                if(levels[i] == 255)
                {
                    rising[i] = false;
                }
            }
            else
            {
                levels[i]--;
                if(levels[i] == -512)
                {
                    rising[i] = true;
                }
            }
        }

        if(0 == arg)
        {
            int32_t color = EHSVtoHEXhelper(angle, 0xFF, 0xFF, false);
            for(uint8_t i = 0; i < NUM_LEDS / 2; i++)
            {
                if(levels[i] > 0)
                {
                    leds[ledRemap[i]].r = (levels[i] * ((color >>  0) & 0xFF) >> 8);
                    leds[ledRemap[i]].g = (levels[i] * ((color >>  8) & 0xFF) >> 8);
                    leds[ledRemap[i]].b = (levels[i] * ((color >> 16) & 0xFF) >> 8);

                    leds[ledRemap[NUM_LEDS - 1 - i]].r = (levels[i] * ((color >>  0) & 0xFF) >> 8);
                    leds[ledRemap[NUM_LEDS - 1 - i]].g = (levels[i] * ((color >>  8) & 0xFF) >> 8);
                    leds[ledRemap[NUM_LEDS - 1 - i]].b = (levels[i] * ((color >> 16) & 0xFF) >> 8);
                }
            }
        }
        else
        {
            for(uint8_t i = 0; i < NUM_LEDS / 2; i++)
            {
                if(levels[i] > 0)
                {
                    leds[ledRemap[i]].r = (levels[i] * ARG_R(arg)) >> 8;
                    leds[ledRemap[i]].g = (levels[i] * ARG_G(arg)) >> 8;
                    leds[ledRemap[i]].b = (levels[i] * ARG_B(arg)) >> 8;

                    leds[ledRemap[NUM_LEDS - 1 - i]].r = (levels[i] * ARG_R(arg)) >> 8;
                    leds[ledRemap[NUM_LEDS - 1 - i]].g = (levels[i] * ARG_G(arg)) >> 8;
                    leds[ledRemap[NUM_LEDS - 1 - i]].b = (levels[i] * ARG_B(arg)) >> 8;
                }
            }
        }
        ledsUpdated = true;
    }

    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/** Smoothly rotate a color wheel around the swadge
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static uint32_t tAccumulated = 0;
    static uint8_t ledCount = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = arg;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= arg)
    {
        tAccumulated -= arg;
        ledsUpdated = true;

        ledCount--;

        uint8_t i;
        for(i = 0; i < NUM_LEDS; i++)
        {
            int16_t angle = ((((i * 256) / NUM_LEDS)) + ledCount) % 256;
            uint32_t color = EHSVtoHEXhelper(angle, 0xFF, 0xFF, false);

            leds[i].r = (color >>  0) & 0xFF;
            leds[i].g = (color >>  8) & 0xFF;
            leds[i].b = (color >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/** Sharply rotate a color wheel around the swadge
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceSharpRainbow(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 300000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 300000)
    {
        tAccumulated -= 300000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount > NUM_LEDS - 1)
        {
            ledCount = 0;
        }

        uint8_t i;
        for(i = 0; i < NUM_LEDS; i++)
        {
            int16_t angle = (((i * 256) / NUM_LEDS)) % 256;
            uint32_t color = EHSVtoHEXhelper(angle, 0xFF, 0xFF, false);

            leds[(i + ledCount) % NUM_LEDS].r = (color >>  0) & 0xFF;
            leds[(i + ledCount) % NUM_LEDS].g = (color >>  8) & 0xFF;
            leds[(i + ledCount) % NUM_LEDS].b = (color >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/** Counts up to 256 in binary. At 256, the color is held for ~3s
 * The 'on' color is smoothly iterated over the color wheel. The 'off'
 * color is also iterated over the color wheel, 180 degrees offset from 'on'
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceBinaryCounter(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount = 0;
    static int32_t ledCount2 = 0;
    static bool led_bool = false;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        ledCount2 = 0;
        led_bool = false;
        tAccumulated = 300000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 300000)
    {
        tAccumulated -= 300000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        ledCount2 = ledCount2 + 1;
        if(ledCount2 > 75)
        {
            led_bool = !led_bool;
            ledCount2 = 0;
        }
        if(ledCount > 255)
        {
            ledCount = 0;
        }
        int16_t angle = ledCount % 256;
        uint32_t colorOn = EHSVtoHEXhelper(angle, 0xFF, 0xFF, false);
        uint32_t colorOff = EHSVtoHEXhelper((angle + 128) % 256, 0xFF, 0xFF, false);

        uint8_t i;
        uint8_t j;
        for(i = 0; i < NUM_LEDS; i++)
        {
            if(ledCount2 >= (1 << NUM_LEDS))
            {
                leds[i].r = (colorOn >>  0) & 0xFF;
                leds[i].g = (colorOn >>  8) & 0xFF;
                leds[i].b = (colorOn >> 16) & 0xFF;
            }
            else
            {
                if(led_bool)
                {
                    j = NUM_LEDS - 1 - i;
                }
                else
                {
                    j = i;
                }

                if((ledCount2 >> i) & 1)
                {
                    leds[(j) % NUM_LEDS].r = (colorOn >>  0) & 0xFF;
                    leds[(j) % NUM_LEDS].g = (colorOn >>  8) & 0xFF;
                    leds[(j) % NUM_LEDS].b = (colorOn >> 16) & 0xFF;
                }
                else
                {
                    leds[(j) % NUM_LEDS].r = (colorOff >>  0) & 0xFF;
                    leds[(j) % NUM_LEDS].g = (colorOff >>  8) & 0xFF;
                    leds[(j) % NUM_LEDS].b = (colorOff >> 16) & 0xFF;
                }
            }
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * Fire pattern. All LEDs are random amount of red, and fifth that of green.
 * The LEDs towards the bottom have a brighter base and more randomness. The
 * LEDs towards the top are dimmer and have less randomness.
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceFire(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 100000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 75000)
    {
        tAccumulated -= 75000;
        ledsUpdated = true;

        uint8_t randC;

        // Base
        randC = danceRand(105) + 150;
        leds[1].r = (randC * ARG_R(arg)) / 256;
        leds[1].g = (randC * ARG_G(arg)) / 256;
        leds[1].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(105) + 150;
        leds[6].r = (randC * ARG_R(arg)) / 256;
        leds[6].g = (randC * ARG_G(arg)) / 256;
        leds[6].b = (randC * ARG_B(arg)) / 256;

        // Mid-low
        randC = danceRand(48) + 32;
        leds[0].r = (randC * ARG_R(arg)) / 256;
        leds[0].g = (randC * ARG_G(arg)) / 256;
        leds[0].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(48) + 32;
        leds[7].r = (randC * ARG_R(arg)) / 256;
        leds[7].g = (randC * ARG_G(arg)) / 256;
        leds[7].b = (randC * ARG_B(arg)) / 256;

        // Mid-high
        randC = danceRand(32) + 16;
        leds[2].r = (randC * ARG_R(arg)) / 256;
        leds[2].g = (randC * ARG_G(arg)) / 256;
        leds[2].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(32) + 16;
        leds[5].r = (randC * ARG_R(arg)) / 256;
        leds[5].g = (randC * ARG_G(arg)) / 256;
        leds[5].b = (randC * ARG_B(arg)) / 256;

        // Tip
        randC = danceRand(16) + 4;
        leds[3].r = (randC * ARG_R(arg)) / 256;
        leds[3].g = (randC * ARG_G(arg)) / 256;
        leds[3].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(16) + 4;
        leds[4].r = (randC * ARG_R(arg)) / 256;
        leds[4].g = (randC * ARG_G(arg)) / 256;
        leds[4].b = (randC * ARG_B(arg)) / 256;
    }
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/** police sirens, flash half red then half blue
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void dancePoliceSiren(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 120000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 120000)
    {
        tAccumulated -= 120000;
        ledsUpdated = true;

        // Skip to the next LED around the swadge
        ledCount = ledCount + 1;
        if(ledCount > NUM_LEDS)
        {
            ledCount = 0;

        }

        uint8_t i;
        if(ledCount < (NUM_LEDS >> 1))
        {
            //red
            for(i = 0; i < (NUM_LEDS >> 1); i++)
            {
                leds[i].r = 0xFF;
                leds[i].g = 0x00;
                leds[i].b = 0x00;
            }
        }
        else
        {
            //blue
            for(i = (NUM_LEDS >> 1); i < NUM_LEDS; i++)
            {
                leds[i].r = 0x00;
                leds[i].g = 0x00;
                leds[i].b = 0xFF;
            }
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/** Turn a random LED on to a random color, one at a time
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void dancePureRandom(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static uint32_t tAccumulated = 0;
    static uint8_t randLedMask = 0;
    static uint32_t randColor = 0;
    static uint8_t ledVal = 0;
    static bool ledRising = true;
    static uint32_t randInterval = 5000;

    if(reset)
    {
        randInterval = 5000;
        tAccumulated = randInterval;
        randLedMask = 0;
        randColor = 0;
        ledVal = 0;
        ledRising = true;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= randInterval)
    {
        tAccumulated -= randInterval;

        if(0 == ledVal)
        {
            randColor = danceRand(256);
            randLedMask = danceRand(1 << NUM_LEDS);
            randInterval = 500 + danceRand(4096);
            ledVal++;
        }
        else if(ledRising)
        {
            ledVal++;
            if(255 == ledVal)
            {
                ledRising = false;
            }
        }
        else
        {
            ledVal--;
            if(0 == ledVal)
            {
                ledRising = true;
            }
        }

        ledsUpdated = true;
        uint32_t color = EHSVtoHEXhelper(randColor, 0xFF, ledVal, false);
        for(uint8_t i = 0; i < NUM_LEDS; i++)
        {
            if((1 << i) & randLedMask)
            {
                leds[i].r = (color >>  0) & 0xFF;
                leds[i].g = (color >>  8) & 0xFF;
                leds[i].b = (color >> 16) & 0xFF;
            }
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * Turn on all LEDs and smooth iterate their singular color around the color wheel
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceRainbowSolid(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount = 0;
    static int32_t color_save = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        color_save = 0;
        tAccumulated = 70000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 70000)
    {
        tAccumulated -= 70000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount > 255)
        {
            ledCount = 0;
        }
        int16_t angle = ledCount % 256;
        color_save = EHSVtoHEXhelper(angle, 0xFF, 0xFF, false);

        uint8_t i;
        for(i = 0; i < NUM_LEDS; i++)
        {
            leds[i].r = (color_save >>  0) & 0xFF;
            leds[i].g = (color_save >>  8) & 0xFF;
            leds[i].b = (color_save >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * Turn on all LEDs and Make Purely White
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceFlashlight(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 70000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 70000)
    {
        tAccumulated -= 70000;
        ledsUpdated = true;

        uint8_t i;
        for(i = 0; i < NUM_LEDS; i++)
        {
            leds[i].r = 0xFF;
            leds[i].g = 0xFF;
            leds[i].b = 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * Pick a random dance mode and call it at its period for 4.5s. Then pick
 * another random dance and repeat
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void danceRandomDance(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t random_choice = -1;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        random_choice = -1;
        tAccumulated = 4500000;
        return;
    }

    if(-1 == random_choice)
    {
        random_choice = danceRand(getNumDances() - 3); // exclude the random mode, excluding random & none
    }

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 4500000)
    {
        tAccumulated -= 4500000;
        random_choice = danceRand(getNumDances() - 3); // exclude the random & none mode
        ledDances[random_choice].func(0, ledDances[random_choice].arg, true);
    }

    ledDances[random_choice].func(tElapsedUs, ledDances[random_choice].arg, false);
}

/** Holiday lights. Picks random target hues (red or green) or (blue or yellow) and saturations for
 * random LEDs at random intervals, then smoothly iterates towards those targets.
 * All LEDs are shown with a randomness added to their brightness for a little
 * sparkle
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param arg        unused
 * @param reset      true to reset this dance's variables
 */
void danceChristmas(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static int32_t ledCount = 0;
    static int32_t ledCount2 = 0;
    static uint8_t color_hue_save[NUM_LEDS] = {0};
    static uint8_t color_saturation_save[NUM_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t current_color_hue[NUM_LEDS] = {0};
    static uint8_t current_color_saturation[NUM_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t target_value[NUM_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t current_value[NUM_LEDS] = {0};

    static uint32_t tAccumulated = 0;
    static uint32_t tAccumulatedValue = 0;

    if(reset)
    {
        ledCount = 0;
        ledCount2 = 0;
        memset(color_saturation_save, 0xFF, sizeof(color_saturation_save));
        memset(current_color_saturation, 0xFF, sizeof(current_color_saturation));
        memset(target_value, 0xFF, sizeof(target_value));
        memset(current_value, 0x00, sizeof(current_value));
        if(arg)
        {
            memset(color_hue_save, 0, sizeof(color_hue_save));
            memset(current_color_hue, 0, sizeof(current_color_hue)); // All red
        }
        else
        {
            memset(color_hue_save, 171, sizeof(color_hue_save));
            memset(current_color_hue, 171, sizeof(current_color_hue)); // All blue
        }
        tAccumulated = 0;
        tAccumulatedValue = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LEDS] = {{0}};
    bool ledsUpdated = false;

    // Run a faster loop for LED brightness updates, this gives a twinkling effect
    tAccumulatedValue += tElapsedUs;
    while(tAccumulatedValue > 3500)
    {
        tAccumulatedValue -= 3500;

        uint8_t i;
        for(i = 0; i < NUM_LEDS; i++)
        {
            if(current_value[i] == target_value[i])
            {
                if(0xFF == target_value[i])
                {
                    // Reached full bright, pick new target value
                    target_value[i] = danceRand(64) + 192;
                }
                else
                {
                    // Reached target value, reset target to full bright
                    target_value[i] = 0xFF;
                }
            }
            // Smoothly move to the target value
            else if(current_value[i] > target_value[i])
            {
                current_value[i] -= 1;
            }
            else if (current_value[i] < target_value[i])
            {
                current_value[i] += 1;
            }
        }
    }

    // Run a slower loop for hue and saturation updates
    tAccumulated += tElapsedUs;
    while(tAccumulated > 7000)
    {
        tAccumulated -= 7000;

        ledCount += 1;
        if(ledCount > ledCount2)
        {
            ledCount = 0;
            ledCount2 = danceRand(1000) + 50; // 350ms to 7350ms
            int color_picker = danceRand(NUM_LEDS - 1);
            int node_select = danceRand(NUM_LEDS);

            if (color_picker < 4)
            {
                // Flip some color targets
                if(arg)
                {
                    if(color_hue_save[node_select] == 0) // red
                    {
                        color_hue_save[node_select] = 86; // green
                    }
                    else
                    {
                        color_hue_save[node_select] = 0; // red
                    }
                }
                else
                {
                    if(color_hue_save[node_select] == 171) // blue
                    {
                        color_hue_save[node_select] = 43; // yellow
                    }
                    else
                    {
                        color_hue_save[node_select] = 171; // blue
                    }
                }
                // Pick a random saturation target
                color_saturation_save[node_select] = danceRand(15) + 240;
            }
            else
            {
                // Whiteish target
                color_saturation_save[node_select] = danceRand(25);
            }
        }

        uint8_t i;
        for(i = 0; i < NUM_LEDS; i++)
        {
            // Smoothly move hue to the target
            if(current_color_hue[i] > color_hue_save[i])
            {
                current_color_hue[i] -= 1;
            }
            else if (current_color_hue[i] < color_hue_save[i])
            {
                current_color_hue[i] += 1;
            }

            // Smoothly move saturation to the target
            if(current_color_saturation[i] > color_saturation_save[i])
            {
                current_color_saturation[i] -= 1;
            }
            else if (current_color_saturation[i] < color_saturation_save[i])
            {
                current_color_saturation[i] += 1;
            }
        }

        // Calculate actual LED values
        for(i = 0; i < NUM_LEDS; i++)
        {
            leds[i].r = (EHSVtoHEXhelper(current_color_hue[i], current_color_saturation[i], current_value[i], false) >>  0) & 0xFF;
            leds[i].g = (EHSVtoHEXhelper(current_color_hue[i], current_color_saturation[i], current_value[i], false) >>  8) & 0xFF;
            leds[i].b = (EHSVtoHEXhelper(current_color_hue[i], current_color_saturation[i], current_value[i], false) >> 16) & 0xFF;
        }
        ledsUpdated = true;
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * @brief Blank the LEDs
 *
 * @param tElapsedUs
 * @param arg
 * @param reset
 */
void danceNone(uint32_t tElapsedUs __attribute__((unused)),
               uint32_t arg __attribute__((unused)), bool reset)
{
    if(reset)
    {
        led_t leds[NUM_LEDS] = {{0}};
        setLeds(leds, NUM_LEDS);
    }
}

/**
 * @brief Switches to the previous dance in the list, with wrapping
 */
void selectPrevDance(void)
{
    if (danceState->danceIdx > 0)
    {
        danceState->danceIdx--;
    }
    else
    {
        danceState->danceIdx = getNumDances() - 1;
    }

    danceState->resetDance = true;
}

/**
 * @brief Switches to the next dance in the list, with wrapping
 */
void selectNextDance(void)
{
    if (danceState->danceIdx < getNumDances() - 1)
    {
        danceState->danceIdx++;
    }
    else
    {
        danceState->danceIdx = 0;
    }

    danceState->resetDance = true;
}
