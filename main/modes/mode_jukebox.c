/*
 * mode_jukebox.c
 *
 *  Created on: 27 Oct 2022
 *      Author: VanillyNeko#4169 & Bryce
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

#include "fighter_music.h"
#include "fighter_mp_result.h"
#include "mode_fighter.h"
#include "mode_tiltrads.h"
#include "mode_platformer.h"
#include "mode_jumper.h"
#include "mode_tunernome.h"
#include "mode_credits.h"
#include "mode_test.h"

#include "mode_jukebox.h"
#include "meleeMenu.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define CORNER_OFFSET 12

/// Helper macro to return an integer clamped within a range (MIN to MAX)
// #define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/*==============================================================================
 * Enums
 *============================================================================*/

// The state data
typedef enum
{
    JUKEBOX_MENU,
    JUKEBOX_PLAYER
} jukeboxScreen_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void  jukeboxEnterMode(display_t* disp);
void  jukeboxExitMode(void);
void  jukeboxButtonCallback(buttonEvt_t* evt);
void  jukeboxMainLoop(int64_t elapsedUs);
void  jukeboxMainMenuCb(const char* opt);

void jukeboxSelectNextDance(void);
void jukeboxSelectPrevDance(void);
void jukeboxDanceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset);
void jukeboxDanceNone(uint32_t tElapsedUs, uint32_t arg, bool reset);

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    display_t* disp;

    font_t ibm_vga8;
    font_t radiostars;
    font_t mm;
    wsg_t arrow;

    uint8_t danceIdx;
    bool resetDance;

    uint8_t categoryIdx;
    uint8_t songIdx;
    bool inMusicSubmode;

    meleeMenu_t* menu;
    jukeboxScreen_t screen;    
} jukebox_t;

jukebox_t* jukebox;

typedef void (*jukeboxLedDance)(uint32_t, uint32_t, bool);

typedef struct
{
    jukeboxLedDance func;
    uint32_t arg;
    char* name;
} jukeboxLedDanceArg;

typedef struct
{
    char* name;
    const song_t* song;
} jukeboxSong;

typedef struct
{
    char* categoryName;
    const jukeboxSong* songs;
} jukeboxCategory;

/*==============================================================================
 * Variables
 *============================================================================*/

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

/*==============================================================================
 * Const Variables
 *============================================================================*/

// Text
static const char str_jukebox[]  = "Jukebox";
static const char str_bgm_muted[] =  "Swadge music is muted!";
static const char str_sfx_muted[] =  "Swadge SFX are muted!";
static const char str_bgm[] = "Music";
static const char str_sfx[] = "SFX";
static const char str_exit[] = "Exit";
static const char str_leds[] = "Sel: LEDs";
static const char str_back[] = "Start: Back";
static const char str_stop[] = ": Stop";
static const char str_play[] = ": Play";

static const jukeboxLedDanceArg jukeboxLedDances[] =
{
    {.func = jukeboxDanceSmoothRainbow, .arg =  4000, .name = "Rainbow Fast"},
    {.func = jukeboxDanceSmoothRainbow, .arg = 20000, .name = "Rainbow Slow"},
    {.func = jukeboxDanceNone,          .arg =     0, .name = "None"},
};

static const jukeboxSong fighterMusic[] =
{
    {.name = "Game", .song = &fighter_music},
};

static const jukeboxSong tiltradsMusic[] =
{
    {.name = "Title", .song = &titleMusic},
    {.name = "Game", .song = &gameMusic},
};

static const jukeboxSong platformerMusic[] =
{
    {.name = "Demagio", .song = &bgmDemagio},
    {.name = "Intro", .song = &bgmIntro},
    {.name = "Smooth", .song = &bgmSmooth},
};

static const jukeboxSong jumperMusic[] =
{
    {.name = "Death", .song = &jumpDeathTune},
    {.name = "Game Over", .song = &jumpGameOverTune},
    {.name = "Winner", .song = &jumpWinTune},
    {.name = "Game", .song = &jumpGameLoop},
    {.name = "Perfect Tune", .song = &jumpPerfectTune},
};

static const jukeboxSong creditsMusic[] =
{
    {.name = "Credits", .song = &creditsSong},
};

static const jukeboxSong testMusic[] =
{
    {.name = "BlackDog", .song = &BlackDog}
};

