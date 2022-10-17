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

const char menuOptExit[] = "Back";

// Mode struct function declarations
void paintEnterMode(display_t* disp);
void paintExitMode(void);
void paintMainLoop(int64_t elapsedUs);
void paintButtonCb(buttonEvt_t* evt);
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
    .fnTouchCallback = NULL,
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
    paintMenu->networkMenu = initMeleeMenu(menuOptNetwork, &(paintMenu->menuFont), paintNetworkMenuCb);
    paintMenu->settingsMenu = initMeleeMenu(menuOptSettings, &(paintMenu->menuFont), paintSettingsMenuCb);

    paintMenuInitialize();
}

void paintExitMode(void)
{
    PAINT_LOGD("Exiting");
    deinitMeleeMenu(paintMenu->menu);
    deinitMeleeMenu(paintMenu->settingsMenu);
    freeFont(&(paintMenu->menuFont));

    switch (paintMenu->screen)
    {
        case PAINT_MENU:
        case PAINT_NETWORK_MENU:
        case PAINT_SETTINGS_MENU:
        break;

        case PAINT_DRAW:
            paintDrawScreenCleanup();
        break;

        case PAINT_SHARE:
        break;

        case PAINT_RECEIVE:
        break;

        case PAINT_GALLERY:
            paintGalleryCleanup();
        break;

        case PAINT_HELP:
            paintTutorialCleanup();
            paintDrawScreenCleanup();
        break;
    }

    free(paintMenu);
}

void paintMainLoop(int64_t elapsedUs)
{
    switch (paintMenu->screen)
    {
    case PAINT_MENU:
    {
        drawMeleeMenu(paintMenu->disp, paintMenu->menu);
        break;
    }

    case PAINT_NETWORK_MENU:
    {
        drawMeleeMenu(paintMenu->disp, paintMenu->networkMenu);
        break;
    }

    case PAINT_SETTINGS_MENU:
    {
        drawMeleeMenu(paintMenu->disp, paintMenu->settingsMenu);
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
                    meleeMenuButton(paintMenu->networkMenu, evt->button);
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
                    if (menuOptCancelErase == paintMenu->settingsMenu->rows[paintMenu->settingsMenuSelection])
                    {
                        paintMenu->eraseDataConfirm = true;
                        paintSetupSettingsMenu(false);
                    }
                    else if (menuOptConfirmErase == paintMenu->settingsMenu->rows[paintMenu->settingsMenuSelection])
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
                    meleeMenuButton(paintMenu->settingsMenu, evt->button);
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

// Util function implementations

void paintMenuInitialize(void)
{
    int32_t index;
    paintLoadIndex(&index);

    paintSetupMainMenu(true);

    paintMenu->menuSelection = 0;
    paintMenu->settingsMenuSelection = 0;
    paintMenu->eraseDataSelected = false;
    paintMenu->eraseDataConfirm = false;

    paintMenu->screen = PAINT_MENU;
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
        addRowToMeleeMenu(paintMenu->menu, menuOptGallery);
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

    resetMeleeMenu(paintMenu->networkMenu, menuOptNetwork, paintNetworkMenuCb);

    if (paintGetAnySlotInUse(index))
    {
        // Only add "share" if there's something to share
        addRowToMeleeMenu(paintMenu->networkMenu, menuOptShare);
    }

    addRowToMeleeMenu(paintMenu->networkMenu, menuOptReceive);
    addRowToMeleeMenu(paintMenu->networkMenu, menuOptExit);

    if (reset)
    {
        paintMenu->networkMenuSelection = 0;
    }

    paintMenu->networkMenu->selectedRow = paintMenu->networkMenuSelection;
}

void paintSetupSettingsMenu(bool reset)
{
    int32_t index;

    resetMeleeMenu(paintMenu->settingsMenu, menuOptSettings, paintSettingsMenuCb);

    paintLoadIndex(&index);
    if (index & PAINT_ENABLE_LEDS)
    {
        addRowToMeleeMenu(paintMenu->settingsMenu, menuOptLedsOn);
    }
    else
    {
        addRowToMeleeMenu(paintMenu->settingsMenu, menuOptLedsOff);
    }

    if (index & PAINT_ENABLE_BLINK)
    {
        addRowToMeleeMenu(paintMenu->settingsMenu, menuOptBlinkOn);
    }
    else
    {
        addRowToMeleeMenu(paintMenu->settingsMenu, menuOptBlinkOff);
    }

    if (paintMenu->eraseDataSelected)
    {
        if (paintMenu->eraseDataConfirm)
        {
            addRowToMeleeMenu(paintMenu->settingsMenu, menuOptConfirmErase);
        }
        else
        {
            addRowToMeleeMenu(paintMenu->settingsMenu, menuOptCancelErase);
        }
    }
    else
    {
        addRowToMeleeMenu(paintMenu->settingsMenu, menuOptEraseData);
    }
    addRowToMeleeMenu(paintMenu->settingsMenu, menuOptExit);

    if (reset)
    {
        paintMenu->settingsMenuSelection = 0;
        paintMenu->eraseDataSelected = false;
        paintMenu->eraseDataConfirm = false;
    }

    paintMenu->settingsMenu->selectedRow = paintMenu->settingsMenuSelection;
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
        paintGallerySetup(paintMenu->disp);
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
    paintMenu->networkMenuSelection = paintMenu->networkMenu->selectedRow;
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
    else if (opt == menuOptExit)
    {
        PAINT_LOGI("Selected Exit");
        paintSetupMainMenu(false);
        paintMenu->screen = PAINT_MENU;
    }
}

void paintSettingsMenuCb(const char* opt)
{
    paintMenu->settingsMenuSelection = paintMenu->settingsMenu->selectedRow;

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
    else if (opt == menuOptExit)
    {
        PAINT_LOGI("Selected Exit");
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
        break;

        case PAINT_DRAW:
            paintDrawScreenCleanup();
        break;

        case PAINT_SHARE:
        case PAINT_RECEIVE:
            switchToSwadgeMode(&modePaint);
        break;

        case PAINT_GALLERY:
            paintGalleryCleanup();
        break;

        case PAINT_HELP:
            paintTutorialCleanup();
            paintDrawScreenCleanup();
        break;
    }

    paintMenu->screen = PAINT_MENU;
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
