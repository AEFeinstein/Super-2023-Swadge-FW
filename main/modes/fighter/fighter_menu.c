//==============================================================================
// Functions
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "meleeMenu.h"
#include "fighter_menu.h"
#include "mode_fighter.h"
#include "p2pConnection.h"

//==============================================================================
// Enums & Structs
//==============================================================================

typedef enum
{
    FIGHTER_MENU,
    FIGHTER_CONNECTING,
    FIGHTER_GAME
} fighterScreen_t;

typedef struct
{
    font_t mmFont;
    meleeMenu_t* menu;
    display_t* disp;
    fighterScreen_t screen;
    p2pInfo p2p;
    bool menuChanged;
} fighterMenu_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void fighterEnterMode(display_t* disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);
void fighterButtonCb(buttonEvt_t* evt);
void fighterMenuCb(const char* opt);

void fighterEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
void fighterEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);
void fighterP2pConCbFn(p2pInfo* p2p, connectionEvt_t);
void fighterP2pMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len);
void fighterP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

//==============================================================================
// Variables
//==============================================================================

swadgeMode modeFighter =
{
    .modeName = "Fighter",
    .fnEnterMode = fighterEnterMode,
    .fnExitMode = fighterExitMode,
    .fnMainLoop = fighterMainLoop,
    .fnButtonCallback = fighterButtonCb,
    .fnTouchCallback = NULL, // fighterTouchCb,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = fighterEspNowRecvCb,
    .fnEspNowSendCb = fighterEspNowSendCb,
    .fnAccelerometerCallback = NULL, // fighterAccelerometerCb,
    .fnAudioCallback = NULL, // fighterAudioCb,
    .fnTemperatureCallback = NULL, // fighterTemperatureCb
};

static const char str_clobber[]     = "Clobber";
static const char str_multiplayer[] = "Multiplayer";
static const char str_hrContest[]   = "HR Contest";
static const char str_exit[]        = "Exit";

static const char str_charKD[]      = "King Donut";
static const char str_charS[]       = "Sunny";
static const char str_charBF[]      = "Big Funkus";
static const char str_back[]        = "Back";

fighterMenu_t* fm;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Enter the fighter mode by displaying the top level menu
 *
 * @param disp The display to draw to
 */
void fighterEnterMode(display_t* disp)
{
    fm = calloc(1, sizeof(fighterMenu_t));

    fm->screen = FIGHTER_MENU;

    // Save the display pointer
    fm->disp = disp;

    // Each menu needs a font, so load that first
    loadFont("mm.font", &(fm->mmFont));

    // Create the menu
    fm->menu = initMeleeMenu(str_clobber, &(fm->mmFont), fighterMenuCb);
    fm->menuChanged = true;

    // Add the rows
    addRowToMeleeMenu(fm->menu, str_multiplayer);
    addRowToMeleeMenu(fm->menu, str_hrContest);
    addRowToMeleeMenu(fm->menu, str_exit);

    p2pInitialize(&(fm->p2p), "FTR",
                  fighterP2pConCbFn, fighterP2pMsgRxCbFn, 0);
}

/**
 * @brief Exit the fighter mode by freeing all resources
 */
void fighterExitMode(void)
{
    fighterExitGame();
    deinitMeleeMenu(fm->menu);
    p2pDeinit(&fm->p2p);
    freeFont(&(fm->mmFont));
    free(fm);
}

/**
 * Call the appropriate main loop function for the screen being displayed
 *
 * @param elapsedUs Microseconds since this function was last called
 */
void fighterMainLoop(int64_t elapsedUs)
{
    switch(fm->screen)
    {
        case FIGHTER_MENU:
        {
            // Redraw the menu if there's been a change
            if(fm->menuChanged)
            {
                drawMeleeMenu(fm->disp, fm->menu);
                fm->menuChanged = false;
            }
            break;
        }
        case FIGHTER_GAME:
        {
            // Loop the game
            fighterGameLoop(elapsedUs);
            break;
        }
        case FIGHTER_CONNECTING:
        {
            // TODO spin a wheel or something
            break;
        }
    }
}

/**
 * Call the appropriate button function for the screen being displayed
 *
 * @param evt
 */
void fighterButtonCb(buttonEvt_t* evt)
{
    switch(fm->screen)
    {
        case FIGHTER_MENU:
        {
            // Pass button events from the Swadge mode to the menu
            if(evt->down)
            {
                fm->menuChanged = true;
                meleeMenuButton(fm->menu, evt->button);
            }
            break;
        }
        case FIGHTER_GAME:
        {
            // Pass button events from the Swdage mode to the game
            fighterGameButtonCb(evt);
            break;
        }
        case FIGHTER_CONNECTING:
        {
            // TODO have a cancel button?
            break;
        }
    }
}