static const jukeboxCategory musicCategories[] =
{
    {.categoryName = "Swadge Bros", .songs = fighterMusic},
    {.categoryName = "Tiltrads", .songs = tiltradsMusic},
    {.categoryName = "Swadge Land", .songs = platformerMusic},
    {.categoryName = "Donut Jump", .songs = jumperMusic},
    {.categoryName = "Credits", .songs = creditsMusic},
    {.categoryName = "Test Mode", .songs = testMusic},
};

static const jukeboxSong fighterSfx[] =
{
    {.name = "Victory", .song = &fVictoryJingle},
    {.name = "Loss", .song = &fLossJingle},
};

static const jukeboxSong tiltradsSfx[] =
{
    {.name = "Game Start Sting", .song = &gameStartSting},
    {.name = "Line 1", .song = &lineOneSFX},
    {.name = "Line 2", .song = &lineTwoSFX},
    {.name = "Line 3", .song = &lineThreeSFX},
    {.name = "Line 4", .song = &lineFourSFX},
    {.name = "Line 5", .song = &lineFiveSFX},
    {.name = "Line 6", .song = &lineSixSFX},
    {.name = "Line 7", .song = &lineSevenSFX},
    {.name = "Line 8", .song = &lineEightSFX},
    {.name = "Line 9", .song = &lineNineSFX},
    {.name = "Line 10", .song = &lineTenSFX},
    {.name = "Line 11", .song = &lineElevenSFX},
    {.name = "Line 12", .song = &lineTwelveSFX},
    {.name = "Line 13", .song = &lineThirteenSFX},
    {.name = "Line 14", .song = &lineFourteenSFX},
    {.name = "Line 15", .song = &lineFifteenSFX},
    {.name = "Line 16", .song = &lineSixteenSFX},
    {.name = "Single Line Clear", .song = &singleLineClearSFX},
    {.name = "Double Line Clear", .song = &doubleLineClearSFX},
    {.name = "Triple Line Clear", .song = &tripleLineClearSFX},
    {.name = "Quadruple Line Clear", .song = &quadLineClearSFX},
    {.name = "Game Over Sting", .song = &gameOverSting},
};

static const jukeboxSong platformerSfx[] =
{
    {.name = "Menu Select", .song = &sndMenuSelect},
    {.name = "Menu Confirm", .song = &sndMenuConfirm},
    {.name = "Menu Deny", .song = &sndMenuDeny},
    {.name = "Game Start (Unused)", .song = &sndGameStart},
    {.name = "Jump 1", .song = &sndJump1},
    {.name = "Jump 2", .song = &sndJump2},
    {.name = "Jump 3", .song = &sndJump3},
    {.name = "Hurt", .song = &sndHurt},
    {.name = "Hit", .song = &sndHit},
    {.name = "Squish", .song = &sndSquish},
    {.name = "Break", .song = &sndBreak},
    {.name = "Coin", .song = &sndCoin},
    {.name = "Power Up", .song = &sndPowerUp},
    {.name = "Warp", .song = &sndWarp},
    {.name = "Wave Ball", .song = &sndWaveBall},
    {.name = "Death", .song = &sndDie},
};

static const jukeboxSong jumperSfx[] =
{
    {.name = "Jump", .song = &jumpPlayerJump},
    {.name = "Collect", .song = &jumperPlayerCollect},
    {.name = "Broke Combo", .song = &jumpPlayerBrokeCombo},
    {.name = "Countdown", .song = &jumpCountdown},
    {.name = "EvilDonut Jump", .song = &jumpEvilDonutJump},
    {.name = "Blump Jump", .song = &jumpBlumpJump},
};

static const jukeboxSong tunernomesfx[] =
{
    {.name = "Primary", .song = &metronome_primary},
    {.name = "Secondary", .song = &metronome_secondary},
};

