//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "swadgeMode.h"
#include "swadge_esp32.h"
#include "mode_main_menu.h"

#include "settingsManager.h"

#include "fighter_menu.h"
#include "jumper_menu.h"
#include "meleeMenu.h"
#include "mode_bee.h"
#include "mode_colorchord.h"
#include "mode_credits.h"
#include "mode_dance.h"
#include "mode_diceroller.h"
#include "mode_flight.h"
#include "mode_gamepad.h"
#include "mode_jukebox.h"
#include "mode_paint.h"
#include "mode_picross.h"
#include "mode_platformer.h"
#include "mode_slide_whistle.h"
#include "mode_test.h"
#include "mode_tiltrads.h"
#include "mode_tunernome.h"
#include "picross_menu.h"
// #include "picross_select.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void mainMenuEnterMode(display_t* disp);
void mainMenuExitMode(void);
void mainMenuMainLoop(int64_t elapsedUs);
void mainMenuButtonCb(buttonEvt_t* evt);
void mainMenuCb(const char* opt);
void mainMenuBatteryCb(uint32_t vBatt);

void mainMenuSetUpTopMenu(bool);
void mainMenuTopLevelCb(const char* opt);
void mainMenuSetUpGamesMenu(bool);
void mainMenuGamesCb(const char* opt);
void mainMenuSetUpToolsMenu(bool);
void mainMenuToolsCb(const char* opt);
void mainMenuSetUpMusicMenu(bool);
void mainMenuMusicCb(const char* opt);
void mainMenuSetUpSettingsMenu(bool);
void mainMenuSettingsCb(const char* opt);
void mainMenuSetUpSecretMenu(bool);
void mainMenuSecretCb(const char* opt);

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t meleeMenuFont;
    font_t ibmFont;
    meleeMenu_t* menu;
    uint8_t topLevelPos;
    uint8_t gamesPos;
    uint8_t toolsPos;
    uint8_t musicPos;
    uint8_t settingsPos;
    uint8_t secretPos;
    int16_t btnState;
    int16_t prevBtnState;
    uint8_t menuSelection;
    uint32_t battVal;
    wsg_t batt[4];
    wsg_t usb;
    int32_t autoLightDanceTimer;
    bool debugMode;
    char gitStr[32];
} mainMenu_t;

//==============================================================================
// Variables
//==============================================================================

mainMenu_t* mainMenu;

swadgeMode modeMainMenu =
{
    .modeName = "mainMenu",
    .fnEnterMode = mainMenuEnterMode,
    .fnExitMode = mainMenuExitMode,
    .fnMainLoop = mainMenuMainLoop,
    .fnButtonCallback = mainMenuButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
    .fnBatteryCallback = mainMenuBatteryCb,
    .overrideUsb = false,
};

const char mainMenuTitle[] = "Swadge 1.1";
const char mainMenuGames[] = "Games";
const char mainMenuTools[] = "Tools";
const char mainMenuMusic[] = "Music";
const char mainMenuSettings[] = "Settings";
const char mainMenuSecret[] = "Secrets";
const char mainMenuBack[] = "Back";
const char mainMenuSoundBgmOn[] = "Music: On";
const char mainMenuSoundBgmOff[] = "Music: Off";
const char mainMenuSoundSfxOn[] = "SFX: On";
const char mainMenuSoundSfxOff[] = "SFX: Off";
char mainMenuTftBrightness[] = "TFT Brightness: 1";
char mainMenuLedBrightness[] = "LED Brightness: 1";
char mainMenuMicGain[] = "Mic Gain: 1";
char mainMenuScreensaverTimeout[] = "Screensaver: 20s";
char mainMenuScreensaverOff[] = "Screensaver: Off";
const char mainMenuCredits[] = "Credits";

static const int16_t cheatCode[11] = {UP, UP, DOWN, DOWN, LEFT, RIGHT, LEFT, RIGHT, BTN_B, BTN_A, START};

