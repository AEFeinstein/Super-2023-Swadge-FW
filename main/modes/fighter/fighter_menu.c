//==============================================================================
// Functions
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "meleeMenu.h"
#include "p2pConnection.h"

#include "fighter_menu.h"
#include "mode_fighter.h"

//==============================================================================
// Enums & Structs
//==============================================================================

typedef enum
{
    FIGHTER_MENU,
    FIGHTER_CONNECTING,
    FIGHTER_WAITING,
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
    fightingCharacter_t characters[2];
    fightingStage_t stage;
} fighterMenu_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void fighterEnterMode(display_t* disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);
void fighterButtonCb(buttonEvt_t* evt);

void setFighterMainMenu(void);
void fighterMainMenuCb(const char* opt);

void setFighterHrMenu(void);
void fighterHrMenuCb(const char* opt);

void setFighterMultiplayerCharSelMenu(void);
void fighterMultiplayerCharMenuCb(const char* opt);

void setFighterMultiplayerStageSelMenu(void);
void fighterMultiplayerStageMenuCb(const char* opt);

void fighterEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
void fighterEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);
void fighterP2pConCbFn(p2pInfo* p2p, connectionEvt_t);
void fighterP2pMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len);
void fighterP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);
void fighterCheckGameBegin(void);

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
static const char str_charSN[]      = "Sunny";
static const char str_charBF[]      = "Big Funkus";
static const char str_back[]        = "Back";

static const char str_stgBF[]       = "Battlefield";
static const char str_stgFD[]       = "Final Destination";

fighterMenu_t* fm;

