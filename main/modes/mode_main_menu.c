//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "swadgeMode.h"
#include "swadge_esp32.h"
#include "mode_main_menu.h"

#include "settingsManager.h"
#include "meleeMenu.h"

#include "fighter_menu.h"
#include "mode_gamepad.h"
#include "mode_demo.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void mainMenuEnterMode(display_t* disp);
void mainMenuExitMode(void);
void mainMenuMainLoop(int64_t elapsedUs);
void mainMenuButtonCb(buttonEvt_t* evt);
void mainMenuCb(const char* opt);

void mainMenuSetUpTopMenu(void);
void mainMenuTopLevelCb(const char* opt);
void mainMenuSetUpGamesMenu(void);
void mainMenuGamesCb(const char* opt);
void mainMenuSetUpToolsMenu(void);
void mainMenuToolsCb(const char* opt);
void mainMenuSetUpSettingsMenu(bool);
void mainMenuSettingsCb(const char* opt);

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    display_t* disp;
    font_t meleeMenuFont;
    meleeMenu_t* menu;
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
    .fnTemperatureCallback = NULL
};

const char mainMenuTitle[] = "Swadge!";
const char mainMenuGames[] = "Games";
const char mainMenuTools[] = "Tools";
const char mainMenuSettings[] = "Settings";
const char mainMenuBack[] = "Back";
const char mainMenuSoundOn[] = "Sound: On";
const char mainMenuSoundOff[] = "Sound: Off";
char mainMenuBrightness[] = "Brightness: 1";
char mainMenuMicVol[] = "Mic Volume: 1";

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO doc
 *
 */
void mainMenuEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    mainMenu = (mainMenu_t*)malloc(sizeof(mainMenu_t));
    memset(mainMenu, 0, sizeof(mainMenu_t));

    // Save a pointer to the display
    mainMenu->disp = disp;

    loadFont("mm.font", &mainMenu->meleeMenuFont);

    mainMenu->menu = initMeleeMenu(mainMenuTitle, &mainMenu->meleeMenuFont, mainMenuTopLevelCb);
    mainMenuSetUpTopMenu();
}

/**
 * @brief TODO doc
 *
 */
void mainMenuExitMode(void)
{
    deinitMeleeMenu(mainMenu->menu);
    freeFont(&mainMenu->meleeMenuFont);
    free(mainMenu);
}

/**
 * @brief TODO doc
 *
 * @param elapsedUs
 */
void mainMenuMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    drawMeleeMenu(mainMenu->disp, mainMenu->menu);
}

/**
 * @brief TODO doc
 *
 * @param evt
 */
void mainMenuButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            case DOWN:
            case LEFT:
            case RIGHT:
            case BTN_A:
            {
                meleeMenuButton(mainMenu->menu, evt->button);
                break;
            }
            case START:
            case SELECT:
            case BTN_B:
            default:
            {
                break;
            }
        }
    }
}

/**
 * @brief TODO doc
 *
 */
void mainMenuSetUpTopMenu(void)
{
    resetMeleeMenu(mainMenu->menu, mainMenuTitle, mainMenuTopLevelCb);
    addRowToMeleeMenu(mainMenu->menu, mainMenuGames);
    addRowToMeleeMenu(mainMenu->menu, mainMenuTools);
    addRowToMeleeMenu(mainMenu->menu, mainMenuSettings);
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void mainMenuTopLevelCb(const char* opt)
{
    if(mainMenuGames == opt)
    {
        mainMenuSetUpGamesMenu();
    }
    else if(mainMenuTools == opt)
    {
        mainMenuSetUpToolsMenu();
    }
    else if(mainMenuSettings == opt)
    {
        mainMenuSetUpSettingsMenu(false);
    }
}

/**
 * @brief TODO doc
 *
 */
void mainMenuSetUpGamesMenu(void)
{
    resetMeleeMenu(mainMenu->menu, mainMenuGames, mainMenuGamesCb);
    addRowToMeleeMenu(mainMenu->menu, modeFighter.modeName);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void mainMenuGamesCb(const char* opt)
{
    if(modeFighter.modeName == opt)
    {
        // Start fighter
        switchToSwadgeMode(&modeFighter);
    }
    else if(mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu();
    }
}

/**
 * @brief TODO doc
 *
 */
void mainMenuSetUpToolsMenu(void)
{
    resetMeleeMenu(mainMenu->menu, mainMenuTools, mainMenuToolsCb);
    addRowToMeleeMenu(mainMenu->menu, modeGamepad.modeName);
    addRowToMeleeMenu(mainMenu->menu, modeDemo.modeName);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void mainMenuToolsCb(const char* opt)
{
    if(modeGamepad.modeName == opt)
    {
        // Start gamepad
        switchToSwadgeMode(&modeGamepad);
    }
    else if(modeDemo.modeName == opt)
    {
        // Start demo
        switchToSwadgeMode(&modeDemo);
    }
    else if(mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu();
    }
}

/**
 * @brief TODO doc
 *
 * @param preservePosition
 */
void mainMenuSetUpSettingsMenu(bool preservePosition)
{
    // Pick the menu string based on the sound option
    const char* soundOpt;
    if(getIsMuted())
    {
        soundOpt = mainMenuSoundOff;
    }
    else
    {
        soundOpt = mainMenuSoundOn;
    }

    // Print the brightness option
    snprintf(mainMenuBrightness, sizeof(mainMenuBrightness), "Brightness: %d", getBrightness());

    // Print the brightness option
    snprintf(mainMenuMicVol, sizeof(mainMenuMicVol), "Mic Volume: %d", getMicVolume());

    // Save the old position
    uint8_t selectedRow = mainMenu->menu->selectedRow;
    // Reset the menu
    resetMeleeMenu(mainMenu->menu, mainMenuSettings, mainMenuSettingsCb);
    addRowToMeleeMenu(mainMenu->menu, soundOpt);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuBrightness);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuMicVol);
    addRowToMeleeMenu(mainMenu->menu, mainMenuBack);
    // Jump to the old position if requested
    if(preservePosition)
    {
        mainMenu->menu->selectedRow = selectedRow;
    }
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void mainMenuSettingsCb(const char* opt)
{
    if(mainMenuSoundOff == opt)
    {
        // Sound is off, turn it on
        setIsMuted(false);
    }
    else if (mainMenuSoundOn == opt)
    {
        // Sound is on, turn it off
        setIsMuted(true);
    }
    else if (mainMenuBrightness == opt)
    {
        uint8_t newBrightness = (getBrightness() + 1) % 10;
        setBrightness(newBrightness);
    }
    else if (mainMenuMicVol == opt)
    {
        uint8_t newVol = (getMicVolume() + 1) % 10;
        setMicVolume(newVol);
    }
    else if(mainMenuBack == opt)
    {
        mainMenuSetUpTopMenu();
        return;
    }

    // Redraw the settings menu with different strings
    mainMenuSetUpSettingsMenu(true);
}