static const song_t secretSong =
{
    .notes =
    {
        {.note = G_5, .timeMs = 162},
        {.note = F_SHARP_5, .timeMs = 162},
        {.note = D_SHARP_5, .timeMs = 162},
        {.note = A_4, .timeMs = 174},
        {.note = G_SHARP_4, .timeMs = 162},
        {.note = E_5, .timeMs = 174},
        {.note = G_SHARP_5, .timeMs = 162},
        {.note = C_6, .timeMs = 372},

    },
    .numNotes = 8,
    .shouldLoop = false
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize and enter the main menu
 */
void mainMenuEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    mainMenu = (mainMenu_t*)calloc(1, sizeof(mainMenu_t));

    // Save a pointer to the display
    mainMenu->disp = disp;

    // Load the font
    loadFont("mm.font", &mainMenu->meleeMenuFont);
    loadFont("ibm_vga8.font", &mainMenu->ibmFont);

    // Load images
    loadWsg("batt1.wsg", &mainMenu->batt[0]);
    loadWsg("batt2.wsg", &mainMenu->batt[1]);
    loadWsg("batt3.wsg", &mainMenu->batt[2]);
    loadWsg("batt4.wsg", &mainMenu->batt[3]);
    loadWsg("usb.wsg", &mainMenu->usb);

    sprintf(mainMenu->gitStr, "Git: %s", GIT_SHA1);

    // Initialize the menu
    mainMenu->menu = initMeleeMenu(mainMenuTitle, &mainMenu->meleeMenuFont, mainMenuTopLevelCb);
    mainMenuSetUpTopMenu(true);
}

/**
 * Deinitialize and exit the main menu
 */
void mainMenuExitMode(void)
{
    freeWsg(&mainMenu->batt[0]);
    freeWsg(&mainMenu->batt[1]);
    freeWsg(&mainMenu->batt[2]);
    freeWsg(&mainMenu->batt[3]);
    freeWsg(&mainMenu->usb);
    deinitMeleeMenu(mainMenu->menu);
    freeFont(&mainMenu->meleeMenuFont);
    freeFont(&mainMenu->ibmFont);
    free(mainMenu);
}

/**
 * The main menu loop, draw the menu if there's been a difference
 *
 * @param elapsedUs unused
 */
void mainMenuMainLoop(int64_t elapsedUs)
{
    // Increment this timer
    mainMenu->autoLightDanceTimer += elapsedUs;
    // If 10s have elapsed with no user input
    if(getScreensaverTime() != 0 && mainMenu->autoLightDanceTimer >= (getScreensaverTime() * 1000000))
    {
        // Switch to the LED dance mode
        switchToSwadgeMode(&modeDance);
        return;
    }

    // Draw the menu
    drawMeleeMenu(mainMenu->disp, mainMenu->menu);

    // Draw the battery indicator depending on the last read value
    wsg_t * toDraw = NULL;
    // 872 is full
    if (mainMenu->battVal == 0 || mainMenu->battVal > 741)
    {
        toDraw = &mainMenu->batt[3];        
    }
    else if (mainMenu->battVal > 695)
    {
        toDraw = &mainMenu->batt[2];        
    }
    else if (mainMenu->battVal > 652)
    {
        toDraw = &mainMenu->batt[1];        
    }
    else // 452 is dead
    {
        toDraw = &mainMenu->batt[0];        
    }

    drawWsg(mainMenu->disp, toDraw, 212, 3, false, false, 0);
}

/**
 * @brief Save the battery voltage
 * 
 * @param vBatt The battery voltage
 */
void mainMenuBatteryCb(uint32_t vBatt)
{
    mainMenu->battVal = vBatt;
}

/**
 * Button callback for the main menu, pass the event to the menu
 *
 * @param evt
 */
