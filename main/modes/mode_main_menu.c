//==============================================================================
// Includes
//==============================================================================

#include "esp_log.h"

#include "swadgeMode.h"
#include "mode_main_menu.h"

#include "menu2d.h"

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
    font_t ibm_vga8;
    font_t radiostars;
    menu_t * menu;
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

static const char _R1I1[] = "Row 1-1";
static const char _R1I2[] = "Row 1-2";
static const char _R2I1[] = "Row 2-1";
static const char _R2I2[] = "Row 2-2";
static const char _R3I1[] = "Row 3-1";
static const char _R3I2[] = "Row 3-2";

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

    loadFont("ibm_vga8.font", &mainMenu->ibm_vga8);
    loadFont("radiostars.font", &mainMenu->radiostars);

    rgba_pixel_t fg =
    {
        .r = 0x1F,
        .g = 0x1F,
        .b = 0x00,
        .a = PX_OPAQUE,
    };
    rgba_pixel_t bg =
    {
        .r = 0x00,
        .g = 0x00,
        .b = 0x04,
        .a = PX_OPAQUE,
    };
    mainMenu->menu = initMenu(disp, "Swadge!", &mainMenu->radiostars, &mainMenu->ibm_vga8, fg, bg, mainMenuCb);

    // void addRowToMenu(menu_t* menu);
    // linkedInfo_t* addItemToRow(menu_t* menu, const char* name);
    addRowToMenu(mainMenu->menu);
    addItemToRow(mainMenu->menu, _R1I1);
    addItemToRow(mainMenu->menu, _R1I2);
    addRowToMenu(mainMenu->menu);
    addItemToRow(mainMenu->menu, _R2I1);
    addItemToRow(mainMenu->menu, _R2I2);
    addRowToMenu(mainMenu->menu);
    addItemToRow(mainMenu->menu, _R3I1);
    addItemToRow(mainMenu->menu, _R3I2);
}


/**
 * @brief TODO
 *
 */
void mainMenuExitMode(void)
{
    deinitMenu(mainMenu->menu);
    freeFont(&mainMenu->ibm_vga8);
    freeFont(&mainMenu->radiostars);
    free(mainMenu);
}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void mainMenuMainLoop(int64_t elapsedUs)
{
    drawMenu(mainMenu->menu);
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
                menuButton(mainMenu->menu, evt->button);
                break;
            }
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
