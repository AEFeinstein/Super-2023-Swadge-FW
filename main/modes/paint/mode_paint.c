#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "swadgeMode.h"
#include "btn.h"
#include "meleeMenu.h"
#include "swadge_esp32.h"
#include "nvs_manager.h"
#include "bresenham.h"
#include "math.h"

#include "mode_main_menu.h"
#include "swadge_util.h"
#include "settingsManager.h"

#include "mode_paint.h"
#include "paint_common.h"
#include "paint_util.h"
#include "paint_draw.h"
#include "paint_gallery.h"
#include "paint_share.h"
#include "paint_nvs.h"

/*
 * REMAINING BIG THINGS TO DO:
 *
 * - Airbrush tool
 * - Stamp tool?
 * - Copy/paste???
 * - Easter egg?
 *
 *
 * MORE MINOR POLISH THINGS:
 *
 * - Fix swapping fg/bg color sometimes causing current color not to be the first in the color picker list
 *   (sometimes this makes it so if you change a tool, it also changes your color)
 * - Use different pick markers / cursors for each brush (e.g. crosshair for circle pen, two L-shaped crosshairs for box pen...)
 */

const char paintTitle[] = "MFPaint";
const char menuOptDraw[] = "Draw";
const char menuOptHelp[] = "Tutorial";
const char menuOptGallery[] = "Gallery";
const char menuOptNetwork[] = "Sharing";
const char menuOptShare[] = "Share";
const char menuOptReceive[] = "Receive";
const char menuOptSettings[] = "Settings";

const char menuOptLedsOn[] = "LEDs: On";
const char menuOptLedsOff[] = "LEDs: Off";
const char menuOptBlinkOn[] = "BlinkPx: On";
const char menuOptBlinkOff[] = "BlinkPx: Off";
const char menuOptEraseData[] = "Erase Data";
const char menuOptCancelErase[] = "Confirm? No!";
const char menuOptConfirmErase[] = "Confirm? Yes";

const char menuOptExit[] = "Exit";
const char menuOptBack[] = "Back";

// Mode struct function declarations
void paintEnterMode(display_t* disp);
void paintExitMode(void);
void paintMainLoop(int64_t elapsedUs);
void paintButtonCb(buttonEvt_t* evt);
void paintTouchCb(touch_event_t* evt);
void paintMainMenuCb(const char* opt);
void paintNetworkMenuCb(const char* opt);
void paintSettingsMenuCb(const char* opt);


