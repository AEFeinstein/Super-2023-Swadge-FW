//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "swadgeMode.h"
#include "swadge_esp32.h"
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

    uint8_t numSwadgeModes = getNumSwadgeModes();
    for(uint8_t idx = 1; idx < numSwadgeModes; idx++)
    {
        addRowToMeleeMenu(mainMenu->menu, swadgeModes[idx]->modeName);
    }
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
    uint8_t numSwadgeModes = getNumSwadgeModes();
    for(uint8_t idx = 1; idx < numSwadgeModes; idx++)
    {
        if(opt == swadgeModes[idx]->modeName)
        {
            ESP_LOGI("FTR", "Implement exit %d", idx);

            switchToSwadgeMode(idx);
        }
    }
}
