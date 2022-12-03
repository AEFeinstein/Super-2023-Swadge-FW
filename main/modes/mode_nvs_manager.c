/*
 * mode_nvs_manager.c
 *
 *  Created on: 3 Dec 2022
 *      Author: bryce and dylwhich
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "bresenham.h"
#include "display.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "esp_timer.h"
#include "led_util.h"
#include "linked_list.h"
#include "mode_main_menu.h"
#include "settingsManager.h"
#include "touch_sensor.h"

#include "mode_nvs_manager.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define CORNER_OFFSET 14
#define TOP_TEXT_X_MARGIN CORNER_OFFSET / 2
#define LINE_BREAK_Y 8

/// Helper macro to return an integer clamped within a range (MIN to MAX)
//#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
//#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/*==============================================================================
 * Enums
 *============================================================================*/



/*==============================================================================
 * Prototypes
 *============================================================================*/

void  nvsManagerEnterMode(display_t* disp);
void  nvsManagerExitMode(void);
void  nvsManagerButtonCallback(buttonEvt_t* evt);
void  nvsManagerTouchCallback(touch_event_t* evt);
void  nvsManagerMainLoop(int64_t elapsedUs);

/*==============================================================================
 * Structs
 *============================================================================*/



/*==============================================================================
 * Variables
 *============================================================================*/

// The swadge mode
swadgeMode modeNvsManager =
{
    .modeName = "Save Data Manager",
    .fnEnterMode = nvsManagerEnterMode,
    .fnExitMode = nvsManagerExitMode,
    .fnButtonCallback = nvsManagerButtonCallback,
    .fnTouchCallback = nvsManagerTouchCallback,
    .fnMainLoop = nvsManagerMainLoop,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .overrideUsb = false
};

// The state data
typedef struct
{
    display_t* disp;
    font_t ibm_vga8;
    font_t radiostars;
    font_t mm;

    // Track touch
    bool touchHeld;
    int32_t touchPosition;
    int32_t touchIntensity;
} nvsManager_t;

nvsManager_t* nvsManager;

/*==============================================================================
 * Const Variables
 *============================================================================*/

const char str_factory_reset[] = "Factory Reset";
const char str_confirm_reset_no[] = "Confirm Reset: No";
const char str_confirm_reset_yes[] = "Confirm Reset: YES";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for nvsManager
 */
void  nvsManagerEnterMode(display_t* disp)
{
    // Allocate zero'd memory for the mode
    nvsManager = calloc(1, sizeof(nvsManager_t));

    nvsManager->disp = disp;

    loadFont("ibm_vga8.font", &nvsManager->ibm_vga8);
    loadFont("radiostars.font", &nvsManager->radiostars);
    loadFont("mm.font", &nvsManager->mm);

    stopNote();
}

/**
 * Called when nvsManager is exited
 */
void  nvsManagerExitMode(void)
{
    stopNote();

    freeFont(&nvsManager->ibm_vga8);
    freeFont(&nvsManager->radiostars);
    freeFont(&nvsManager->mm);

    free(nvsManager);
}

/**
 * This function is called when a button press is pressed. Buttons are
 * handled by interrupts and queued up for this callback, so there are no
 * strict timing restrictions for this function.
 *
 * @param evt The button event that occurred
 */
void  nvsManagerTouchCallback(touch_event_t* evt)
{
    nvsManager->rhythmNoteIdx = 0;
    nvsManager->lastCallTimeUs = 0;
    nvsManager->touchHeld = evt->state != 0;
    nvsManager->shouldPlay = evt->state != 0 || nvsManager->aHeld;
    //nvsManager->touchPosition = roundf((evt->position * BAR_X_WIDTH) / 255);
}

/**
 * @brief Button callback function, plays notes and switches parameters
 *
 * @param evt The button event that occurred
 */
void  nvsManagerButtonCallback(buttonEvt_t* evt)
{
}

/**
 * Update the display by drawing the current state of affairs
 */
void  nvsManagerMainLoop(int64_t elapsedUs)
{
    nvsManager->disp->clearPx();
    fillDisplayArea(nvsManager->disp, 0, 0, nvsManager->disp->w, nvsManager->disp->h, c100);
}