void mainMenuButtonCb(buttonEvt_t* evt)
{
    // Any button event resets this timer
    mainMenu->autoLightDanceTimer = 0;

    mainMenu->prevBtnState = mainMenu->btnState;
    mainMenu->btnState = evt->state;

    if(evt->down)
    {
        if(!mainMenu->debugMode)
        {
            if  (
                    (mainMenu->btnState & cheatCode[mainMenu->menuSelection])
                    &&
                    !(mainMenu->prevBtnState & cheatCode[mainMenu->menuSelection])
                )
            {
                mainMenu->menuSelection++;

                if(mainMenu->menuSelection > 10)
                {
                    mainMenu->menuSelection = 0;
                    mainMenu->debugMode = true;
                    buzzer_play_bgm(&secretSong);
                    mainMenuSetUpSecretMenu(true);
                    return;
                }
            }
            else
            {
                mainMenu->menuSelection = 0;
            }
        }

        switch(evt->button)
        {
            case UP:
            case DOWN:
            case BTN_A:
            case START:
            case SELECT:
            {
                meleeMenuButton(mainMenu->menu, evt->button);
                break;
            }
            case LEFT:
            case RIGHT:
            {
                // If this is the settings menu
                if(mainMenuSettings == mainMenu->menu->title)
                {
                    // Save the position
                    mainMenu->settingsPos = mainMenu->menu->selectedRow;

                    // Adjust the selected option
                    if(mainMenuMicGain == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        if(LEFT == evt->button)
                        {
                            decMicGain();
                        }
                        else
                        {
                            incMicGain();
                        }
                    }
                    else if(mainMenuSoundBgmOn == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        // Music is on, turn it off
                        setBgmIsMuted(true);
                    }
                    else if(mainMenuSoundBgmOff == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        // Music is off, turn it on
                        setBgmIsMuted(false);
                    }
                    else if(mainMenuSoundSfxOn == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        // SFX is on, turn it off
                        setSfxIsMuted(true);
                    }
                    else if(mainMenuSoundSfxOff == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        // SFX is off, turn it on
                        setSfxIsMuted(false);
                    }
                    else if(mainMenuTftBrightness == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        if(LEFT == evt->button)
                        {
                            decTftBrightness();
                        }
                        else
                        {
                            incTftBrightness();
                        }
                    }
                    else if(mainMenuLedBrightness == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        if(LEFT == evt->button)
                        {
                            decLedBrightness();
                        }
                        else
                        {
                            incLedBrightness();
                        }
                    }
                    else if(mainMenuScreensaverOff == mainMenu->menu->rows[mainMenu->menu->selectedRow] ||
                            mainMenuScreensaverTimeout == mainMenu->menu->rows[mainMenu->menu->selectedRow])
                    {
                        if (LEFT == evt->button)
                        {
                            decScreensaverTime();
                        }
                        else
                        {
                            incScreensaverTime();
                        }
                    }
                    // Redraw menu options
                    mainMenuSetUpSettingsMenu(false);
                }
                break;
            }
            case BTN_B:
            {
                // If not on the main menu
                if(mainMenuTitle != mainMenu->menu->title)
                {
                    // Go back to the main menu
                    mainMenuSetUpTopMenu(false);
                    return;
                }
                break;
            }
        }
    }
}

/**
 * Set up the top level menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void mainMenuSetUpTopMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(mainMenu->menu, mainMenuTitle, mainMenuTopLevelCb);
    addRowToMeleeMenu(mainMenu->menu, mainMenuGames);
    addRowToMeleeMenu(mainMenu->menu, mainMenuTools);
    addRowToMeleeMenu(mainMenu->menu, mainMenuMusic);
    addRowToMeleeMenu(mainMenu->menu, mainMenuSettings);
    addRowToMeleeMenu(mainMenu->menu, mainMenuCredits);

    // Set the position
    if(resetPos)
    {
        mainMenu->topLevelPos = 0;
    }
    mainMenu->menu->selectedRow = mainMenu->topLevelPos;
}

/**
 * Callback for the top level menu
 *
 * @param opt The menu option which was selected
 */
void mainMenuTopLevelCb(const char* opt)
{
    // Save the position
    mainMenu->topLevelPos = mainMenu->menu->selectedRow;

    // Handle the option
    if(mainMenuGames == opt)
    {
        mainMenuSetUpGamesMenu(true);
    }
    else if(mainMenuTools == opt)
    {
        mainMenuSetUpToolsMenu(true);
    }
    else if(mainMenuMusic == opt)
    {
        mainMenuSetUpMusicMenu(true);
    }
    else if(mainMenuSettings == opt)
    {
        mainMenuSetUpSettingsMenu(true);
    }
    else if (mainMenuCredits == opt)
    {
        switchToSwadgeMode(&modeCredits);
    }
}

/**
 * Set up the games menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void mainMenuSetUpGamesMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(mainMenu->menu, mainMenuGames, mainMenuGamesCb);
    addRowToMeleeMenu(mainMenu->menu, modeFighter.modeName);
    addRowToMeleeMenu(mainMenu->menu, modePlatformer.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeJumper.modeName);
    addRowToMeleeMenu(mainMenu->menu, modePicross.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeFlight.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeTiltrads.modeName);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
    // Set the position
    if(resetPos)
    {
        mainMenu->gamesPos = 0;
    }
    mainMenu->menu->selectedRow = mainMenu->gamesPos;
}

/**
 * Callback for the games menu
 *
 * @param opt The menu option which was selected
 */
