//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"

#include "bresenham.h"
#include "swadge_esp32.h"
#include "swadgeMode.h"

#include "mode_gamepad.h"

//==============================================================================
// Defines
//==============================================================================

#define DPAD_BTN_RADIUS     16
#define DPAD_CLUSTER_RADIUS 45

#define START_BTN_RADIUS 10
#define START_BTN_SEP     2

#define AB_BTN_RADIUS 25
#define AB_BTN_Y_OFF   8
#define AB_BTN_SEP     2

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
    hid_gamepad_report_t gpState;
    bool drawDisp;
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
 * Enter the gamepad mode, allocate memory, intialize USB
 */
void gamepadEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    gamepad = (gamepad_t*)calloc(1, sizeof(gamepad_t));

    // Save a pointer to the display
    gamepad->disp = disp;
    gamepad->drawDisp = true;

    /* Initialize USB peripheral */
    tinyusb_config_t tusb_cfg = {};
    tinyusb_driver_install(&tusb_cfg);
}

/**
 * Exit the gamepad mode and free memory
 */
void gamepadExitMode(void)
{
    free(gamepad);
}

/**
 * Draw the gamepad state to the display when it changes
 *
 * @param elapsedUs unused
 */
void gamepadMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    // If there is something to draw
    if(gamepad->drawDisp)
    {
        // Lower the flag to draw for the next loop
        gamepad->drawDisp = false;        

        // Clear the display
        gamepad->disp->clearPx();

        // Helper function pointer
        void (*drawFunc)(display_t *, int, int, int, paletteColor_t);

        // A list of all the hat directions, in order
        static const uint8_t hatDirs[] = 
        {
            GAMEPAD_HAT_UP,
            GAMEPAD_HAT_UP_RIGHT,
            GAMEPAD_HAT_RIGHT,
            GAMEPAD_HAT_DOWN_RIGHT,
            GAMEPAD_HAT_DOWN,
            GAMEPAD_HAT_DOWN_LEFT,
            GAMEPAD_HAT_LEFT,
            GAMEPAD_HAT_UP_LEFT
        };

        // For each hat direction
        for(uint8_t i = 0; i < ARRAY_SIZE(hatDirs); i++)
        {
            // The degree around the cluster
            int16_t deg = i * 45;
            // The center of the cluster
            int16_t xc = gamepad->disp->w / 4;
            int16_t yc = gamepad->disp->h / 2;
            // Draw the button around the cluster
            xc += (( getSin1024(deg) * DPAD_CLUSTER_RADIUS) / 1024);
            yc += ((-getCos1024(deg) * DPAD_CLUSTER_RADIUS) / 1024);

            // Draw either a filled or outline circle, if this is the direction pressed
            drawFunc = (gamepad->gpState.hat == hatDirs[i]) ? &plotCircleFilled : &plotCircle;
            drawFunc(gamepad->disp, xc, yc, DPAD_BTN_RADIUS, c555);
        }

        // Start button
        drawFunc = (gamepad->gpState.buttons == GAMEPAD_BUTTON_START) ? &plotCircleFilled : &plotCircle;
        drawFunc(gamepad->disp,
            (gamepad->disp->w / 2) - START_BTN_RADIUS - START_BTN_SEP,
            (gamepad->disp->h / 4),
            START_BTN_RADIUS, c555);

        // Select
        drawFunc = (gamepad->gpState.buttons == GAMEPAD_BUTTON_SELECT) ? &plotCircleFilled : &plotCircle;
        drawFunc(gamepad->disp,
            (gamepad->disp->w / 2) + START_BTN_RADIUS + START_BTN_SEP,
            (gamepad->disp->h / 4),
            START_BTN_RADIUS, c555);

        // Button A
        drawFunc = (gamepad->gpState.buttons == GAMEPAD_BUTTON_A) ? &plotCircleFilled : &plotCircle;
        drawFunc(gamepad->disp,
            ((3 * gamepad->disp->w) / 4) - AB_BTN_RADIUS - AB_BTN_SEP,
            (gamepad->disp->h / 2) + AB_BTN_Y_OFF,
            AB_BTN_RADIUS, c555);

        // Button B
        drawFunc = (gamepad->gpState.buttons == GAMEPAD_BUTTON_B) ? &plotCircleFilled : &plotCircle;
        drawFunc(gamepad->disp,
            ((3 * gamepad->disp->w) / 4) + AB_BTN_RADIUS + AB_BTN_SEP,
            (gamepad->disp->h / 2) - AB_BTN_Y_OFF,
            AB_BTN_RADIUS, c555);
    }
}

/**
 * Button callback. Send the button state over USB and save it for drawing
 *
 * @param evt The button event that occurred
 */
void gamepadButtonCb(buttonEvt_t* evt)
{
    gamepad->drawDisp = true;

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
        gamepad->gpState.buttons = btnPressed;
        gamepad->gpState.hat = hatDir;
        gamepad->gpState.x = 0;
        gamepad->gpState.y = 0;
        gamepad->gpState.z = 0;
        gamepad->gpState.rx = 0;
        gamepad->gpState.ry = 0;
        gamepad->gpState.rz = 0;

        tud_gamepad_report(&gamepad->gpState);
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
