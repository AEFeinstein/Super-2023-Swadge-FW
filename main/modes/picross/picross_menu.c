//==============================================================================
// Functions
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "meleeMenu.h"

#include "picross_menu.h"
#include "mode_picross.h"
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

void picrossMainMenuCb(const char* opt);

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

}

void picrossExitMode(void)
{
    picrossExitGame();
    deinitMeleeMenu(pm->menu);
    //p2pDeinit(&jm->p2p);
    freeFont(&(pm->mmFont));
    free(pm);
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

        }
        case PICROSS_GAME:
        {
            picrossGameLoop(elapsedUs);
            break;
        }
            //No wifi mode stuff
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
 * @brief Sets up the top level menu for Picross, including callback
 */
void setPicrossMainMenu(void)
{
    resetMeleeMenu(pm->menu, str_picrossTitle, picrossMainMenuCb);
    addRowToMeleeMenu(pm->menu, str_picross);
    addRowToMeleeMenu(pm->menu, str_exit);
    pm->menuChanged = true;
    pm->screen = PICROSS_MENU;
}

void picrossMainMenuCb(const char* opt)
{
    if (opt == str_picross)
    {
        ESP_LOGI("FTR", "Let's go!");
        picrossStartGame(pm->disp, &pm->mmFont);
        pm->screen = PICROSS_GAME;
        return;
    }

    if (opt == str_exit)
    {
        // Exit to main menu
        switchToSwadgeMode(&modeMainMenu);
    }
}
