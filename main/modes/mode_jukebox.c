/*
 * mode_jukebox.c
 *
 *  Created on: 27 Oct 2022
 *      Author: VanillyNeko#4169 & Brycey92#9215
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "display.h"
#include "led_util.h"
#include "mode_main_menu.h"
#include "musical_buzzer.h"
#include "settingsManager.h"
#include "swadgeMode.h"
#include "swadge_util.h"

#include "mode_jukebox.h"
#include "meleeMenu.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define CORNER_OFFSET 12

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/*==============================================================================
 * Enums
 *============================================================================*/



/*==============================================================================
 * Prototypes
 *============================================================================*/

void  jukeboxEnterMode(display_t* disp);
void  jukeboxExitMode(void);
void  jukeboxButtonCallback(buttonEvt_t* evt);
void  jukeboxMainLoop(int64_t elapsedUs);
void  jukeboxMainMenuCb(const char* opt);

void jukeboxDanceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset);

/*==============================================================================
 * Structs
 *============================================================================*/

typedef void (*jukeboxLedDance)(uint32_t, uint32_t, bool);

typedef struct
{
    jukeboxLedDance func;
    uint32_t arg;
    char* name;
} jukeboxLedDanceArg;

/*==============================================================================
 * Variables
 *============================================================================*/

// Text
static const char str_exit[] = "Exit";

static const jukeboxLedDanceArg jukeboxLedDances[]=
{
    {.func = jukeboxDanceSmoothRainbow, .arg =  4000, .name = "Rainbow Fast"},
    {.func = jukeboxDanceSmoothRainbow, .arg = 20000, .name = "Rainbow Slow"},
};

swadgeMode modeJukebox =
{
    .modeName = "Jukebox",
    .fnEnterMode = jukeboxEnterMode,
    .fnExitMode = jukeboxExitMode,
    .fnButtonCallback = jukeboxButtonCallback,
    .fnTouchCallback = NULL,
    .fnMainLoop = jukeboxMainLoop,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .overrideUsb = false
};

// The state data
typedef enum
{
    JUKEBOX_MENU,
    JUKEBOX_PLAYER
} jukeboxScreen_t;


typedef struct
{
    display_t* disp;

    font_t ibm_vga8;
    font_t radiostars;
    font_t mm;

    uint8_t danceIdx;

    bool resetDance;

    meleeMenu_t* menu;
    jukeboxScreen_t screen;    
} jukebox_t;

jukebox_t* jukebox;

/*==============================================================================
 * Const Variables
 *============================================================================*/

// Text
static const char str_jukebox[]  = "Jukebox";
const char jukeboxMutedText[] =  "Swadge is muted!";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for jukebox
 */
void  jukeboxEnterMode(display_t* disp)
{
    // Allocate zero'd memory for the mode
    jukebox = calloc(1, sizeof(jukebox_t));

    jukebox->disp = disp;

    loadFont("ibm_vga8.font", &jukebox->ibm_vga8);
    loadFont("radiostars.font", &jukebox->radiostars);
    loadFont("mm.font", &jukebox->mm);

    jukebox->menu = initMeleeMenu(str_jukebox, &(jukebox->mm), jukeboxMainMenuCb);

    setJukeboxMainMenu();

    jukebox->screen = JUKEBOX_MENU;

    stopNote();
}

/**
 * Called when jukebox is exited
 */
void  jukeboxExitMode(void)
{
    stopNote();

    freeFont(&jukebox->ibm_vga8);
    freeFont(&jukebox->radiostars);
    freeFont(&jukebox->mm);

    free(jukebox);
}

/**
 * @brief Button callback function
 *
 * @param evt The button event that occurred
 */
void  jukeboxButtonCallback(buttonEvt_t* evt)
{
    switch(jukebox->screen)
    {
        case JUKEBOX_MENU:
        {
            if (evt->down)
            {
                meleeMenuButton(jukebox->menu, evt->button);
            }
            break;
        }
    }
}

/**
 * Update the display by drawing the current state of affairs
 */
void  jukeboxMainLoop(int64_t elapsedUs)
{
    jukeboxLedDances[jukebox->danceIdx].func(elapsedUs, jukeboxLedDances[jukebox->danceIdx].arg, jukebox->resetDance);

    jukebox->disp->clearPx();
    //fillDisplayArea(jukebox->disp, 0, 0, jukebox->disp->w, jukebox->disp->h, c010);
    switch(jukebox->screen)
    {
        case JUKEBOX_MENU:
        {
            //drawMeleeMenu(jukebox->disp, jukebox->menu);
            break;
        }
    }


    // Warn the user that the swadge is muted, if that's the case
    if(getSfxIsMuted())
    {
        drawText(
            jukebox->disp,
            &jukebox->radiostars, c551,
            jukeboxMutedText,
            (jukebox->disp->w - textWidth(&jukebox->radiostars, jukeboxMutedText)) / 2,
            jukebox->disp->h / 2);
    }
}

void setJukeboxMainMenu(void)
{
    resetMeleeMenu(jukebox->menu, str_jukebox, jukeboxMainMenuCb);
    addRowToMeleeMenu(jukebox->menu, str_exit);

    jukebox->screen = JUKEBOX_MENU;
}

void jukeboxMainMenuCb(const char * opt)
{
    if (opt == str_exit)
    {
        switchToSwadgeMode(&modeMainMenu);
        return;
    }
}

/** Smoothly rotate a color wheel around the swadge
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void jukeboxDanceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset)
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
