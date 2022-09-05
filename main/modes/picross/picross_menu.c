//==============================================================================
// Functions
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "meleeMenu.h"

#include "picross_menu.h"
#include "mode_picross.h"
#include "picross_select.h"
#include "mode_main_menu.h"

//==============================================================================
// Enums & Structs
//==============================================================================

typedef enum
{
    PICROSS_MENU,
    PICROSS_LEVELSELECT,
    PICROSS_GAME
} picrossScreen_t;

typedef struct
{
    font_t mmFont;
    meleeMenu_t* menu;
    display_t* disp;
    picrossLevelDef_t levels[8];
    picrossScreen_t screen;
    bool menuChanged;

} picrossMenu_t;
//==============================================================================
// Function Prototypes
//==============================================================================


void picrossEnterMode(display_t* disp);
void picrossExitMode(void);
void picrossMainLoop(int64_t elapsedUs);
void picrossButtonCb(buttonEvt_t* evt);
void loadLevels(void);
void picrossMainMenuCb(const char* opt);
void SelectPicrossLevel(picrossLevelDef_t* selectedLevel);
void ReturnToLevelSelect(void);


//==============================================================================
// Variables
//==============================================================================

swadgeMode modePicross =
{
    .modeName = "Picross",
    .fnEnterMode = picrossEnterMode,
    .fnExitMode = picrossExitMode,
    .fnMainLoop = picrossMainLoop,
    .fnButtonCallback = picrossButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

static const char str_picrossTitle[] = "Picross";
static const char str_picross[] = "Start";
static const char str_levelSelect[] = "Puzzle Select";
static const char str_exit[] = "Exit";

picrossMenu_t* pm;

//==============================================================================
// Functions
//==============================================================================
/**
 * @brief Enter the picross mode by displaying the top level menu
 *
 * @param disp The display to draw to
 *
 */
void picrossEnterMode(display_t* disp)
{
    // Allocate and zero memory
    pm = calloc(1, sizeof(picrossMenu_t));

    pm->disp = disp;

    loadFont("mm.font", &(pm->mmFont));

    pm->menu = initMeleeMenu(str_picrossTitle, &(pm->mmFont), picrossMainMenuCb);

    setPicrossMainMenu();

    pm->screen = PICROSS_MENU;

    loadLevels();

}

void picrossExitMode(void)
{
    picrossExitGame();
    deinitMeleeMenu(pm->menu);
    //p2pDeinit(&jm->p2p);
    freeFont(&(pm->mmFont));
    free(pm);
}

void loadLevels()
{
    loadWsg("test1.wsg", &pm->levels[0].levelWSG);
    loadWsg("test1_complete.wsg", &pm->levels[0].completedWSG);
    pm->levels[0].index = 0;

    loadWsg("test2.wsg", &pm->levels[1].levelWSG);
    loadWsg("test2_c.wsg", &pm->levels[1].completedWSG);
    pm->levels[1].index = 1;
}

/**
 * Call the appropriate main loop function for the screen being displayed
 *
 * @param elapsedUd Time.deltaTime
 */
void picrossMainLoop(int64_t elapsedUs)
{
    switch(pm->screen)
    {
        case PICROSS_MENU:
        {
            if (pm->menuChanged)
            {
                drawMeleeMenu(pm->disp, pm->menu);
                pm->menuChanged = false;
            }
            break;
        }
        case PICROSS_LEVELSELECT:
        {
            picrossLevelSelectLoop(elapsedUs);
            break;
        }
        case PICROSS_GAME:
        {
            picrossGameLoop(elapsedUs);
            break;
        }
    }
}

/**
 * @brief Call the appropriate button functions for the screen being displayed
 *
 * @param evt
 */
void picrossButtonCb(buttonEvt_t* evt)
{
    switch(pm->screen)
    {
        case PICROSS_MENU:
        {
            //Pass button events from the Swadge mode to the menu
            if (evt->down)
            {
                pm->menuChanged = true;
                meleeMenuButton(pm->menu, evt->button);
            }
            break;
        }
        case PICROSS_LEVELSELECT:
        {
            picrossLevelSelectButtonCb(evt);
            break;
        }
        case PICROSS_GAME:
        {
            picrossGameButtonCb(evt);
            break;
        }
            //No wifi mode stuff
    }
}


/////////////////////////

/**
 * @brief Sets up the top level menu for Picross
 */
void setPicrossMainMenu(void)
{
    resetMeleeMenu(pm->menu, str_picrossTitle, picrossMainMenuCb);
    addRowToMeleeMenu(pm->menu, str_picross);
    addRowToMeleeMenu(pm->menu, str_levelSelect);
    addRowToMeleeMenu(pm->menu, str_exit);
    pm->menuChanged = true;
    pm->screen = PICROSS_MENU;
}

//menu button callbacks. Set the screen and call the appropriate start functions
void picrossMainMenuCb(const char* opt)
{
    if (opt == str_picross)
    {
        // picrossStartGame(pm->disp, &pm->mmFont);
        // pm->screen = PICROSS_GAME;
        return;
    }
    if (opt == str_levelSelect)
    {
        pm->screen = PICROSS_LEVELSELECT;
        picrossStartLevelSelect(pm->disp,&pm->mmFont,pm->levels,(picrossLevelDef_t*)SelectPicrossLevel);
        return;
    }
    if (opt == str_exit)
    {
        // Exit to main menu
        switchToSwadgeMode(&modeMainMenu);
        return;
    }
}

void SelectPicrossLevel(picrossLevelDef_t* selectedLevel)
{
    pm->screen = PICROSS_GAME;
    picrossStartGame(pm->disp, &pm->mmFont, (void*)ReturnToLevelSelect, selectedLevel);
}

void ReturnToLevelSelect()
{
    //todo: abstract this to function
    pm->screen = PICROSS_LEVELSELECT;
    picrossStartLevelSelect(pm->disp,&pm->mmFont,pm->levels,(picrossLevelDef_t**)SelectPicrossLevel);      
}