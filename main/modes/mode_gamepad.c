//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include "esp_timer.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include "meleeMenu.h"

#include "bresenham.h"
#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "swadge_util.h"

#include "mode_gamepad.h"
#include "mode_main_menu.h"

//==============================================================================
// Defines
//==============================================================================

#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))

#define Y_OFF 20

#define DPAD_BTN_RADIUS     16
#define DPAD_CLUSTER_RADIUS 45

#define START_BTN_RADIUS 10
#define START_BTN_SEP     2

#define AB_BTN_RADIUS 25
#define AB_BTN_Y_OFF   8
#define AB_BTN_SEP     2

#define ACCEL_BAR_H       8
#define ACCEL_BAR_SEP     1
#define MAX_ACCEL_BAR_W 100

#define TOUCHBAR_WIDTH  100
#define TOUCHBAR_HEIGHT  20
#define TOUCHBAR_Y_OFF   55

//==============================================================================
// Enums
//==============================================================================
typedef enum
{
    GAMEPAD_MENU,
    GAMEPAD_MAIN
} gamepadScreen_t;

typedef enum {
    GAMEPAD_GENERIC,
    GAMEPAD_NS
} gamepadType_t;

//==============================================================================
// Functions Prototypes
//==============================================================================

void gamepadEnterMode(display_t* disp);
void gamepadExitMode(void);
void gamepadMainLoop(int64_t elapsedUs);
void gamepadButtonCb(buttonEvt_t* evt);
void gamepadTouchCb(touch_event_t* evt);
void gamepadAccelCb(accel_t* accel);
void gamepadReportStateToHost(void);

void setGamepadMainMenu(void);
void gamepadMainMenuCb(const char* opt);
void gamepadMenuLoop(int64_t elapsedUs);
void gamepadMenuButtonCb(buttonEvt_t* evt);
void gamepadMenuTouchCb(touch_event_t* evt);
void gamepadMenuAccelCb(accel_t* accel);
void gamepadStart(display_t* disp, gamepadType_t type);

//==============================================================================
// Variables
//==============================================================================


static const char str_gamepadTitle[] = "Gamepad Type";
static const char str_pc[] = "PC";
static const char str_ns[] = "Switch";
static const char str_exit[] = "Exit";

typedef struct
{
    font_t mmFont;
    meleeMenu_t* menu;
    display_t* disp;
    gamepadScreen_t screen;
    uint8_t selectedGamepadType;
} gamepadMenu_t;

typedef struct
{
    display_t* disp;
    hid_gamepad_report_t gpState;
    hid_gamepad_ns_report_t gpNsState;
    font_t ibmFont;
    uint8_t gamepadType;
    bool isPluggedIn;
} gamepad_t;

gamepad_t* gamepad;

swadgeMode modeGamepad =
{
    .modeName = "Gamepad",
    .fnEnterMode = gamepadEnterMode,
    .fnExitMode = gamepadExitMode,
    .fnMainLoop = gamepadMenuLoop,
    .fnButtonCallback = gamepadMenuButtonCb,
    .fnTouchCallback = gamepadMenuTouchCb,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = gamepadMenuAccelCb,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
    .overrideUsb = true
};

gamepadMenu_t* gm;

const hid_gamepad_button_bm_t touchMap[] =
{
    GAMEPAD_BUTTON_C,
    GAMEPAD_BUTTON_X,
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_Z,
    GAMEPAD_BUTTON_TL,
};