swadgeMode modePaint =
{
    .modeName = paintTitle,
    .fnEnterMode = paintEnterMode,
    .fnExitMode = paintExitMode,
    .fnMainLoop = paintMainLoop,
    .fnButtonCallback = paintButtonCb,
    .fnTouchCallback = paintTouchCb,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

// Util function declarations

void paintDeleteAllData(void);
void paintMenuInitialize(void);
void paintSetupMainMenu(bool reset);
void paintSetupNetworkMenu(bool reset);
void paintSetupSettingsMenu(bool reset);

// Mode struct function implemetations

void paintEnterMode(display_t* disp)
{
    PAINT_LOGI("Allocating %zu bytes for paintMenu...", sizeof(paintMenu_t));
    paintMenu = calloc(1, sizeof(paintMenu_t));

    paintMenu->disp = disp;
    loadFont("mm.font", &(paintMenu->menuFont));

    paintMenu->menu = initMeleeMenu(paintTitle, &(paintMenu->menuFont), paintMainMenuCb);

    paintMenuInitialize();
}

void paintExitMode(void)
{
    PAINT_LOGD("Exiting");

    // Cleanup any sub-modes based on paintMenu->screen
    paintReturnToMainMenu();

    deinitMeleeMenu(paintMenu->menu);
    freeFont(&(paintMenu->menuFont));

    free(paintMenu);
    paintMenu = NULL;
}

void paintMainLoop(int64_t elapsedUs)
{
    switch (paintMenu->screen)
    {
    case PAINT_MENU:
    case PAINT_NETWORK_MENU:
    case PAINT_SETTINGS_MENU:
    {
        if (paintMenu->enableScreensaver && getScreensaverTime() != 0)
        {
            paintMenu->idleTimer += elapsedUs;
        }

        if (getScreensaverTime() != 0 && paintMenu->idleTimer >= (getScreensaverTime() * 1000000))
        {
            PAINT_LOGI("Selected Gallery");
            paintGallerySetup(paintMenu->disp, true);
            paintGallery->returnScreen = paintMenu->screen;
            paintMenu->screen = PAINT_GALLERY;
        }
        else
        {
            drawMeleeMenu(paintMenu->disp, paintMenu->menu);
        }
        break;
    }

    case PAINT_DRAW:
    case PAINT_HELP:
    {
        paintDrawScreenMainLoop(elapsedUs);
        break;
    }

    case PAINT_SHARE:
        // Implemented in a different mode
        break;

    case PAINT_RECEIVE:
        // Implemented in a different mode
        break;

    case PAINT_GALLERY:
    {
        paintGalleryMainLoop(elapsedUs);
        break;
    }
    }
}

void paintButtonCb(buttonEvt_t* evt)
{
    paintMenu->idleTimer = 0;

    switch (paintMenu->screen)
    {
        case PAINT_MENU:
        {
            if (evt->down)
            {
                if (evt->button == BTN_B)
                {
                    switchToSwadgeMode(&modeMainMenu);
                }
                else
                {
                    meleeMenuButton(paintMenu->menu, evt->button);
                }
            }
            break;
        }

        case PAINT_NETWORK_MENU:
        {
            if (evt->down)
            {
                if (evt->button == BTN_B)
                {
                    paintMenu->screen = PAINT_MENU;
                    paintSetupMainMenu(false);
                }
                else
                {
                    meleeMenuButton(paintMenu->menu, evt->button);
                }
            }
            break;
        }

        case PAINT_SETTINGS_MENU:
        {
            if (evt->down)
            {
                if (evt->button == LEFT || evt->button == RIGHT)
                {
                    if (menuOptCancelErase == paintMenu->menu->rows[paintMenu->settingsMenuSelection])
                    {
                        paintMenu->eraseDataConfirm = true;
                        paintSetupSettingsMenu(false);
                    }
                    else if (menuOptConfirmErase == paintMenu->menu->rows[paintMenu->settingsMenuSelection])
                    {
                        paintMenu->eraseDataConfirm = false;
                        paintSetupSettingsMenu(false);
                    }
                }
                else if (evt->button == BTN_B)
                {
                    paintMenu->screen = PAINT_MENU;
                    paintSetupMainMenu(false);
                }
                else
                {
                    meleeMenuButton(paintMenu->menu, evt->button);
                }
            }
            break;
        }

        case PAINT_DRAW:
        case PAINT_HELP:
        {
            paintDrawScreenButtonCb(evt);
            break;
        }

        case PAINT_GALLERY:
        {
            paintGalleryModeButtonCb(evt);
            break;
        }

        case PAINT_SHARE:
        case PAINT_RECEIVE:
            // Handled in a different mode
        break;
    }
}

void paintTouchCb(touch_event_t* evt)
{
    if (paintMenu->screen == PAINT_DRAW || paintMenu->screen == PAINT_HELP)
    {
        paintDrawScreenTouchCb(evt);
    }
}

// Util function implementations

void paintMenuInitialize(void)
{
    paintMenu->menuSelection = 0;
    paintMenu->settingsMenuSelection = 0;
    paintMenu->eraseDataSelected = false;
    paintMenu->eraseDataConfirm = false;
    paintMenu->idleTimer = 0;

    paintMenu->screen = PAINT_MENU;

    paintSetupMainMenu(true);
}

void paintSetupMainMenu(bool reset)
{
    int32_t index;
    paintLoadIndex(&index);

    resetMeleeMenu(paintMenu->menu, paintTitle, paintMainMenuCb);
    addRowToMeleeMenu(paintMenu->menu, menuOptDraw);

    if (paintGetAnySlotInUse(index))
    {
        // Only add "gallery" if there's something to view
        paintMenu->enableScreensaver = true;
        addRowToMeleeMenu(paintMenu->menu, menuOptGallery);
    }
    else
    {
        paintMenu->enableScreensaver = false;
    }

    addRowToMeleeMenu(paintMenu->menu, menuOptNetwork);
    addRowToMeleeMenu(paintMenu->menu, menuOptHelp);
    addRowToMeleeMenu(paintMenu->menu, menuOptSettings);
    addRowToMeleeMenu(paintMenu->menu, menuOptExit);

    if (reset)
    {
        paintMenu->menuSelection = 0;
    }

    paintMenu->menu->selectedRow = paintMenu->menuSelection;
}

void paintSetupNetworkMenu(bool reset)
{
    int32_t index;
    paintLoadIndex(&index);

    resetMeleeMenu(paintMenu->menu, menuOptNetwork, paintNetworkMenuCb);

    if (paintGetAnySlotInUse(index))
    {
        // Only add "share" if there's something to share
        addRowToMeleeMenu(paintMenu->menu, menuOptShare);
    }

    addRowToMeleeMenu(paintMenu->menu, menuOptReceive);
    addRowToMeleeMenu(paintMenu->menu, menuOptBack);

    if (reset)
    {
        paintMenu->networkMenuSelection = 0;
    }

    paintMenu->menu->selectedRow = paintMenu->networkMenuSelection;
}

void paintSetupSettingsMenu(bool reset)
{
    int32_t index;

    resetMeleeMenu(paintMenu->menu, menuOptSettings, paintSettingsMenuCb);

    paintLoadIndex(&index);
    if (index & PAINT_ENABLE_LEDS)
    {
        addRowToMeleeMenu(paintMenu->menu, menuOptLedsOn);
    }
    else
    {
        addRowToMeleeMenu(paintMenu->menu, menuOptLedsOff);
    }

    if (index & PAINT_ENABLE_BLINK)
    {
        addRowToMeleeMenu(paintMenu->menu, menuOptBlinkOn);
    }
    else
    {
        addRowToMeleeMenu(paintMenu->menu, menuOptBlinkOff);
    }

    if (paintMenu->eraseDataSelected)
    {
        if (paintMenu->eraseDataConfirm)
        {
            addRowToMeleeMenu(paintMenu->menu, menuOptConfirmErase);
        }
        else
        {
            addRowToMeleeMenu(paintMenu->menu, menuOptCancelErase);
        }
    }
    else
    {
        addRowToMeleeMenu(paintMenu->menu, menuOptEraseData);
    }
    addRowToMeleeMenu(paintMenu->menu, menuOptBack);

    if (reset)
    {
        paintMenu->settingsMenuSelection = 0;
        paintMenu->eraseDataSelected = false;
        paintMenu->eraseDataConfirm = false;
    }

    paintMenu->menu->selectedRow = paintMenu->settingsMenuSelection;
}

void paintMainMenuCb(const char* opt)
{
    paintMenu->menuSelection = paintMenu->menu->selectedRow;
    if (opt == menuOptDraw)
    {
        PAINT_LOGI("Selected Draw");
        paintMenu->screen = PAINT_DRAW;
        paintDrawScreenSetup(paintMenu->disp);
    }
    else if (opt == menuOptGallery)
    {
        PAINT_LOGI("Selected Gallery");
        paintMenu->screen = PAINT_GALLERY;
        paintGallerySetup(paintMenu->disp, false);
    }
    else if (opt == menuOptNetwork)
    {
        PAINT_LOGI("Selected Network");
        paintSetupNetworkMenu(true);
        paintMenu->screen = PAINT_NETWORK_MENU;
    }
    else if (opt == menuOptHelp)
    {
        PAINT_LOGE("Selected Help");
        paintMenu->screen = PAINT_HELP;
        paintTutorialSetup(paintMenu->disp);
        paintDrawScreenSetup(paintMenu->disp);
    }
    else if (opt == menuOptSettings)
    {
        paintSetupSettingsMenu(true);
        paintMenu->screen = PAINT_SETTINGS_MENU;
    }
    else if (opt == menuOptExit)
    {
        PAINT_LOGI("Selected Exit");
        switchToSwadgeMode(&modeMainMenu);
    }
}

void paintNetworkMenuCb(const char* opt)
{
    paintMenu->networkMenuSelection = paintMenu->menu->selectedRow;
    if (opt == menuOptShare)
    {
        PAINT_LOGI("Selected Share");
        paintMenu->screen = PAINT_SHARE;
        switchToSwadgeMode(&modePaintShare);
    }
    else if (opt == menuOptReceive)
    {
        PAINT_LOGI("Selected Receive");
        paintMenu->screen = PAINT_RECEIVE;
        switchToSwadgeMode(&modePaintReceive);
    }
    else if (opt == menuOptBack)
    {
        PAINT_LOGI("Selected Back");
        paintSetupMainMenu(false);
        paintMenu->screen = PAINT_MENU;
    }
}

void paintSettingsMenuCb(const char* opt)
{
    paintMenu->settingsMenuSelection = paintMenu->menu->selectedRow;

    int32_t index;
    if (opt == menuOptLedsOff)
    {
        // Enable the LEDs
        paintLoadIndex(&index);
        index |= PAINT_ENABLE_LEDS;
        paintSaveIndex(index);
    }
    else if (opt == menuOptLedsOn)
    {
        paintLoadIndex(&index);
        index &= ~PAINT_ENABLE_LEDS;
        paintSaveIndex(index);
    }
    else if (opt == menuOptBlinkOff)
    {
        paintLoadIndex(&index);
        index |= PAINT_ENABLE_BLINK;
        paintSaveIndex(index);
    }
    else if (opt == menuOptBlinkOn)
    {
        paintLoadIndex(&index);
        index &= ~PAINT_ENABLE_BLINK;
        paintSaveIndex(index);
    }
    else if (opt == menuOptEraseData)
    {
        paintMenu->eraseDataSelected = true;
    }
    else if (opt == menuOptConfirmErase)
    {
        paintDeleteAllData();
        paintMenu->eraseDataConfirm = false;
        paintMenu->eraseDataSelected = false;
    }
    else if (opt == menuOptCancelErase)
    {
        paintMenu->eraseDataSelected = false;
        paintMenu->eraseDataConfirm = false;
    }
    else if (opt == menuOptBack)
    {
        PAINT_LOGI("Selected Back");
        paintSetupMainMenu(false);
        paintMenu->screen = PAINT_MENU;
        return;
    }

    paintSetupSettingsMenu(false);
}

void paintReturnToMainMenu(void)
{
    switch(paintMenu->screen)
    {
        case PAINT_MENU:
        case PAINT_NETWORK_MENU:
        case PAINT_SETTINGS_MENU:
        case PAINT_SHARE:
        case PAINT_RECEIVE:
        break;

        case PAINT_DRAW:
            paintDrawScreenCleanup();
        break;

        case PAINT_GALLERY:
            if (paintGallery->screensaverMode)
            {
                paintMenu->screen = paintGallery->returnScreen;
                paintGalleryCleanup();
                if (paintMenu->screen == PAINT_MENU)
                {
                    paintSetupMainMenu(false);
                }
                else if (paintMenu->screen == PAINT_NETWORK_MENU)
                {
                    paintSetupNetworkMenu(false);
                }
                else if (paintMenu->screen == PAINT_SETTINGS_MENU)
                {
                    paintSetupSettingsMenu(false);
                }
                return;
            }
            else
            {
                paintGalleryCleanup();
            }
        break;

        case PAINT_HELP:
            paintTutorialCleanup();
            paintDrawScreenCleanup();
        break;
    }

    paintMenu->screen = PAINT_MENU;
    paintMenu->idleTimer = 0;
    paintSetupMainMenu(false);
}

void paintDeleteAllData(void)
{
    int32_t index;
    paintLoadIndex(&index);

    for (uint8_t i = 0; i < PAINT_SAVE_SLOTS; i++)
    {
        paintDeleteSlot(&index, i);
    }

    paintDeleteIndex();
}
