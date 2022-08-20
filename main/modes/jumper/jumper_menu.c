//==============================================================================
// Functions
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "meleeMenu.h"

#include "jumper_menu.h"
#include "mode_jumper.h"
//==============================================================================
// Enums & Structs
//==============================================================================

typedef enum
{
    JUMPER_MENU,
    JUMPER_GAME
} jumperScreen_t;

typedef struct
{
    font_t mmFont;
    meleeMenu_t* menu;
    display_t* disp;
    jumperScreen_t screen;
    bool menuChanged;
    
} jumperMenu_t;
//==============================================================================
// Function Prototypes
//==============================================================================


void jumperEnterMode(display_t* disp);
void jumperExitMode(void);
void jumperMainLoop(int64_t elapsedUs);
void jumperButtonCb(buttonEvt_t* evt);

void setJumperMainMenu(void);
void jumperMainMenuCb(const char* opt);

//==============================================================================
// Variables
//==============================================================================

swadgeMode modeJumper = {
    .modeName = "Jumper",
    .fnEnterMode = jumperEnterMode,
    .fnExitMode = jumperExitMode,
    .fnMainLoop = jumperMainLoop,
    .fnButtonCallback = jumperButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

static const char str_jumpTitle[] = "Donut Jump!";
static const char str_jump[] = "Jump";
static const char str_exit[] = "Exit";

jumperMenu_t* jm;

//==============================================================================
// Functions
//==============================================================================
/**
 * @brief Enter the jumper mode by displaying the top level menu
 * 
 * @param disp The display to draw to
 * 
 */
void jumperEnterMode(display_t* disp)
{
    // Allocate and zero memory
    jm = calloc(1, sizeof(jumperMenu_t));

    jm->disp = disp;

    loadFont("mm.font", &(jm->mmFont));

    jm->menu = initMeleeMenu(str_jumpTitle, &(jm->mmFont), jumperMainMenuCb);

    setJumperMainMenu();

    jm->screen = JUMPER_MENU;
    
}

void jumperExitMode(void)
{
    jumperExitGame();
    deinitMeleeMenu(jm->menu);
    //p2pDeinit(&jm->p2p);
    freeFont(&(jm->mmFont));
    free(jm);
}


/**
 * Call the appropriate main loop function for the screen being displayed
 * 
 * @param elapsedUd Time.deltaTime
 */
void jumperMainLoop(int64_t elapsedUs)
{
    switch(jm->screen)
    {
        case JUMPER_MENU:
        {
            if (jm->menuChanged)
            {
                drawMeleeMenu(jm->disp, jm->menu);
                jm->menuChanged = false;
            }
            break;
        }
        case JUMPER_GAME:
        {
            jumperGameLoop(elapsedUs);
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
void jumperButtonCb(buttonEvt_t* evt)
{
    switch(jm->screen)
    {
        case JUMPER_MENU:
        {
            //Pass button events from the Swadge mode to the menu
            if (evt->down)
            {
                jm->menuChanged = true;
                meleeMenuButton(jm->menu, evt->button);
            }
            break;
        }
        case JUMPER_GAME:
        {
            jumperGameButtonCb(evt);
            break;
        }
        //No wifi mode stuff
    }
}


/////////////////////////

/**
 * @brief Sets up the top level menu for Jumper, including callback 
 */
void setJumperMainMenu(void)
{
    resetMeleeMenu(jm->menu, str_jumpTitle, jumperMainMenuCb);
    addRowToMeleeMenu(jm->menu, str_jump);
    addRowToMeleeMenu(jm->menu, str_exit);
    jm->menuChanged = true;
    jm->screen = JUMPER_MENU;
}

void jumperMainMenuCb(const char* opt)
{
    if (opt == str_jump)
    {
        ESP_LOGI("FTR", "Let's go!");
        jumperStartGame(jm->disp, &jm->mmFont);
        jm->screen = JUMPER_GAME;
        return;
    }

    if (opt == str_exit)
    {
        ESP_LOGI("FTR", "Implement exit");
        //Clear all things jumper
        switchToSwadgeMode(0);
    }
}