const hid_gamepad_button_bm_t touchMapNs[] =
{
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_TL,
    GAMEPAD_BUTTON_Z,
    GAMEPAD_BUTTON_TR,
    GAMEPAD_BUTTON_X,
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Enter the gamepad mode, allocate memory, intialize USB
 */
void gamepadEnterMode(display_t* disp)
{
     // Allocate and zero memory
    gm = calloc(1, sizeof(gamepadMenu_t));

    gm->disp = disp;
    
    loadFont("mm.font", &(gm->mmFont));

    gm->menu = initMeleeMenu("Gamepad", &(gm->mmFont), gamepadMainMenuCb);

    setGamepadMainMenu();
}

/**
 * Exit the gamepad mode and free memory
 */
void gamepadExitMode(void)
{
    deinitMeleeMenu(gm->menu);
    freeFont(&(gm->mmFont));
    free(gm);

    if(gamepad != NULL){
        freeFont(&(gamepad->ibmFont));
        free(gamepad);
        gamepad = NULL;
    }
}

void setGamepadMainMenu(void)
{
    resetMeleeMenu(gm->menu, str_gamepadTitle, gamepadMainMenuCb);
    addRowToMeleeMenu(gm->menu, str_pc);
    addRowToMeleeMenu(gm->menu, str_ns);
    addRowToMeleeMenu(gm->menu, str_exit);

    gm->screen = GAMEPAD_MENU;
}

void gamepadMainMenuCb(const char* opt)
{
    if (opt == str_pc)
    {
        gamepadStart(gm->disp, GAMEPAD_GENERIC);
        gm->screen = GAMEPAD_MAIN;
        return;
    }

    if (opt == str_ns)
    {
        gamepadStart(gm->disp, GAMEPAD_NS);
        gm->screen = GAMEPAD_MAIN;
        return;
    }

    if (opt == str_exit)
    {
        // Exit to main menu
        switchToSwadgeMode(&modeMainMenu);
        return;
    }
}

void gamepadStart(display_t* disp, gamepadType_t type){
    // Allocate memory for this mode
    gamepad = (gamepad_t*)calloc(1, sizeof(gamepad_t));

    // Save a pointer to the display
    gamepad->disp = disp;

    gamepad->gamepadType = type;

    tusb_desc_device_t nsDescriptor = {
        .bLength = 18U,
        .bDescriptorType = 1,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 64,
        .idVendor = 0x0f0d,
        .idProduct = 0x0092,
        .bcdDevice = 0x0100,
        .iManufacturer = 0x01,
        .iProduct = 0x02,
        .iSerialNumber = 0x03,
        .bNumConfigurations = 0x01
    };

    tinyusb_config_t default_cfg = {
    };

    tinyusb_config_t tusb_cfg = {
        .descriptor = &nsDescriptor
    };

    tinyusb_driver_install((gamepad->gamepadType==GAMEPAD_NS) ? &tusb_cfg :&default_cfg);

    gamepad->gpNsState.x = 128;
    gamepad->gpNsState.y = 128;
    gamepad->gpNsState.rx = 128;
    gamepad->gpNsState.ry = 128;


    // Load the font
    loadFont("ibm_vga8.font", &(gamepad->ibmFont));
}

/**
 * Call the appropriate main loop function for the screen being displayed
 *
 * @param elapsedUd Time.deltaTime
 */
void gamepadMenuLoop(int64_t elapsedUs)
{
    switch(gm->screen)
    {
        case GAMEPAD_MENU:
        {
            drawMeleeMenu(gm->disp, gm->menu);
            break;
        }
        case GAMEPAD_MAIN:
        {
            gamepadMainLoop(elapsedUs);
            break;
        }
            //No wifi mode stuff
    }
}

/**
 * @brief Call the appropriate button functions for the screen being displayed
 *
 * @param evt
 */
void gamepadMenuButtonCb(buttonEvt_t* evt)
{
    switch(gm->screen)
    {
        case GAMEPAD_MENU:
        {
            //Pass button events from the Swadge mode to the menu
            if (evt->down)
            {
                meleeMenuButton(gm->menu, evt->button);
            }
            break;
        }
        case GAMEPAD_MAIN:
        {
            gamepadButtonCb(evt);
            break;
        }
            //No wifi mode stuff
    }
}

void gamepadMenuTouchCb(touch_event_t* evt){
    switch(gm->screen)
    {
        case GAMEPAD_MENU:
        {
            break;
        }
        case GAMEPAD_MAIN:
        {
            gamepadTouchCb(evt);
            break;
        }
    }
};

void gamepadMenuAccelCb(accel_t* accel){
    switch(gm->screen)
    {
        case GAMEPAD_MENU:
        {
            break;
        }
        case GAMEPAD_MAIN:
        {
            gamepadAccelCb(accel);
            break;
        }
    }
};

/**
 * Draw the gamepad state to the display when it changes
 *
 * @param elapsedUs unused
 */
void gamepadMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    // Check if plugged in or not
    if(tud_ready() != gamepad->isPluggedIn)
    {
        gamepad->isPluggedIn = tud_ready();
    }

    // Clear the display
    fillDisplayArea(gamepad->disp, 0, 0, gamepad->disp->w, gamepad->disp->h, c213);

    // Always Draw some reminder text, centered
    const char reminderText[] = "Start + Select to Exit";
    int16_t tWidth = textWidth(&gamepad->ibmFont, reminderText);
    drawText(gamepad->disp, &gamepad->ibmFont, c555, reminderText, (gamepad->disp->w - tWidth) / 2, 10);

    // If it's plugged in, draw buttons
    if(gamepad->isPluggedIn)
    {
        // Helper function pointer
        void (*drawFunc)(display_t*, int, int, int, paletteColor_t);

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
            int16_t yc = (gamepad->disp->h / 2) + Y_OFF;
            // Draw the button around the cluster
            xc += (( getSin1024(deg) * DPAD_CLUSTER_RADIUS) / 1024);
            yc += ((-getCos1024(deg) * DPAD_CLUSTER_RADIUS) / 1024);

            // Draw either a filled or outline circle, if this is the direction pressed
            switch(gamepad->gamepadType){
                case GAMEPAD_NS:{
                    drawFunc = (gamepad->gpNsState.hat == (hatDirs[i]-1)) ? &plotCircleFilled : &plotCircle;
                    break;
                }
                case GAMEPAD_GENERIC:
                default: {
                    drawFunc = (gamepad->gpState.hat == hatDirs[i]) ? &plotCircleFilled : &plotCircle;
                    break;
                }
            }

            drawFunc(gamepad->disp, xc, yc, DPAD_BTN_RADIUS, c551 /*paletteHsvToHex(i * 32, 0xFF, 0xFF)*/);
        }

        // Select button
        switch(gamepad->gamepadType){
            case GAMEPAD_NS:{
                drawFunc = (gamepad->gpNsState.buttons & GAMEPAD_NS_BUTTON_SELECT) ? &plotCircleFilled : &plotCircle;
                break;
            }
            case GAMEPAD_GENERIC:
            default:{
                drawFunc = (gamepad->gpState.buttons & GAMEPAD_BUTTON_SELECT) ? &plotCircleFilled : &plotCircle;
                break;
            }
        }
        drawFunc(gamepad->disp,
                 (gamepad->disp->w / 2) - START_BTN_RADIUS - START_BTN_SEP,
                 (gamepad->disp->h / 4) + Y_OFF,
                 START_BTN_RADIUS, c333);

        // Start button
        switch(gamepad->gamepadType){ 
            case GAMEPAD_NS:{
                drawFunc = (gamepad->gpNsState.buttons & GAMEPAD_NS_BUTTON_START) ? &plotCircleFilled : &plotCircle;
                break;
            }
            case GAMEPAD_GENERIC:
            default:{
                drawFunc = (gamepad->gpState.buttons & GAMEPAD_BUTTON_START) ? &plotCircleFilled : &plotCircle;
                break;
            }
        }
        drawFunc(gamepad->disp,
                 (gamepad->disp->w / 2) + START_BTN_RADIUS + START_BTN_SEP,
                 (gamepad->disp->h / 4) + Y_OFF,
                 START_BTN_RADIUS, c333);

        // Button A
        switch(gamepad->gamepadType){
            case GAMEPAD_NS:{
                drawFunc = (gamepad->gpNsState.buttons & GAMEPAD_NS_BUTTON_A) ? &plotCircleFilled : &plotCircle;
                break;
            }
            case GAMEPAD_GENERIC:
            default: {
                drawFunc = (gamepad->gpState.buttons & GAMEPAD_BUTTON_A) ? &plotCircleFilled : &plotCircle;
                break;
            }
        }
        drawFunc(gamepad->disp,
                 ((3 * gamepad->disp->w) / 4) + AB_BTN_RADIUS + AB_BTN_SEP,
                 (gamepad->disp->h / 2) - AB_BTN_Y_OFF + Y_OFF,
                 AB_BTN_RADIUS, c243);

        // Button B
        switch(gamepad->gamepadType){
            case GAMEPAD_NS:{
                drawFunc = (gamepad->gpNsState.buttons & GAMEPAD_NS_BUTTON_B) ? &plotCircleFilled : &plotCircle;
                break;
            }
            case GAMEPAD_GENERIC:
            default:{
                drawFunc = (gamepad->gpState.buttons & GAMEPAD_BUTTON_B) ? &plotCircleFilled : &plotCircle;
                break;
            }
        }
        drawFunc(gamepad->disp,
                 ((3 * gamepad->disp->w) / 4) - AB_BTN_RADIUS - AB_BTN_SEP,
                 (gamepad->disp->h / 2) + AB_BTN_Y_OFF + Y_OFF,
                 AB_BTN_RADIUS, c401);

        // Draw touch strip
        int16_t tBarX = gamepad->disp->w - TOUCHBAR_WIDTH;
        uint8_t numTouchElem = (sizeof(touchMap) / sizeof(touchMap[0]));
        for(uint8_t touchIdx = 0; touchIdx < numTouchElem; touchIdx++)
        {            
            if((gamepad->gamepadType == GAMEPAD_GENERIC) ? gamepad->gpState.buttons & touchMap[touchIdx]:gamepad->gpNsState.buttons & touchMapNs[touchIdx])
            {
                fillDisplayArea(gamepad->disp,
                                tBarX - 1, TOUCHBAR_Y_OFF,
                                tBarX + (TOUCHBAR_WIDTH / numTouchElem), TOUCHBAR_Y_OFF + TOUCHBAR_HEIGHT,
                                c111);
            }
            else
            {
                plotRect(gamepad->disp,
                         tBarX - 1, TOUCHBAR_Y_OFF,
                         tBarX + (TOUCHBAR_WIDTH / numTouchElem), TOUCHBAR_Y_OFF + TOUCHBAR_HEIGHT,
                         c111);
            }
            tBarX += (TOUCHBAR_WIDTH / numTouchElem);
        }

        if(gamepad->gamepadType != GAMEPAD_GENERIC){
            return;
        }

        // Set up drawing accel bars
        int16_t barY = (gamepad->disp->h * 3) / 4;

        // Plot X accel
        int16_t barWidth = ((gamepad->gpState.rx + 128) * MAX_ACCEL_BAR_W) / 256;
        fillDisplayArea(gamepad->disp, gamepad->disp->w - barWidth, barY, gamepad->disp->w, barY + ACCEL_BAR_H, c500);
        barY += (ACCEL_BAR_H + ACCEL_BAR_SEP);

        // Plot Y accel
        barWidth = ((gamepad->gpState.ry + 128) * MAX_ACCEL_BAR_W) / 256;
        fillDisplayArea(gamepad->disp, gamepad->disp->w - barWidth, barY, gamepad->disp->w, barY + ACCEL_BAR_H, c050);
        barY += (ACCEL_BAR_H + ACCEL_BAR_SEP);

        // Plot Z accel
        barWidth = ((gamepad->gpState.rz + 128) * MAX_ACCEL_BAR_W) / 256;
        fillDisplayArea(gamepad->disp, gamepad->disp->w - barWidth, barY, gamepad->disp->w, barY + ACCEL_BAR_H, c005);
        // barY += (ACCEL_BAR_H + ACCEL_BAR_SEP);
    }
    else
    {
        // If it's not plugged in, give a hint
        const char plugInText[] = "Plug USB-C into computer please!";
        tWidth = textWidth(&gamepad->ibmFont, plugInText);
        drawText(gamepad->disp, &gamepad->ibmFont, c555, plugInText,
                 (gamepad->disp->w - tWidth) / 2,
                 (gamepad->disp->h - gamepad->ibmFont.h) / 2);
    }
}