void mainMenuGamesCb(const char* opt)
{
    // Save the position
    mainMenu->gamesPos = mainMenu->menu->selectedRow;

    // Handle the option
    if(modeFighter.modeName == opt)
    {
        // Start fighter
        switchToSwadgeMode(&modeFighter);
    }
    else if(modeTiltrads.modeName == opt)
    {
        // Start tiltrads
        switchToSwadgeMode(&modeTiltrads);
    }
    else if(modePlatformer.modeName == opt)
    {
        // Start platformer
        switchToSwadgeMode(&modePlatformer);
    }
    else if(modeJumper.modeName == opt)
    {
        // Start jumper
        switchToSwadgeMode(&modeJumper);
    }
    else if(modePicross.modeName == opt)
    {
        switchToSwadgeMode(&modePicross);
    }
    else if(modeFlight.modeName == opt)
    {
        switchToSwadgeMode(&modeFlight);
    }
    else if(mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu(false);
    }
}

/**
 * Set up the tools menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void mainMenuSetUpToolsMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(mainMenu->menu, mainMenuTools, mainMenuToolsCb);
    addRowToMeleeMenu(mainMenu->menu, modeGamepad.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeDance.modeName);
    addRowToMeleeMenu(mainMenu->menu, modePaint.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeDiceRoller.modeName);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
    // Set the position
    if(resetPos)
    {
        mainMenu->toolsPos = 0;
    }
    mainMenu->menu->selectedRow = mainMenu->toolsPos;
}

/**
 * Callback for the tools menu
 *
 * @param opt The menu option which was selected
 */
void mainMenuToolsCb(const char* opt)
{
    // Save the position
    mainMenu->toolsPos = mainMenu->menu->selectedRow;

    // Handle the option
    if(modeGamepad.modeName == opt)
    {
        // Start gamepad
        switchToSwadgeMode(&modeGamepad);
    }
    else if(modeTunernome.modeName == opt)
    {
        // Start tunernome
        switchToSwadgeMode(&modeTunernome);
    }
    else if(modeColorchord.modeName == opt)
    {
        // Start Colorchord
        switchToSwadgeMode(&modeColorchord);
    }
    else if(modeDance.modeName == opt)
    {
        // Start Light Dances
        switchToSwadgeMode(&modeDance);
    }
    else if (modePaint.modeName == opt)
    {
        // Start Paint
        switchToSwadgeMode(&modePaint);
    }
    else if (modeDiceRoller.modeName == opt)
    {
        switchToSwadgeMode(&modeDiceRoller);
    }
    else if(mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu(false);
    }
}

/**
 * Set up the music menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void mainMenuSetUpMusicMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(mainMenu->menu, mainMenuMusic, mainMenuMusicCb);
    addRowToMeleeMenu(mainMenu->menu, modeColorchord.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeTunernome.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeSlideWhistle.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeJukebox.modeName);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
    // Set the position
    if(resetPos)
    {
        mainMenu->musicPos = 0;
    }
    mainMenu->menu->selectedRow = mainMenu->musicPos;
}

/**
 * Callback for the music menu
 *
 * @param opt The menu option which was selected
 */
void mainMenuMusicCb(const char* opt)
{
    // Save the position
    mainMenu->musicPos = mainMenu->menu->selectedRow;

    // Handle the option
    if(modeColorchord.modeName == opt)
    {
        // Start colorchord
        switchToSwadgeMode(&modeColorchord);
    }
    else if(modeTunernome.modeName == opt)
    {
        // Start tunernome
        switchToSwadgeMode(&modeTunernome);
    }
    else if(modeSlideWhistle.modeName == opt)
    {
        // Start slide whistle
        switchToSwadgeMode(&modeSlideWhistle);
    }
    else if(modeJukebox.modeName == opt)
    {
        // Start jukebox
        switchToSwadgeMode(&modeJukebox);
    }
    else if(mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu(false);
    }
}

