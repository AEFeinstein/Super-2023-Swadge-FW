#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "swadgeMode.h"
#include "meleeMenu.h"
#include "swadge_esp32.h"
#include "mode_paint.h"
#include "mode_main_menu.h"

static const char paintTitle[] = "Paint";
static const char menuOptDraw[] = "Draw";
static const char menuOptGallery[] = "Gallery";
static const char menuOptReceive[] = "Receive";
static const char menuOptExit[] = "Exit";

typedef enum
{
    // Top menu
    PAINT_MENU,
    // Control instructions
    PAINT_HELP,
    // Drawing mode
    PAINT_DRAW,
    // Select and view/edit saved drawings
    PAINT_GALLERY,
    // View a drawing, no editing
    PAINT_VIEW,
    // Share a drawing via ESPNOW
    PAINT_SHARE,
    // Receive a shared drawing over ESPNOW
    PAINT_RECEIVE,
} paintScreen_t;

typedef struct
{
    font_t menuFont;
    meleeMenu_t* menu;
    display_t* disp;
    paintScreen_t screen;
} paintMenu_t;


// Mode struct function declarations
void paintEnterMode(display_t* disp);
void paintExitMode();
void paintMainLoop(int64_t elapsedUs);
void paintButtonCb(buttonEvt_t* evt);
void paintMainMenuCb(const char* opt);


swadgeMode modePaint =
{
    .modeName = "Swadge Paint",
    .fnEnterMode = paintEnterMode,
    .fnExitMode = paintExitMode,
    .fnMainLoop = paintMainLoop,
    .fnButtonCallback = paintButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

paintMenu_t* paintState;

// Util function declarations

void paintSetupMainMenu();

// Mode struct function implemetations

void paintEnterMode(display_t* disp)
{
    paintState = calloc(1, sizeof(paintMenu_t));

    paintState->disp = disp;
    loadFont("mm.font", &(paintState->menuFont));

    paintState->menu = initMeleeMenu(paintTitle, &(paintState->menuFont), paintMainMenuCb);

    paintSetupMainMenu();
}

void paintExitMode()
{
    deinitMeleeMenu(paintState->menu);
    freeFont(&(paintState->menuFont));
    free(paintState);
    //pm = NULL;
}

void paintMainLoop(int64_t elapsedUs)
{
    switch (paintState->screen)
    {
        case PAINT_MENU:
        {
            drawMeleeMenu(paintState->disp, paintState->menu);
            break;
        }
        default:
            break;
    }
}

void paintButtonCb(buttonEvt_t* evt)
{
    switch (paintState->screen)
    {
        case PAINT_MENU:
            if (evt->down)
            {
                meleeMenuButton(paintState->menu, evt->button);
            }
            break;

        default:
            break;
    }
}

// Util function implementations

void paintSetupMainMenu()
{
    resetMeleeMenu(paintState->menu, paintTitle, paintMainMenuCb);
    addRowToMeleeMenu(paintState->menu, menuOptDraw);
    addRowToMeleeMenu(paintState->menu, menuOptGallery);
    addRowToMeleeMenu(paintState->menu, menuOptReceive);
    addRowToMeleeMenu(paintState->menu, menuOptExit);
    paintState->screen = PAINT_MENU;
}

void paintMainMenuCb(const char* opt)
{
    if (opt == menuOptDraw)
    {
        ESP_LOGD("Paint", "Selected Draw");
    }
    else if (opt == menuOptGallery)
    {
        ESP_LOGD("Paint", "Selected Gallery");
    }
    else if (opt == menuOptReceive)
    {
        ESP_LOGD("Paint", "Selected Receive");
    }
    else if (opt == menuOptExit)
    {
        ESP_LOGD("Paint", "Selected Exit");
        switchToSwadgeMode(&modeMainMenu);
    }
}