/**
 * Button callback. Send the button state over USB and save it for drawing
 *
 * @param evt The button event that occurred
 */
void gamepadButtonCb(buttonEvt_t* evt)
{
    switch(gamepad->gamepadType){
        case GAMEPAD_GENERIC: {
            // Build a list of all independent buttons held down
            gamepad->gpState.buttons = 0;
            if(evt->state & BTN_A)
            {
                gamepad->gpState.buttons |= GAMEPAD_BUTTON_A;
            }
            if(evt->state & BTN_B)
            {
                gamepad->gpState.buttons |= GAMEPAD_BUTTON_B;
            }
            if(evt->state & START)
            {
                gamepad->gpState.buttons |= GAMEPAD_BUTTON_START;
            }
            if(evt->state & SELECT)
            {
                gamepad->gpState.buttons |= GAMEPAD_BUTTON_SELECT;
            }

            // Figure out which way the D-Pad is pointing
            gamepad->gpState.hat = GAMEPAD_HAT_CENTERED;
            if(evt->state & UP)
            {
                if(evt->state & RIGHT)
                {
                    gamepad->gpState.hat = GAMEPAD_HAT_UP_RIGHT;
                }
                else if(evt->state & LEFT)
                {
                    gamepad->gpState.hat = GAMEPAD_HAT_UP_LEFT;
                }
                else
                {
                    gamepad->gpState.hat = GAMEPAD_HAT_UP;
                }
            }
            else if(evt->state & DOWN)
            {
                if(evt->state & RIGHT)
                {
                    gamepad->gpState.hat = GAMEPAD_HAT_DOWN_RIGHT;
                }
                else if(evt->state & LEFT)
                {
                    gamepad->gpState.hat = GAMEPAD_HAT_DOWN_LEFT;
                }
                else
                {
                    gamepad->gpState.hat = GAMEPAD_HAT_DOWN;
                }
            }
            else if(evt->state & RIGHT)
            {
                gamepad->gpState.hat = GAMEPAD_HAT_RIGHT;
            }
            else if(evt->state & LEFT)
            {
                gamepad->gpState.hat = GAMEPAD_HAT_LEFT;
            }

            break;
        }
        case GAMEPAD_NS:{
            // Build a list of all independent buttons held down
            gamepad->gpNsState.buttons = 0;

            if(evt->state & BTN_A)
            {
                gamepad->gpNsState.buttons |= GAMEPAD_NS_BUTTON_A;
            }
            if(evt->state & BTN_B)
            {
                gamepad->gpNsState.buttons |= GAMEPAD_NS_BUTTON_B;
            }
            if(evt->state & START)
            {
                if(evt->state & DOWN){
                    gamepad->gpNsState.buttons |= GAMEPAD_NS_BUTTON_MODE;
                } else {
                    gamepad->gpNsState.buttons |= GAMEPAD_NS_BUTTON_START;
                }
            }
            if(evt->state & SELECT)
            {
                if(evt->state & DOWN){
                    gamepad->gpNsState.buttons |= GAMEPAD_NS_BUTTON_C;
                } else {
                    gamepad->gpNsState.buttons |= GAMEPAD_NS_BUTTON_SELECT;
                }
            }

            // Figure out which way the D-Pad is pointing
            gamepad->gpNsState.hat = GAMEPAD_NS_HAT_CENTERED;
            if(evt->state & UP)
            {
                if(evt->state & RIGHT)
                {
                    gamepad->gpNsState.hat = GAMEPAD_NS_HAT_UP_RIGHT;
                }
                else if(evt->state & LEFT)
                {
                    gamepad->gpNsState.hat = GAMEPAD_NS_HAT_UP_LEFT;
                }
                else
                {
                    gamepad->gpNsState.hat = GAMEPAD_NS_HAT_UP;
                }
            }
            else if(evt->state & DOWN)
            {
                if(evt->state & RIGHT)
                {
                    gamepad->gpNsState.hat = GAMEPAD_NS_HAT_DOWN_RIGHT;
                }
                else if(evt->state & LEFT)
                {
                    gamepad->gpNsState.hat = GAMEPAD_NS_HAT_DOWN_LEFT;
                }
                else
                {
                    gamepad->gpNsState.hat = GAMEPAD_NS_HAT_DOWN;
                }
            }
            else if(evt->state & RIGHT)
            {
                gamepad->gpNsState.hat = GAMEPAD_NS_HAT_RIGHT;
            }
            else if(evt->state & LEFT)
            {
                gamepad->gpNsState.hat = GAMEPAD_NS_HAT_LEFT;
            }

            break;
        }
    }
    
    // Send state to host
    gamepadReportStateToHost();
}