static const jukeboxCategory sfxCategories[] =
{
    {.categoryName = "Swadge Bros", .songs = fighterSfx},
    {.categoryName = "Tiltrads", .songs = tiltradsSfx},
    {.categoryName = "Swadge Land", .songs = platformerSfx},
    {.categoryName = "Donut Jump", .songs = jumperSfx},
    {.categoryName = "Tunernome", .songs = tunernomesfx},
};

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

    loadWsg("arrow21.wsg", &jukebox->arrow);

    jukebox->menu = initMeleeMenu(str_jukebox, &(jukebox->mm), jukeboxMainMenuCb);

    setJukeboxMainMenu();

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

    freeWsg(&jukebox->arrow);

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
        case JUKEBOX_PLAYER:
        {
            if(evt->down)
            {
                switch(evt->button)
                {
                    case BTN_A:
                    {
                        if(jukebox->inMusicSubmode)
                        {
                            buzzer_play_bgm(musicCategories[jukebox->categoryIdx].songs[jukebox->songIdx].song);
                        }
                        else
                        {
                            buzzer_play_sfx(sfxCategories[jukebox->categoryIdx].songs[jukebox->songIdx].song);
                        }
                        break;
                    }
                    case BTN_B:
                    {
                        buzzer_stop();
                        break;
                    }
                    case SELECT:
                    {
                        jukeboxSelectNextDance();
                        break;
                    }
                    case START:
                    {
                        setJukeboxMainMenu();
                        break;
                    }
                    case UP:
                    {
                        uint8_t length;
                        if(jukebox->inMusicSubmode)
                        {
                            length = lengthof(musicCategories);
                        }
                        else
                        {
                            length = lengthof(sfxCategories);
                        }

                        if(jukebox->categoryIdx == 0)
                        {
                            jukebox->categoryIdx = length;
                        }
                        jukebox->categoryIdx = jukebox->categoryIdx - 1;

                        jukebox->songIdx = 0;
                        break;
                    }
                    case DOWN:
                    {
                        uint8_t length;
                        if(jukebox->inMusicSubmode)
                        {
                            length = lengthof(musicCategories);
                        }
                        else
                        {
                            length = lengthof(sfxCategories);
                        }

                        jukebox->categoryIdx = (jukebox->categoryIdx + 1) % length;

                        jukebox->songIdx = 0;
                        break;
                    }
                    case LEFT:
                    {
                        uint8_t length;
                        if(jukebox->inMusicSubmode)
                        {
                            length = lengthof(musicCategories[jukebox->categoryIdx].songs);
                        }
                        else
                        {
                            length = lengthof(sfxCategories[jukebox->categoryIdx].songs);
                        }

                        if(jukebox->songIdx == 0)
                        {
                            jukebox->songIdx = length;
                        }
                        jukebox->songIdx = jukebox->songIdx - 1;
                        break;
                    }
                    case RIGHT:
                    {
                        uint8_t length;
                        if(jukebox->inMusicSubmode)
                        {
                            length = lengthof(musicCategories[jukebox->categoryIdx].songs);
                        }
                        else
                        {
                            length = lengthof(sfxCategories[jukebox->categoryIdx].songs);
                        }

                        jukebox->songIdx = (jukebox->songIdx + 1) % length;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
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
    jukebox->resetDance = false;

    jukebox->disp->clearPx();
    switch(jukebox->screen)
    {
        case JUKEBOX_MENU:
        {
            drawMeleeMenu(jukebox->disp, jukebox->menu);
            break;
        }
        case JUKEBOX_PLAYER:
        {
            fillDisplayArea(jukebox->disp, 0, 0, jukebox->disp->w, jukebox->disp->h, c010);

            // Plot the button funcs
            // LEDs
            drawText(
                jukebox->disp,
                &jukebox->radiostars, c444,
                str_leds,
                CORNER_OFFSET,
                CORNER_OFFSET);
            
            // Back
            drawText(
                jukebox->disp,
                &jukebox->radiostars, c444,
                str_back,
                jukebox->disp->w - textWidth(&jukebox->radiostars, str_back) - CORNER_OFFSET,
                CORNER_OFFSET);

            // Stop
            int16_t afterText = drawText(
                                    jukebox->disp,
                                    &jukebox->radiostars, c511,
                                    "B",
                                    CORNER_OFFSET,
                                    jukebox->disp->h - jukebox->radiostars.h - CORNER_OFFSET);
            drawText(
                jukebox->disp,
                &jukebox->radiostars, c444,
                str_stop,
                afterText,
                jukebox->disp->h - jukebox->radiostars.h - CORNER_OFFSET);
            
            // Play
            afterText = drawText(
                            jukebox->disp,
                            &jukebox->radiostars, c151,
                            "A",
                            jukebox->disp->w - textWidth(&jukebox->radiostars, str_play) - textWidth(&jukebox->radiostars, "A") - CORNER_OFFSET,
                            jukebox->disp->h - jukebox->radiostars.h - CORNER_OFFSET);
            drawText(
                jukebox->disp,
                &jukebox->radiostars, c444,
                str_play,
                afterText,
                jukebox->disp->h - jukebox->radiostars.h - CORNER_OFFSET);


            
            char* categoryName;
            char* songName;
            char* songTypeName;
            if(jukebox->inMusicSubmode)
            {
                categoryName = musicCategories[jukebox->categoryIdx].categoryName;
                songName = musicCategories[jukebox->categoryIdx].songs[jukebox->songIdx].name;
                songTypeName = "Music";
            }
            else
            {
                categoryName = sfxCategories[jukebox->categoryIdx].categoryName;
                songName = sfxCategories[jukebox->categoryIdx].songs[jukebox->songIdx].name;
                songTypeName = "SFX";
            }

            // Draw the mode name
            char text[32];
            snprintf(text, sizeof(text), "Mode: %s", categoryName);
            int16_t width = textWidth(&(jukebox->mm), text);
            int16_t yOff = (jukebox->disp->h - jukebox->mm.h) / 2 - jukebox->mm.h * 2;
            drawText(jukebox->disp, &(jukebox->mm), c555,
                    text,
                    (jukebox->disp->w - width) / 2,
                    yOff);
            // Draw some arrows
            drawWsg(jukebox->disp, &jukebox->arrow,
                    ((jukebox->disp->w - width) / 2) - 8 - jukebox->arrow.w, yOff,
                    false, false, 0);
            drawWsg(jukebox->disp, &jukebox->arrow,
                    ((jukebox->disp->w - width) / 2) + width + 8, yOff,
                    false, false, 180);

            // Draw the song name
            snprintf(text, sizeof(text), "%s: %s", songTypeName, songName);
            yOff = (jukebox->disp->h - jukebox->mm.h) / 2 + jukebox->mm.h * 2;
            width = textWidth(&(jukebox->mm), text);
            drawText(jukebox->disp, &(jukebox->mm), c555,
                    text,
                    (jukebox->disp->w - width) / 2,
                    yOff);
            // Draw some arrows
            drawWsg(jukebox->disp, &jukebox->arrow,
                    ((jukebox->disp->w - width) / 2) - 8 - jukebox->arrow.w, yOff,
                    false, false, 270);
            drawWsg(jukebox->disp, &jukebox->arrow,
                    ((jukebox->disp->w - width) / 2) + width + 8, yOff,
                    false, false, 90);

            

            // Warn the user that the swadge is muted, if that's the case
            if(jukebox->inMusicSubmode)
            {
                if(getBgmIsMuted())
                {
                    drawText(
                        jukebox->disp,
                        &jukebox->radiostars, c551,
                        str_bgm_muted,
                        (jukebox->disp->w - textWidth(&jukebox->radiostars, str_bgm_muted)) / 2,
                        jukebox->disp->h / 2);
                }
            }
            else
            {
                if(getSfxIsMuted())
                {
                    drawText(
                        jukebox->disp,
                        &jukebox->radiostars, c551,
                        str_sfx_muted,
                        (jukebox->disp->w - textWidth(&jukebox->radiostars, str_sfx_muted)) / 2,
                        jukebox->disp->h / 2);
                }
            }
            break;
        }
    }
}

void setJukeboxMainMenu(void)
{
    resetMeleeMenu(jukebox->menu, str_jukebox, jukeboxMainMenuCb);
    addRowToMeleeMenu(jukebox->menu, str_bgm);
    addRowToMeleeMenu(jukebox->menu, str_sfx);
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
    else if (opt == str_bgm)
    {
        jukebox->screen = JUKEBOX_PLAYER;
        jukebox->categoryIdx = 1;
        jukebox->songIdx = 0;
        jukebox->inMusicSubmode = true;
    }
    else if (opt == str_sfx)
    {
        jukebox->screen = JUKEBOX_PLAYER;
        jukebox->categoryIdx = 1;
        jukebox->songIdx = 0;
        jukebox->inMusicSubmode = false;
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

/**
 * @brief Blank the LEDs
 *
 * @param tElapsedUs
 * @param arg
 * @param reset
 */
void jukeboxDanceNone(uint32_t tElapsedUs __attribute__((unused)),
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
void jukeboxSelectPrevDance(void)
{
    if (jukebox->danceIdx > 0)
    {
        jukebox->danceIdx--;
    }
    else
    {
        jukebox->danceIdx = lengthof(jukeboxLedDances) - 1;
    }

    jukebox->resetDance = true;
}

/**
 * @brief Switches to the next dance in the list, with wrapping
 */
void jukeboxSelectNextDance(void)
{
    if (jukebox->danceIdx < lengthof(jukeboxLedDances) - 1)
    {
        jukebox->danceIdx++;
    }
    else
    {
        jukebox->danceIdx = 0;
    }

    jukebox->resetDance = true;
}
