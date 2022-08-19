//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"

#include "swadgeMode.h"

#include "mode_gamepad.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void gamepadEnterMode(display_t* disp);
void gamepadExitMode(void);
void gamepadMainLoop(int64_t elapsedUs);
void gamepadButtonCb(buttonEvt_t* evt);
void gamepadTouchCb(touch_event_t* evt);

//==============================================================================
// Variables
//==============================================================================

typedef struct
{
    display_t* disp;
} gamepad_t;

gamepad_t* gamepad;

swadgeMode modeGamepad =
{
    .modeName = "Gamepad",
    .fnEnterMode = gamepadEnterMode,
    .fnExitMode = gamepadExitMode,
    .fnMainLoop = gamepadMainLoop,
    .fnButtonCallback = gamepadButtonCb,
    .fnTouchCallback = gamepadTouchCb,
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
void gamepadEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    gamepad = (gamepad_t*)calloc(1, sizeof(gamepad_t));

    // Save a pointer to the display
    gamepad->disp = disp;
}

/**
 * @brief TODO
 *
 */
void gamepadExitMode(void)
{
    free(gamepad);
}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void gamepadMainLoop(int64_t elapsedUs)
{
    // Meh
}

/**
 * @brief TODO
 *
 * @param evt
 */
void gamepadButtonCb(buttonEvt_t* evt)
{
    // Only send data if USB is ready
    if(tud_ready())
    {
        // Build a list of all independent buttons held down
        hid_gamepad_button_bm_t btnPressed = 0;
        if(evt->state & BTN_A)
        {
            btnPressed |= GAMEPAD_BUTTON_A;
        }
        if(evt->state & BTN_B)
        {
            btnPressed |= GAMEPAD_BUTTON_B;
        }
        if(evt->state & START)
        {
            btnPressed |= GAMEPAD_BUTTON_START;
        }
        if(evt->state & SELECT)
        {
            btnPressed |= GAMEPAD_BUTTON_SELECT;
        }

        // Figure out which way the D-Pad is pointing
        hid_gamepad_hat_t hatDir = GAMEPAD_HAT_CENTERED;
        if(evt->state & UP)
        {
            if(evt->state & RIGHT)
            {
                hatDir |= GAMEPAD_HAT_UP_RIGHT;
            }
            else if(evt->state & LEFT)
            {
                hatDir |= GAMEPAD_HAT_UP_LEFT;
            }
            else
            {
                hatDir |= GAMEPAD_HAT_UP;
            }
        }
        else if(evt->state & DOWN)
        {
            if(evt->state & RIGHT)
            {
                hatDir |= GAMEPAD_HAT_DOWN_RIGHT;
            }
            else if(evt->state & LEFT)
            {
                hatDir |= GAMEPAD_HAT_DOWN_LEFT;
            }
            else
            {
                hatDir |= GAMEPAD_HAT_DOWN;
            }
        }
        else if(evt->state & RIGHT)
        {
            hatDir |= GAMEPAD_HAT_RIGHT;
        }
        else if(evt->state & LEFT)
        {
            hatDir |= GAMEPAD_HAT_LEFT;
        }

        // Build and send the state over USB
        hid_gamepad_report_t report =
        {
            .buttons = btnPressed,
            .hat = hatDir
        };
        tud_gamepad_report(&report);
    }
}

/**
 * @brief TODO
 *
 * @param evt
 */
void gamepadTouchCb(touch_event_t* evt)
{
    ESP_LOGE("GP", "%s (%d %d %d)", __func__, evt->pad_num, evt->pad_status, evt->pad_val);
}