/**
 * @brief TODO
 *
 * @param evt
 */
void gamepadTouchCb(touch_event_t* evt)
{
    switch(gamepad->gamepadType){
        case GAMEPAD_GENERIC: {
            if(evt->down)
            {
                gamepad->gpState.buttons |= touchMap[evt->pad];
            }
            else
            {
                gamepad->gpState.buttons &= ~touchMap[evt->pad];
            }

            break;
        }
        case GAMEPAD_NS: {
            if(evt->down)
            {
                gamepad->gpNsState.buttons |= touchMapNs[evt->pad];
            }
            else
            {
                gamepad->gpNsState.buttons &= ~touchMapNs[evt->pad];
            }

            break;
        }
    }

    // Send state to host
    gamepadReportStateToHost();
}

/**
 * Acceleromoeter callback. Save the state and send it over USB
 *
 * @param accel The last read acceleration value
 */
void gamepadAccelCb(accel_t* accel)
{
    switch(gamepad->gamepadType){
        case GAMEPAD_GENERIC:{
            // Values are roughly -256 to 256, so divide, clamp, and save
            gamepad->gpState.rx = CLAMP((accel->x) / 2, -128, 127);
            gamepad->gpState.ry = CLAMP((accel->y) / 2, -128, 127);
            gamepad->gpState.rz = CLAMP((accel->z) / 2, -128, 127);

            // Send state to host
            gamepadReportStateToHost();
            break;
        }
        default:
            break;
    }

}

/**
 * @brief Send the state over USB to the host
 */
void gamepadReportStateToHost(void)
{
    // Only send data if USB is ready
    if(tud_ready())
    {
        switch(gamepad->gamepadType){
            case GAMEPAD_GENERIC: {
                if(gamepad->gpState.buttons & ((GAMEPAD_BUTTON_C | GAMEPAD_BUTTON_X | GAMEPAD_BUTTON_Y | GAMEPAD_BUTTON_Z | GAMEPAD_BUTTON_TL)))
                {
                    int32_t center, intensity;
                    getTouchCentroid(&center, &intensity);
                    int16_t scaledVal = (center >> 2) - 128;
                    if(scaledVal < -128)
                    {
                        gamepad->gpState.z = -128;
                    }
                    else if (scaledVal > 127)
                    {
                        gamepad->gpState.z = 127;
                    }
                    else
                    {
                        gamepad->gpState.z = scaledVal;
                    }
                }
                else
                {
                    gamepad->gpState.z = 0;
                }
                // Send the state over USB
                tud_gamepad_report(&gamepad->gpState);
                break;
            }
            case GAMEPAD_NS: {
                tud_gamepad_ns_report(&gamepad->gpNsState);
                break;
            }
        }
        
        
    }    
}