/**
 * This is called when a menu option is selected
 *
 * @param opt The option that was selected (string pointer)
 */
void fighterMenuCb(const char* opt)
{
    // When a row is clicked, print the label for debugging
    if(opt == str_multiplayer)
    {
        ESP_LOGI("FTR", "Implement multiplayer");
        // TODO connect -> choose char -> choose stage -> play
    }
    else if (opt == str_hrContest)
    {
        // Home Run contest selected, display character select menu
        resetMeleeMenu(fm->menu, str_hrContest);
        addRowToMeleeMenu(fm->menu, str_charKD);
        addRowToMeleeMenu(fm->menu, str_charS);
        addRowToMeleeMenu(fm->menu, str_charBF);
        addRowToMeleeMenu(fm->menu, str_back);
    }
    else if (opt == str_charKD)
    {
        // King Donut Selected
        if(fm->menu->title == str_hrContest)
        {
            // Selection in Home Run Contest
            fightingCharacter_t fChars[] = {KING_DONUT, SANDBAG};
            fighterStartGame(fm->disp, &fm->mmFont, HR_CONTEST, fChars, HR_STADIUM);
            fm->screen = FIGHTER_GAME;
        }
        else if(fm->menu->title == str_multiplayer)
        {
            // Selection in Multiplayer
            // TODO select stage
            ESP_LOGI("FTR", "King Donut Multiplayer");
        }
    }
    else if (opt == str_charS)
    {
        // Sunny Selected
        if(fm->menu->title == str_hrContest)
        {
            // Selection in Home Run Contest
            fightingCharacter_t fChars[] = {SUNNY, SANDBAG};
            fighterStartGame(fm->disp, &fm->mmFont, HR_CONTEST, fChars, HR_STADIUM);
            fm->screen = FIGHTER_GAME;
        }
        else if(fm->menu->title == str_multiplayer)
        {
            // Selection in Multiplayer
            // TODO select stage
            ESP_LOGI("FTR", "Sunny Multiplayer");
        }
    }
    else if (opt == str_charBF)
    {
        // Big Funkus Selected
        if(fm->menu->title == str_hrContest)
        {
            // Selection in Home Run Contest
            fightingCharacter_t fChars[] = {BIG_FUNKUS, SANDBAG};
            fighterStartGame(fm->disp, &fm->mmFont, HR_CONTEST, fChars, HR_STADIUM);
            fm->screen = FIGHTER_GAME;
        }
        else if(fm->menu->title == str_multiplayer)
        {
            // Selection in Multiplayer
            // TODO select stage
            ESP_LOGI("FTR", "Big Funkus Multiplayer");
        }
    }
    else if (opt == str_back)
    {
        // Back selected from Home Run Contest
        if(fm->menu->title == str_hrContest)
        {
            // Reset to top level melee menu
            resetMeleeMenu(fm->menu, str_clobber);
            addRowToMeleeMenu(fm->menu, str_multiplayer);
            addRowToMeleeMenu(fm->menu, str_hrContest);
            addRowToMeleeMenu(fm->menu, str_exit);
            fm->menuChanged = true;
        }
        // TODO others
    }
    else if (opt == str_exit)
    {
        // TODO Exit selected
        ESP_LOGI("FTR", "TODO Implement exit");
    }
}

/**
 * This function is called whenever an ESP-NOW packet is received.
 *
 * @param mac_addr The MAC address which sent this data
 * @param data     A pointer to the data received
 * @param len      The length of the data received
 * @param rssi     The RSSI for this packet, from 1 (weak) to ~90 (touching)
 */
void fighterEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
    // Forward to p2p
    p2pRecvCb(&fm->p2p, mac_addr, data, len, rssi);
}

/**
 * This function is called whenever an ESP-NOW packet is sent.
 * It is just a status callback whether or not the packet was actually sent.
 * This will be called after calling espNowSend()
 *
 * @param mac_addr The MAC address which the data was sent to
 * @param status   The status of the transmission
 */
void fighterEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    // Forward to p2p
    p2pSendCb(&fm->p2p, mac_addr, status);
}

/**
 * @brief This is the p2p connection callback
 *
 * @param p2p The p2p struct for this connection
 * @param evt The connection event that occurred
 */
void fighterP2pConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    // TODO
}

/**
 * @brief This is the p2p message receive callback
 *
 * @param p2p The p2p struct for this connection
 * @param msg The message that was received
 * @param payload The payload for the message
 * @param len The length of the message
 */
void fighterP2pMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len)
{
    // TODO
}

/**
 * @brief This is the p2p message sent callback
 *
 * @param p2p The p2p struct for this connection
 * @param status The status of the transmission
 */
void fighterP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    // TODO
}
