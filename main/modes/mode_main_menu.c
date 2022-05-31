//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "swadgeMode.h"
#include "mode_main_menu.h"

#include "meleeMenu.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void mainMenuEnterMode(display_t * disp);
void mainMenuExitMode(void);
void mainMenuMainLoop(int64_t elapsedUs);
void mainMenuButtonCb(buttonEvt_t* evt);
void mainMenuCb(const char* opt);

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    display_t * disp;
    font_t meleeMenuFont;
    meleeMenu_t * menu;
} mainMenu_t;

//==============================================================================
// Variables
//==============================================================================

mainMenu_t * mainMenu;

swadgeMode modemainMenu =
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

static const char _R1I1[] = "1-P Mode";
static const char _R2I1[] = "VS. Mode";
static const char _R3I1[] = "Trophies";
static const char _R4I1[] = "Options";
static const char _R5I1[] = "Data";

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 */
void mainMenuEnterMode(display_t * disp)
{
    // Allocate memory for this mode
    mainMenu = (mainMenu_t *)malloc(sizeof(mainMenu_t));
    memset(mainMenu, 0, sizeof(mainMenu_t));

    // Save a pointer to the display
    mainMenu->disp = disp;

    loadFont("mm.font", &mainMenu->meleeMenuFont);

    mainMenu->menu = initMeleeMenu("Main Menu", &mainMenu->meleeMenuFont, mainMenuCb);

    addRowToMeleeMenu(mainMenu->menu, _R1I1);
    addRowToMeleeMenu(mainMenu->menu, _R2I1);
    addRowToMeleeMenu(mainMenu->menu, _R3I1);
    addRowToMeleeMenu(mainMenu->menu, _R4I1);
    addRowToMeleeMenu(mainMenu->menu, _R5I1);
}


/**
 * @brief TODO
 *
 */
void mainMenuExitMode(void)
{
    deinitMeleeMenu(mainMenu->menu);
    freeFont(&mainMenu->meleeMenuFont);
    free(mainMenu);
}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void mainMenuMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    drawMeleeMenu(mainMenu->disp, mainMenu->menu);
}

/**
 * @brief TODO
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
 * @brief TODO
 * 
 * @param opt 
 */
void mainMenuCb(const char* opt)
{
    ESP_LOGI("MNU", "%s", opt);
}
