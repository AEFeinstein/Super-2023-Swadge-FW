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
 * - Draw icons for the tools?
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

static const char paintTitle[] = "MFPaint";
static const char menuOptDraw[] = "Draw";
static const char menuOptGallery[] = "Gallery";
static const char menuOptShare[] = "Share";
static const char menuOptReceive[] = "Receive";
static const char menuOptExit[] = "Exit";

// Mode struct function declarations
void paintEnterMode(display_t* disp);
void paintExitMode(void);
void paintMainLoop(int64_t elapsedUs);
void paintButtonCb(buttonEvt_t* evt);
void paintMainMenuCb(const char* opt);


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

void paintInitialize(void);

// Mode struct function implemetations

void paintEnterMode(display_t* disp)
{
    PAINT_LOGI("Allocating %zu bytes for paintMenu...", sizeof(paintMenu_t));
    paintMenu = calloc(1, sizeof(paintMenu_t));

    paintMenu->disp = disp;
    loadFont("mm.font", &(paintMenu->menuFont));

    paintMenu->menu = initMeleeMenu(paintTitle, &(paintMenu->menuFont), paintMainMenuCb);
    paintInitialize();
}

void paintExitMode(void)
{
    PAINT_LOGD("Exiting");
    deinitMeleeMenu(paintMenu->menu);
    freeFont(&(paintMenu->menuFont));

    switch (paintMenu->screen)
    {
        case PAINT_MENU:
        break;

        case PAINT_DRAW:
            paintDrawScreenCleanup();
        break;

        case PAINT_SHARE:
        break;

        case PAINT_RECEIVE:
        break;

        case PAINT_VIEW:
        break;

        case PAINT_GALLERY:
            paintGalleryCleanup();
        break;

        case PAINT_HELP:
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

    case PAINT_DRAW:
    {
        paintDrawScreenMainLoop(elapsedUs);
        break;
    }

    case PAINT_SHARE:
    break;

    case PAINT_RECEIVE:
    break;

    case PAINT_VIEW:
    break;

    case PAINT_GALLERY:
    {
        paintGalleryMainLoop(elapsedUs);
        break;
    }

    case PAINT_HELP:
    break;
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
                meleeMenuButton(paintMenu->menu, evt->button);
            }
            break;
        }

        case PAINT_DRAW:
        {
            paintDrawScreenButtonCb(evt);
            break;
        }

        case PAINT_VIEW:
        break;

        case PAINT_HELP:
        break;

        case PAINT_GALLERY:
        {
            paintGalleryModeButtonCb(evt);
            break;
        }

        case PAINT_SHARE:
        break;

        case PAINT_RECEIVE:
        break;
    }
}

// Util function implementations

void paintInitialize(void)
{
    int32_t index;
    paintLoadIndex(&index);

    resetMeleeMenu(paintMenu->menu, paintTitle, paintMainMenuCb);
    addRowToMeleeMenu(paintMenu->menu, menuOptDraw);

    if (paintGetAnySlotInUse(index))
    {
        // Only add "gallery" and "share" if there's something to share
        addRowToMeleeMenu(paintMenu->menu, menuOptGallery);
        addRowToMeleeMenu(paintMenu->menu, menuOptShare);
    }

    addRowToMeleeMenu(paintMenu->menu, menuOptReceive);
    addRowToMeleeMenu(paintMenu->menu, menuOptExit);

    paintMenu->screen = PAINT_MENU;
}

void paintMainMenuCb(const char* opt)
{
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
    else if (opt == menuOptShare)
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
        switchToSwadgeMode(&modeMainMenu);
    }
}

void paintReturnToMainMenu(void)
{
    switch(paintMenu->screen)
    {
        case PAINT_MENU:
        break;

        case PAINT_DRAW:
            paintDrawScreenCleanup();
        break;

        case PAINT_SHARE:
        case PAINT_RECEIVE:
            switchToSwadgeMode(&modePaint);
        break;

        case PAINT_VIEW:
        break;

        case PAINT_GALLERY:
            paintGalleryCleanup();
        break;

        case PAINT_HELP:
        break;
    }

    paintMenu->screen = PAINT_MENU;
}