/**
 * Set up the settings menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void mainMenuSetUpSettingsMenu(bool resetPos)
{
    // Pick the menu string based on the sound option
    const char* soundBgmOpt;
    if(getBgmIsMuted())
    {
        soundBgmOpt = mainMenuSoundBgmOff;
    }
    else
    {
        soundBgmOpt = mainMenuSoundBgmOn;
    }
    const char* soundSfxOpt;
    if(getSfxIsMuted())
    {
        soundSfxOpt = mainMenuSoundSfxOff;
    }
    else
    {
        soundSfxOpt = mainMenuSoundSfxOn;
    }

    // Print the tftBrightness option
    snprintf(mainMenuTftBrightness, sizeof(mainMenuTftBrightness), "TFT Brightness: %d", getTftBrightness());

    // Print the ledBrightness option
    snprintf(mainMenuLedBrightness, sizeof(mainMenuLedBrightness), "LED Brightness: %d", getLedBrightness());

    // Print the mic gain option
    snprintf(mainMenuMicGain, sizeof(mainMenuMicGain), "Mic Gain: %d", getMicGain());

    // Print the screensaver time option
    const char* screensaverMenuOpt;
    int16_t screensaverTime = getScreensaverTime();
    if (screensaverTime == 0)

    {
        screensaverMenuOpt = mainMenuScreensaverOff;
    } else
    {
        char screensaverUnit = (screensaverTime >= 60) ? 'm' : 's';
        int16_t screensaverDisplay = (screensaverTime >= 60) ? (screensaverTime / 60) : screensaverTime;
        snprintf(mainMenuScreensaverTimeout, sizeof(mainMenuScreensaverTimeout), "Screensaver: %d%c", abs(screensaverDisplay) % 100, screensaverUnit);
        screensaverMenuOpt = mainMenuScreensaverTimeout;
    }

    // Reset the menu
    resetMeleeMenu(mainMenu->menu, mainMenuSettings, mainMenuSettingsCb);
    addRowToMeleeMenu(mainMenu->menu, soundBgmOpt);
    addRowToMeleeMenu(mainMenu->menu, soundSfxOpt);
    addRowToMeleeMenu(mainMenu->menu, screensaverMenuOpt);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuTftBrightness);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuLedBrightness);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuMicGain);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
    // Set the position
    if(resetPos)
    {
        mainMenu->settingsPos = 0;
    }
    mainMenu->menu->selectedRow = mainMenu->settingsPos;
}

/**
 * Callback for the settings menu
 *
 * @param opt The menu option which was selected
 */
void mainMenuSettingsCb(const char* opt)
{
    // Save the position
    mainMenu->settingsPos = mainMenu->menu->selectedRow;

    // Handle the option
    if(mainMenuSoundBgmOff == opt)
    {
        // Sound is off, turn it on
        setBgmIsMuted(false);
    }
    else if (mainMenuSoundBgmOn == opt)
    {
        // Sound is on, turn it off
        setBgmIsMuted(true);
    }
    else if(mainMenuSoundSfxOff == opt)
    {
        // Sound is off, turn it on
        setSfxIsMuted(false);
    }
    else if (mainMenuSoundSfxOn == opt)
    {
        // Sound is on, turn it off
        setSfxIsMuted(true);
    }
    else if (mainMenuTftBrightness == opt)
    {
        incTftBrightness();
    }
    else if (mainMenuLedBrightness == opt)
    {
        incLedBrightness();
    }
    else if (mainMenuMicGain == opt)
    {
        incMicGain();
    }
    else if (mainMenuScreensaverOff == opt ||
             mainMenuScreensaverTimeout == opt)
    {
        incScreensaverTime();
    }
    else if (mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu(false);
        return;
    }

    // Redraw the settings menu with different strings
    mainMenuSetUpSettingsMenu(false);
}

/**
 * Set up the secret menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void mainMenuSetUpSecretMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(mainMenu->menu, mainMenuSecret, mainMenuSecretCb);
    addRowToMeleeMenu(mainMenu->menu, modeBee.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeTest.modeName);
    addRowToMeleeMenu(mainMenu->menu, mainMenu->gitStr);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
    // Set the position
    if(resetPos)
    {
        mainMenu->secretPos = 0;
    }
    mainMenu->menu->selectedRow = mainMenu->secretPos;
}

/**
 * Callback for the secret menu
 *
 * @param opt The menu option which was selected
 */
void mainMenuSecretCb(const char* opt)
{
    // Save the position
    mainMenu->secretPos = mainMenu->menu->selectedRow;

    // Handle the option
    if(modeBee.modeName == opt)
    {
        // Start bee stuff
        switchToSwadgeMode(&modeBee);
    }
    if(modeTest.modeName == opt)
    {
        // Start test mode
        switchToSwadgeMode(&modeTest);
    }
    else if(mainMenuBack == opt)
    {
        mainMenu->menuSelection = 0;
        mainMenu->btnState = 0;
        mainMenu->prevBtnState = 0;
        mainMenu->debugMode = false;
        mainMenuSetUpTopMenu(false);
    }
}
