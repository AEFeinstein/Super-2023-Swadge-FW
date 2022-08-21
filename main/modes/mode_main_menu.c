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
#include "mode_tiltrads.h"
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

void mainMenuSetUpTopMenu(bool);
void mainMenuTopLevelCb(const char* opt);
void mainMenuSetUpGamesMenu(bool);
void mainMenuGamesCb(const char* opt);
void mainMenuSetUpToolsMenu(bool);
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
    bool shouldDraw;
    uint8_t topLevelPos;
    uint8_t gamesPos;
    uint8_t toolsPos;
    uint8_t settingsPos;
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

    // Initialize the menu
    mainMenu->menu = initMeleeMenu(mainMenuTitle, &mainMenu->meleeMenuFont, mainMenuTopLevelCb);
    mainMenuSetUpTopMenu(true);

    // Set it to draw
    mainMenu->shouldDraw = true;
}

/**
 * Deinitialize and exit the main menu
 */
void mainMenuExitMode(void)
{
    deinitMeleeMenu(mainMenu->menu);
    freeFont(&mainMenu->meleeMenuFont);
    free(mainMenu);
}

/**
 * The main menu loop, draw the menu if there's been a difference
 *
 * @param elapsedUs unused
 */
void mainMenuMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    if(mainMenu->shouldDraw)
    {
        mainMenu->shouldDraw = false;
        drawMeleeMenu(mainMenu->disp, mainMenu->menu);
    }
}

/**
 * Button callback for the main menu, pass the event to the menu
 *
 * @param evt
 */
void mainMenuButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        meleeMenuButton(mainMenu->menu, evt->button);
        mainMenu->shouldDraw = true;
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
    addRowToMeleeMenu(mainMenu->menu, mainMenuSettings);
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
    else if(mainMenuSettings == opt)
    {
        mainMenuSetUpSettingsMenu(true);
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
    addRowToMeleeMenu(mainMenu->menu, modeDemo.modeName);
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
    else if(modeDemo.modeName == opt)
    {
        // Start demo
        switchToSwadgeMode(&modeDemo);
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

    // Reset the menu
    resetMeleeMenu(mainMenu->menu, mainMenuSettings, mainMenuSettingsCb);
    addRowToMeleeMenu(mainMenu->menu, soundOpt);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuBrightness);
    addRowToMeleeMenu(mainMenu->menu, (const char*)mainMenuMicVol);
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
        mainMenuSetUpTopMenu(false);
        return;
    }

    // Redraw the settings menu with different strings
    mainMenuSetUpSettingsMenu(false);
}