const char charSelMsg[]       = "1CS";
const char stageSelMsg[]      = "3SS";
const char buttonInputMsg[]   = "3BI";
const char sceneComposedMsg[] = "4SC";

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
    // Allocate and zero memory
    fm = calloc(1, sizeof(fighterMenu_t));

    // Save the display pointer
    fm->disp = disp;

    // Each menu needs a font, so load that first
    loadFont("mm.font", &(fm->mmFont));

    // Create the menu
    fm->menu = initMeleeMenu(str_clobber, &(fm->mmFont), fighterMainMenuCb);

    // Set the main menu
    setFighterMainMenu();

    // Start on the menu
    fm->screen = FIGHTER_MENU;

    // Clear state
    fm->characters[0] = NO_CHARACTER;
    fm->characters[1] = NO_CHARACTER;
    fm->stage = NO_STAGE;

    // Initialize p2p
    p2pInitialize(&(fm->p2p), "FTR", fighterP2pConCbFn, fighterP2pMsgRxCbFn, 0);
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
            fm->disp->clearPx();
            drawText(fm->disp, &fm->mmFont, c543, "Connecting", 0, 0);
            break;
        }
        case FIGHTER_WAITING:
        {
            // TODO spin a wheel or something
            fm->disp->clearPx();
            drawText(fm->disp, &fm->mmFont, c543, "Waiting", 0, 0);
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
            // TODO cancel button?
            break;
        }
        case FIGHTER_WAITING:
        {
            // TODO cancel button?
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

/**
 * @brief TODO doc
 *
 */
void setFighterMainMenu(void)
{
    resetMeleeMenu(fm->menu, str_clobber, fighterMainMenuCb);
    addRowToMeleeMenu(fm->menu, str_multiplayer);
    addRowToMeleeMenu(fm->menu, str_hrContest);
    addRowToMeleeMenu(fm->menu, str_exit);
    fm->menuChanged = true;
    fm->screen = FIGHTER_MENU;
}

/**
 * This is called when a menu option is selected
 *
 * @param opt The option that was selected (string pointer)
 */
void fighterMainMenuCb(const char* opt)
{
    // When a row is clicked, print the label for debugging
    if(opt == str_multiplayer)
    {
        // Show the screen for connecting
        fm->screen = FIGHTER_CONNECTING;
        p2pStartConnection(&fm->p2p);
    }
    else if (opt == str_hrContest)
    {
        // Home Run contest selected, display character select menu
        setFighterHrMenu();
    }
    else if (opt == str_exit)
    {
        // TODO Exit selected
        ESP_LOGI("FTR", "Implement exit");
    }
}

////////////////////////////////////////////////////////////////////////////////

/**
 * @brief TODO doc
 *
 */
void setFighterHrMenu(void)
{
    resetMeleeMenu(fm->menu, str_hrContest, fighterHrMenuCb);
    addRowToMeleeMenu(fm->menu, str_charKD);
    addRowToMeleeMenu(fm->menu, str_charSN);
    addRowToMeleeMenu(fm->menu, str_charBF);
    addRowToMeleeMenu(fm->menu, str_back);
    fm->menuChanged = true;
    fm->screen = FIGHTER_MENU;
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void fighterHrMenuCb(const char* opt)
{
    // These are the same for HR COntest
    fm->stage = HR_STADIUM;
    fm->characters[1] = SANDBAG;

    // Check the menu option selected
    if (opt == str_charKD)
    {
        // King Donut Selected
        fm->characters[0] = KING_DONUT;
    }
    else if (opt == str_charSN)
    {
        // Sunny Selected
        fm->characters[0] = SUNNY;
    }
    else if (opt == str_charBF)
    {
        // Big Funkus Selected
        fm->characters[0] = BIG_FUNKUS;
    }
    else if (opt == str_back)
    {
        // Reset to top level melee menu
        setFighterMainMenu();
        return;
    }
    else
    {
        return;
    }

    // No return, start the game
    fighterStartGame(fm->disp, &fm->mmFont, HR_CONTEST, fm->characters, fm->stage);
    fm->screen = FIGHTER_GAME;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * @brief TODO doc
 *
 */
void setFighterMultiplayerCharSelMenu(void)
{
    resetMeleeMenu(fm->menu, str_multiplayer, fighterMultiplayerCharMenuCb);
    addRowToMeleeMenu(fm->menu, str_charKD);
    addRowToMeleeMenu(fm->menu, str_charSN);
    addRowToMeleeMenu(fm->menu, str_charBF);
    addRowToMeleeMenu(fm->menu, str_back);
    fm->menuChanged = true;
    fm->screen = FIGHTER_MENU;
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void fighterMultiplayerCharMenuCb(const char* opt)
{
    uint8_t charIdx = (GOING_FIRST == fm->p2p.cnc.playOrder) ? 0 : 1;
    if (opt == str_charKD)
    {
        // King Donut Selected
        fm->characters[charIdx] = KING_DONUT;
    }
    else if (opt == str_charSN)
    {
        // Sunny Selected
        fm->characters[charIdx] = SUNNY;
    }
    else if (opt == str_charBF)
    {
        // Big Funkus Selected
        fm->characters[charIdx] = BIG_FUNKUS;
    }
    else if (opt == str_back)
    {
        // Reset to top level melee menu
        setFighterMainMenu();
        return;
    }
    else
    {
        return;
    }

    // No return means a character was selected
    // Send character to the other swadge
    const char payload[] =
    {
        fm->characters[charIdx]
    };
    p2pSendMsg(&fm->p2p, charSelMsg, payload, sizeof(payload), fighterP2pMsgTxCbFn);

    if(GOING_FIRST == fm->p2p.cnc.playOrder)
    {
        // Player going first picks the stage
        setFighterMultiplayerStageSelMenu();
    }
    else
    {
        // Otherwise wait for the stage to be picked
        fm->screen = FIGHTER_WAITING;
    }
}

////////////////////////////////////////////////////////////////////////////////

/**
 * @brief TODO doc
 *
 */
void setFighterMultiplayerStageSelMenu(void)
{
    resetMeleeMenu(fm->menu, str_multiplayer, fighterMultiplayerStageMenuCb);
    addRowToMeleeMenu(fm->menu, str_stgBF);
    addRowToMeleeMenu(fm->menu, str_stgFD);
    addRowToMeleeMenu(fm->menu, str_back);
    fm->menuChanged = true;
    fm->screen = FIGHTER_MENU;
}

/**
 * @brief TODO doc
 *
 * @param opt
 */
void fighterMultiplayerStageMenuCb(const char* opt)
{
    if(str_stgBF == opt)
    {
        fm->stage = BATTLEFIELD;
        printf("Picked battlefield %d", fm->stage);
    }
    else if(str_stgFD == opt)
    {
        fm->stage = FINAL_DESTINATION;
        printf("Final Destination %d", fm->stage);
    }
    else if(str_back == opt)
    {
        // Reset to character select menu
        setFighterMultiplayerCharSelMenu();
        return;
    }
    else
    {
        return;
    }

    // No return means a stage was selected
    // Send stage to other swadge, check if game can start
    const char payload[] =
    {
        fm->stage
    };
    p2pSendMsg(&fm->p2p, stageSelMsg, payload, sizeof(payload), fighterP2pMsgTxCbFn);
}

////////////////////////////////////////////////////////////////////////////////

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
    // TODO things based on connection events
    switch(evt)
    {
        case CON_STARTED:
        {
            break;
        }
        case RX_GAME_START_ACK:
        {
            break;
        }
        case RX_GAME_START_MSG:
        {
            break;
        }
        case CON_ESTABLISHED:
        {
            // Connection established, show character select screen
            setFighterMultiplayerCharSelMenu();
            break;
        }
        case CON_LOST:
        {
            // Reset to top level melee menu
            setFighterMainMenu();
            break;
        }
    }
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
    // TODO things for msg RX CB
    if(msg[0] == charSelMsg[0])
    {
        uint8_t charIdx = (GOING_FIRST == fm->p2p.cnc.playOrder) ? 1 : 0;
        fm->characters[charIdx] = payload[0];
        fighterCheckGameBegin();
    }
    else if(msg[0] == stageSelMsg[0])
    {
        fm->stage = payload[0];

        printf("Received stage %d", fm->stage);

        fighterCheckGameBegin();
    }
    else if(msg[0] == buttonInputMsg[0])
    {

    }
    else if(msg[0] == sceneComposedMsg[0])
    {

    }
}

/**
 * @brief This is the p2p message sent callback
 *
 * @param p2p The p2p struct for this connection
 * @param status The status of the transmission
 */
void fighterP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    // TODO things for msg TX CB
    switch(status)
    {
        case MSG_ACKED:
        {
            fighterCheckGameBegin();
            break;
        }
        case MSG_FAILED:
        {
            setFighterMainMenu();
            break;
        }
    }
}

/**
 * @brief TODO doc
 *
 */
void fighterCheckGameBegin(void)
{
    // Check if the game should be started
    if(fm->screen != FIGHTER_GAME &&
            fm->characters[0] != NO_CHARACTER &&
            fm->characters[1] != NO_CHARACTER &&
            fm->stage != NO_STAGE)
    {
        // Characters and stage set, start the game!
        fighterStartGame(fm->disp, &fm->mmFont, MULTIPLAYER, fm->characters, fm->stage);
        fm->screen = FIGHTER_GAME;
    }
}
