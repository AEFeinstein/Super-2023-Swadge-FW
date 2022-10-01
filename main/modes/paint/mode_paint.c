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

/*
 * REMAINING BIG THINGS TO DO:
 *
 * - Draw icons for the tools?
 * - Explicitly mark everything that doesn't work in the canvas coordinate space for whatever reason
 * - Sharing! How will that work...
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
    PAINT_LOGI("Allocating %zu bytes for paintState...", sizeof(paintMenu_t));
    paintState = calloc(1, sizeof(paintMenu_t));

    paintState->disp = disp;
    loadFont("mm.font", &(paintState->menuFont));
    loadFont("radiostars.font", &(paintState->toolbarFont));

    paintState->menu = initMeleeMenu(paintTitle, &(paintState->menuFont), paintMainMenuCb);

    // Clear the LEDs
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        paintState->leds[i].r = 0;
        paintState->leds[i].g = 0;
        paintState->leds[i].b = 0;
    }
    setLeds(paintState->leds, NUM_LEDS);

    paintInitialize();
}

void paintExitMode(void)
{
    PAINT_LOGD("Exiting");
    deinitMeleeMenu(paintState->menu);
    freeFont(&(paintState->menuFont));

    freePxStack(&paintState->pxStack);
    freePxStack(&paintState->cursorPxs);
    free(paintState);
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
    break;

    case PAINT_HELP:
    break;
    }
}

void paintButtonCb(buttonEvt_t* evt)
{
    switch (paintState->screen)
    {
        case PAINT_MENU:
        {
            if (evt->down)
            {
                meleeMenuButton(paintState->menu, evt->button);
            }
            break;
        }

        case PAINT_DRAW:
        {
            switch (paintState->buttonMode)
            {
                case BTN_MODE_DRAW:
                {
                    paintDrawModeButtonCb(evt);
                    break;
                }

                case BTN_MODE_SELECT:
                {
                    paintSelectModeButtonCb(evt);
                    break;
                }

                case BTN_MODE_SAVE:
                {
                    paintSaveModeButtonCb(evt);
                    break;
                }
            }

            break;
        }

        case PAINT_VIEW:
        break;

        case PAINT_HELP:
        break;

        case PAINT_GALLERY:
        break;

        case PAINT_SHARE:
        break;

        case PAINT_RECEIVE:
        break;
    }
}

// Util function implementations

void paintInitialize(void)
{
    resetMeleeMenu(paintState->menu, paintTitle, paintMainMenuCb);
    addRowToMeleeMenu(paintState->menu, menuOptDraw);
    addRowToMeleeMenu(paintState->menu, menuOptGallery);
    addRowToMeleeMenu(paintState->menu, menuOptReceive);
    addRowToMeleeMenu(paintState->menu, menuOptExit);

    paintState->screen = PAINT_MENU;

    paintDrawScreenSetup();
}

void paintMainMenuCb(const char* opt)
{
    if (opt == menuOptDraw)
    {
        PAINT_LOGI("Selected Draw");
        paintState->screen = PAINT_DRAW;
    }
    else if (opt == menuOptGallery)
    {
        PAINT_LOGI("Selected Gallery");
        paintState->screen = PAINT_GALLERY;
    }
    else if (opt == menuOptReceive)
    {
        PAINT_LOGI("Selected Receive");
        paintState->screen = PAINT_RECEIVE;
    }
    else if (opt == menuOptExit)
    {
        PAINT_LOGI("Selected Exit");
        switchToSwadgeMode(&modeMainMenu);
    }
}
