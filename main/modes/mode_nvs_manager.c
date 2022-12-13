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

#include <esp32/rom/crc.h>

#include "bresenham.h"
#include "display.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "espNowUtils.h"
#include "esp_timer.h"
#include "led_util.h"
#include "linked_list.h"
#include "meleeMenu.h"
#include "mode_main_menu.h"
#include "mode_test.h"
#include "nvs_manager.h"
#include "p2pConnection.h"
#include "settingsManager.h"
#ifdef NVS_MANAGER_TOUCH
#include "touch_sensor.h"
#endif
#if defined(EMU)
#include "emu_main.h"
#endif

#include "mode_nvs_manager.h"

/*==============================================================================
 * Defines
 *============================================================================*/

//#define NVS_MANAGER_TOUCH

#define CORNER_OFFSET 14
//#define TOP_TEXT_X_MARGIN CORNER_OFFSET / 2
#define SUMMARY_LINE_BREAK_Y 8
#define LINE_BREAK_Y 4
#define MAX_INT_STRING_LENGTH 21
#define ENTRIES_BUF_SIZE MAX_INT_STRING_LENGTH + 8

#define NVS_VER_LEN 7

/// Helper macro to return an integer clamped within a range (MIN to MAX)
//#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the highest of two integers
//#define MAX(X, Y) ( ((X) > (Y)) ? (X) : (Y) )
//#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/*==============================================================================
 * Enums
 *============================================================================*/



/*==============================================================================
 * Prototypes
 *============================================================================*/

// Standard mode functions
void nvsManagerEnterMode(display_t* disp);
void nvsManagerExitMode(void);
void nvsManagerButtonCallback(buttonEvt_t* evt);
#ifdef NVS_MANAGER_TOUCH
void nvsManagerTouchCallback(touch_event_t* evt);
#endif
void nvsManagerMainLoop(int64_t elapsedUs);

// ESP-NOW functions
void nvsManagerEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
void nvsManagerEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);
void nvsManagerP2pConnect();
void nvsManagerP2pConCbFn(p2pInfo* p2p, connectionEvt_t evt);
void nvsManagerP2pMsgRxCbFn(p2pInfo* p2p, const uint8_t* payload, uint8_t len);
void nvsManagerP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status, const uint8_t* data, uint8_t dataLen);

// Menu functions
void nvsManagerSetUpTopMenu(bool resetPos);
void nvsManagerTopLevelCb(const char* opt);
void nvsManagerSetUpManageDataMenu(bool resetPos);
void nvsManagerManageDataCb(const char* opt);
void nvsManagerSetUpReceiveMenu(bool resetPos);
void nvsManagerReceiveMenuCb(const char* opt);
void nvsManagerSetUpSendConnMenu(bool resetPos);
void nvsManagerSendConnMenuCb(const char* opt);
void nvsManagerSetUpRecvConnMenu(bool resetPos);
void nvsManagerRecvConnMenuCb(const char* opt);

bool nvsManagerReadAllNvsEntryInfos();
char* blobToStrWithPrefix(const void * value, size_t length);
const char* getNvsTypeName(nvs_type_t type);

/*==============================================================================
 * Enums
 *============================================================================*/

/// @brief Defines each separate screen in the NVS manager mode
typedef enum
{
    // Top menu and other menus
    NVS_MENU,
    // Summary of used, free, and total entries in NVS
    NVS_SUMMARY,
    // Warn users about the potential dangers of managing keys
    NVS_WARNING,
    // Manage a specific key/value pair
    NVS_MANAGE_KEY,
    // Send a key/value pair or all data
    NVS_SEND,
    // Receive a key/value pair or all data
    NVS_RECEIVE,
} nvsScreen_t;

/// @brief Defines each action the user can take while viewing a key/value pair in NVS manager
typedef enum
{
    // Erase the key
    NVS_ACTION_ERASE,
    // Send the key to another Swadge
    NVS_ACTION_SEND,
    // Go back (this will always be the last option, so it can be used as a maximum value)
    NVS_ACTION_BACK,
} nvsKeyAction_t;

/// @brief Defines each receive mode in NVS manager
typedef enum
{
    // Receive all NVS key/value pairs from another Swadge
    NVS_RECV_MODE_ALL,
    // Receive several NVS key/value pairs from another Swadge
    NVS_RECV_MODE_MULTI,
    // Receive one NVS key/value pair from another Swadge
    NVS_RECV_MODE_ONE,
} nvsReceiveMode_t;

/// @brief Defines each packet type in NVS manager
typedef enum
{
    // Handshake to choose latest common protocol version. 4-byte CRC32, 7-byte version
    NVS_PACKET_VERSION,
    // 4-byte CRC32, 2-byte number of key/value pairs sender has to send, 4-byte total number of NVS entries occupied by pairs
    NVS_PACKET_NUM_PAIRS_AND_ENTRIES,
    // Metatada about the key/value pair being sent. 4-byte CRC32, 16-byte namespace, 1-byte NVS data type, 4-byte number of data bytes in value, 16-byte key
    NVS_PACKET_PAIR_HEADER,
    // 4-byte CRC32, up to (P2P_MAX_DATA_LEN - 4) bytes data
    NVS_PACKET_VALUE_DATA,
    // Confirm CRC32 OK
    NVS_PACKET_ACK,
    // Indicate CRC32 not OK, request resend of previous packet
    NVS_PACKET_NAK,
    // Indicate unrecoverable p2p error. Both Swadges end session and clean up. 4-byte CRC32, 1-byte error ID
    NVS_PACKET_ERROR,
} nvsPacketType_t;

/// @brief Defines each p2p error type in NVS manager
typedef enum
{
    // Sent by newer NVS manager when support for communicating with old versions has been removed, and the older NVS manager's protocol is too old
    NVS_ERROR_NO_COMMON_VERSION,
    // Receiving Swadge does not have enough total entries (free + used) in NVS to fit everything the sender wants to send
    NVS_ERROR_INSUFFICIENT_SPACE,
    // Either Swadge has received a packet that passed CRC32, but contained unexpected data or packet type for the current state
    NVS_ERROR_UNEXPECTED_PACKET,
    // Either Swadge has been unable to verify CRC32 checksums from the other Swadge too many times
    NVS_ERROR_TOO_MANY_CRC_FAILS,
    // User has aborted the transfer from either side
    NVS_ERROR_USER_ABORT,
    // Any error not defined otherwise
    NVS_OTHER_ERROR,
} nvsCommErrorType_t;

/// @brief Defines each state of p2p in NVS manager
typedef enum
{
    NVS_STATE_NOT_CONNECTED,
    NVS_STATE_WAITING_FOR_PKT_VER,
    NVS_STATE_SENT_PKT_VER,
} nvsCommState_t;

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    char version[7];
} nvsPacketVersion;

typedef struct
{
    uint16_t numEntries;
} nvsPacketNumEntries;

typedef struct
{
    uint8_t 
} nvsPacketEntryHeader;

typedef struct
{
    uint8_t error;
} nvsPacketError;

// The state data
typedef struct
{
    // Display, fonts, wsgs
    display_t* disp;
    font_t ibm_vga8;
    font_t radiostars;
    font_t mm;
    wsg_t ibm_vga8_arrow;

    // Menu
    meleeMenu_t* menu;
    uint16_t topLevelPos;
    uint16_t manageDataPos;
    uint16_t receiveMenuPos;
    nvsKeyAction_t manageKeyAction;
    bool lockManageKeyAction;
    // The screen within NVS manager that the user is in
    nvsScreen_t screen;
    bool eraseDataSelected;
    bool eraseDataConfirm;
    bool warningAccepted;
    void (*fnReturnMenu)(bool resetPos);

#ifdef NVS_MANAGER_TOUCH
    // Track touch
    bool touchHeld;
    int32_t touchPosition;
    int32_t touchIntensity;
#endif

    // Paginated text cache
    list_t pages;
    node_t* curPage;
    uint32_t curPageNum;
    uint16_t loadedRow;
    // Just points to blobStr or numStr
    const char* pageStr;

    ///////////////////
    // Cached NVS info
    ///////////////////

    // General NVS info
    nvs_stats_t nvsStats;
    nvs_entry_info_t* nvsEntryInfos;
    size_t nvsNumEntryInfos;

    // Per-key NVS info
    char* blobStr;
    char numStr[MAX_INT_STRING_LENGTH];
    size_t keyUsedEntries;

    // P2P
    p2pInfo p2p;
    nvsReceiveMode_t receiveMode;
    bool wired;
    uint32_t timeSincePacket;
} nvsManager_t;

/*==============================================================================
 * Variables
 *============================================================================*/

// The swadge mode
swadgeMode modeNvsManager =
{
    .modeName = "Save Data Mgr",
    .fnEnterMode = nvsManagerEnterMode,
    .fnExitMode = nvsManagerExitMode,
    .fnButtonCallback = nvsManagerButtonCallback,
#ifdef NVS_MANAGER_TOUCH
    .fnTouchCallback = nvsManagerTouchCallback,
#else
    .fnTouchCallback = NULL,
#endif
    .fnMainLoop = nvsManagerMainLoop,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = nvsManagerEspNowRecvCb,
    .fnEspNowSendCb = nvsManagerEspNowSendCb,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .overrideUsb = true
};

nvsManager_t* nvsManager;

/*==============================================================================
 * Const Variables
 *============================================================================*/

// Top menu
const char str_summary[] = "Summary";
const char str_manage_data[] = "Manage Data";
const char str_send_all[] = "Send All";
const char str_receive[] = "Receive";
const char str_factory_reset[] = "Factory Reset";
const char str_confirm_no[] = "Confirm: No!";
const char str_confirm_yes[] = "Confirm: Yes";
const char str_back[] = "Back";
const char str_exit[] = "Exit";

// Summary
const paletteColor_t color_summary_text = c555;
const paletteColor_t color_summary_h_rule = c222;
const paletteColor_t color_summary_used = c134;
const paletteColor_t color_summary_free = c333;
const char str_non_volatile_storage[] = "Non-Volatile Storage (nvs:)";
const char str_nvs[] = "NVS";
const char str_type[] = "Type:";
const char str_local_flash_part[] = "Local Flash Partition";
const char str_file_system[] = "File system:";
const char str_used_space[] = "Used space:";
const char str_namespaces[] = "Namespaces:";
const char str_free_space[] = "Free space:";
const char str_capacity[] = "Capacity:";
const char str_1_entry[] = "1 entry";
const char str_entries_format[] = "%zu entries";

// Warning
const paletteColor_t color_warning_bg = c200;
const paletteColor_t color_warning_text = c555;
const char str_warning[] = "WARNING:";
const char str_warning_text[] = "Modifying, erasing, or receiving individual key/value pairs may cause certain modes or features of your Swadge to become unusable until a Factory Reset is performed, or may result in loss of save data.\n\n\nIf you acknowledge these risks and wish to proceed anyway, press Select.\n\n Press any other button to return.";

// Manage key
// Menu border colors: c112, c211, c021, c221, c102, c210,
const paletteColor_t color_key = c335;
const paletteColor_t color_namespace = c533;
const paletteColor_t color_type = c354;
const paletteColor_t color_used_entries = c553;
const paletteColor_t color_value = c435;
//c543
const paletteColor_t color_error = c511;
const char str_key[] = "Key: ";
const char str_namespace[] = "Namespace: ";
const char str_unknown[] = "Unknown";
const char str_read_failed[] = "Error: Failed to read data";
const char str_unknown_type[] = "Error: Unknown type";
const char str_erase[] = "Erase";
const char str_send[] = "Send";
const char str_hex_format[] = "0x%x";
//const char str_u_dec_format[] = "%u";
const char str_i_dec_format[] = "%d";
const char str_page_format[] = "Page %u";

// Send/receive
const char NVS_VERSION[] = "221212a"; // Must be seven chars! yymmddl. Use strcmp, NOT ==
const char str_receive_all[] = "Receive All";
const char str_receive_multi[] = "Receive Multi";
const char str_receive_one[] = "Receive One";
const char str_connection[] = "Connection";
const char str_wireless[] = "Wireless";
const char str_wired[] = "Wired";

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

    loadWsg("arrow10.wsg", &nvsManager->ibm_vga8_arrow);

    nvsManager->loadedRow = UINT16_MAX;

    // TODO: handle errors
    nvsManagerReadAllNvsEntryInfos();

    // Initialize the menu
    nvsManager->menu = initMeleeMenu(modeNvsManager.modeName, &nvsManager->mm, nvsManagerTopLevelCb);
    nvsManagerSetUpTopMenu(true);
}

/**
 * Called when nvsManager is exited
 */
void  nvsManagerExitMode(void)
{
    deinitMeleeMenu(nvsManager->menu);

    freeFont(&nvsManager->ibm_vga8);
    freeFont(&nvsManager->radiostars);
    freeFont(&nvsManager->mm);

    freeWsg(&nvsManager->ibm_vga8_arrow);

    p2pDeinit(&nvsManager->p2p);

    if(nvsManager->nvsEntryInfos != NULL)
    {
        free(nvsManager->nvsEntryInfos);
    }

    if (nvsManager->blobStr != NULL)
    {
        free(nvsManager->blobStr);
        nvsManager->blobStr = NULL;
    }

    clear(&nvsManager->pages);

    free(nvsManager);
}

#ifdef NVS_MANAGER_TOUCH
/**
 * This function is called when a button press is pressed. Buttons are
 * handled by interrupts and queued up for this callback, so there are no
 * strict timing restrictions for this function.
 *
 * @param evt The button event that occurred
 */
void  nvsManagerTouchCallback(touch_event_t* evt)
{
    nvsManager->touchHeld = evt->state != 0;
    nvsManager->touchPosition = roundf((evt->position * nvsManager->disp->w) / 255);
}
#endif

/**
 * @brief Button callback function, plays notes and switches parameters
 *
 * @param evt The button event that occurred
 */
void  nvsManagerButtonCallback(buttonEvt_t* evt)
{
    switch(nvsManager->screen)
    {
        case NVS_MENU:
        {
            if(evt->down)
            {
                const char* selectedOption = nvsManager->menu->rows[nvsManager->menu->selectedRow];

                switch(evt->button)
                {
                    case LEFT:
                    case RIGHT:
                    {
                        if(str_confirm_no == selectedOption)
                        {
                            nvsManager->eraseDataConfirm = true;
                            nvsManagerSetUpTopMenu(false);
                        }
                        else if(str_confirm_yes == selectedOption)
                        {
                            nvsManager->eraseDataConfirm = false;
                            nvsManagerSetUpTopMenu(false);
                        }

                        break;
                    }
                    case BTN_B:
                    {
                        if(nvsManager->menu->title == modeNvsManager.modeName)
                        {
                            if(nvsManager->eraseDataSelected)
                            {
                                nvsManager->eraseDataSelected = false;
                                // This is done later in the function
                                // nvsManager->eraseDataConfirm = false;
                                nvsManagerSetUpTopMenu(false);
                            }
                        }
                        else if(nvsManager->menu->title == str_connection)
                        {
                            nvsManager->fnReturnMenu(false);
                        }
                        else if(nvsManager->menu->title == str_receive_all || nvsManager->menu->title == str_receive_multi || nvsManager->menu->title == str_receive_one)
                        {
                            nvsManagerSetUpReceiveMenu(false);
                        }
                        else
                        {
                            nvsManagerSetUpTopMenu(false);
                        }

                        break;
                    }
                    default:
                    {
                        meleeMenuButton(nvsManager->menu, evt->button);
                        selectedOption = nvsManager->menu->rows[nvsManager->menu->selectedRow];
                        break;
                    }
                }

                if(nvsManager->menu->title == modeNvsManager.modeName && nvsManager->eraseDataSelected && str_confirm_no != selectedOption &&
                    str_confirm_yes != selectedOption)
                {
                    // If the confirm-erase option is not selected, reset eraseDataConfirm and redraw the menu
                    nvsManager->topLevelPos = nvsManager->menu->selectedRow;
                    nvsManager->eraseDataSelected = false;
                    nvsManager->eraseDataConfirm = false;
                    nvsManagerSetUpTopMenu(false);
                }
            }

            break;
        }
        case NVS_SUMMARY:
        {
            if(evt->down && evt->button == BTN_B)
            {
                nvsManagerSetUpTopMenu(false);
            }

            break;
        }
        case NVS_WARNING:
        {
            if(evt->down)
            {
                switch(evt->button)
                {
                    case SELECT:
                    {
                        nvsManager->warningAccepted = true;
                        nvsManagerSetUpManageDataMenu(true);
                        break;
                    }
                    case BTN_B:
                    default:
                    {
                        nvsManager->screen = NVS_MENU;
                        break;
                    }
                }
            }

            break;
        }
        case NVS_MANAGE_KEY:
        {
            if(evt->down)
            {
                switch (evt->button)
                {
                    case BTN_B:
                    {
                        switch(nvsManager->manageKeyAction)
                        {
                            case NVS_ACTION_ERASE:
                            {
                                if(nvsManager->eraseDataSelected)
                                {
                                    nvsManager->eraseDataSelected = false;
                                    nvsManager->eraseDataConfirm = false;
                                    break;
                                }

                                // Intentional fallthrough
                            }
                            case NVS_ACTION_BACK:
                            default:
                            {
                                nvsManagerSetUpManageDataMenu(false);
                            }
                        }
                        break;
                    }
                    case UP:
                    {
                        if(nvsManager->curPage != NULL && nvsManager->curPage->prev != NULL)
                        {
                            nvsManager->curPage = nvsManager->curPage->prev;
                            nvsManager->curPageNum--;
                        }
                        break;
                    }
                    case DOWN:
                    {
                        if(nvsManager->curPage != NULL && nvsManager->curPage->next != NULL)
                        {
                            nvsManager->curPage = nvsManager->curPage->next;
                            nvsManager->curPageNum++;
                        }
                        break;
                    }
                    case LEFT:
                    case RIGHT:
                    {
                        switch(nvsManager->manageKeyAction)
                        {
                            case NVS_ACTION_ERASE:
                            {
                                if(nvsManager->eraseDataConfirm)
                                {
                                    nvsManager->eraseDataConfirm = false;
                                    break;
                                }
                                else if(nvsManager->eraseDataSelected)
                                {
                                    nvsManager->eraseDataConfirm = true;
                                    break;
                                }

                                // Intentional fallthrough
                            }
                            case NVS_ACTION_BACK:
                            default:
                            {
                                if(nvsManager->lockManageKeyAction)
                                {
                                    break;
                                }

                                if(evt->button == LEFT)
                                {
                                    nvsManager->manageKeyAction = (nvsManager->manageKeyAction + 1) % NVS_ACTION_BACK;
                                }
                                else // button == RIGHT
                                {
                                    if(nvsManager->manageKeyAction == 0)
                                    {
                                        nvsManager->manageKeyAction = NVS_ACTION_BACK;
                                    }
                                    else
                                    {
                                        nvsManager->manageKeyAction--;
                                    }
                                }
                                break;
                            }
                        }
                        break;
                    }
                    case BTN_A:
                    {
                        switch(nvsManager->manageKeyAction)
                        {
                            case NVS_ACTION_ERASE:
                            {
                                if(!nvsManager->eraseDataSelected)
                                {
                                    nvsManager->eraseDataSelected = true;
                                }
                                else
                                {
                                    if(nvsManager->eraseDataConfirm)
                                    {
                                        // TODO: handle errors
                                        eraseNvsKey(nvsManager->nvsEntryInfos[nvsManager->loadedRow].key);
                                        nvsManager->eraseDataSelected = false;
                                        nvsManager->eraseDataConfirm = false;
                                        // TODO: handle errors
                                        nvsManagerReadAllNvsEntryInfos();
                                        nvsManagerSetUpManageDataMenu(false);
                                    }
                                    else
                                    {
                                        nvsManager->eraseDataSelected = false;
                                    }
                                }
                                break;
                            }
                            case NVS_ACTION_SEND:
                            {
                                break;
                            }
                            case NVS_ACTION_BACK:
                            {
                                nvsManagerSetUpManageDataMenu(false);
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            break;
        }
        case NVS_SEND:
        {
            if(evt->down)
            {
                switch(evt->button)
                {
                    case BTN_B:
                    {
                        nvsManagerSetUpSendConnMenu(false);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }

            break;
        }
        case NVS_RECEIVE:
        {
            if(evt->down)
            {
                switch(evt->button)
                {
                    case BTN_B:
                    {
                        nvsManagerSetUpRecvConnMenu(false);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }

            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * Update the display by drawing the current state of affairs
 */
void  nvsManagerMainLoop(int64_t elapsedUs)
{
    switch(nvsManager->screen)
    {
        case NVS_MENU:
        {
            // Draw the menu
            drawMeleeMenu(nvsManager->disp, nvsManager->menu);
            break;
        }
        case NVS_SUMMARY:
        {
            nvsManager->disp->clearPx();

            led_t leds[NUM_LEDS];
            memset(leds, 0, NUM_LEDS * sizeof(led_t));
            setLeds(leds, NUM_LEDS);

            char buf[ENTRIES_BUF_SIZE];

            // Partition name
            int16_t yOff = CORNER_OFFSET;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_non_volatile_storage, CORNER_OFFSET, yOff);

            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y + 1;
            plotLine(nvsManager->disp, CORNER_OFFSET, yOff, nvsManager->disp->w - CORNER_OFFSET, yOff, color_summary_h_rule, 0);

            // Partition type
            yOff += SUMMARY_LINE_BREAK_Y + 1;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_type, CORNER_OFFSET, yOff);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_local_flash_part, nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, str_local_flash_part) - CORNER_OFFSET, yOff);

            // Partition file system
            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_file_system, CORNER_OFFSET, yOff);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_nvs, nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, str_nvs) - CORNER_OFFSET, yOff);

            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y + 1;
            plotLine(nvsManager->disp, CORNER_OFFSET, yOff, nvsManager->disp->w - CORNER_OFFSET, yOff, color_summary_h_rule, 0);

            // Used space
            yOff += SUMMARY_LINE_BREAK_Y + 1;
            fillDisplayArea(nvsManager->disp, CORNER_OFFSET, yOff, CORNER_OFFSET + nvsManager->ibm_vga8.h, yOff + nvsManager->ibm_vga8.h, color_summary_used);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_used_space, CORNER_OFFSET + nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y, yOff);
            snprintf(buf, ENTRIES_BUF_SIZE, str_entries_format, nvsManager->nvsStats.used_entries);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, nvsManager->nvsStats.used_entries == 1 ? str_1_entry : buf, nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, buf) - CORNER_OFFSET, yOff);

            // Namespaces
            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_namespaces, CORNER_OFFSET + nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y, yOff);
            snprintf(buf, ENTRIES_BUF_SIZE, str_entries_format, nvsManager->nvsStats.namespace_count);
            int16_t tWidth = textWidth(&nvsManager->ibm_vga8, buf);
            snprintf(buf, ENTRIES_BUF_SIZE, "%zu", nvsManager->nvsStats.namespace_count);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, buf, nvsManager->disp->w - tWidth - CORNER_OFFSET, yOff);

            // Free space
            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y;
            fillDisplayArea(nvsManager->disp, CORNER_OFFSET, yOff, CORNER_OFFSET + nvsManager->ibm_vga8.h, yOff + nvsManager->ibm_vga8.h, color_summary_free);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_free_space, CORNER_OFFSET + nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y, yOff);
            snprintf(buf, ENTRIES_BUF_SIZE, str_entries_format, nvsManager->nvsStats.free_entries);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, nvsManager->nvsStats.free_entries == 1 ? str_1_entry : buf, nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, buf) - CORNER_OFFSET, yOff);

            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y + 1;
            plotLine(nvsManager->disp, CORNER_OFFSET, yOff, nvsManager->disp->w - CORNER_OFFSET, yOff, color_summary_h_rule, 0);

            // Capacity
            yOff += SUMMARY_LINE_BREAK_Y + 1;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_capacity, CORNER_OFFSET + nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y, yOff);
            snprintf(buf, ENTRIES_BUF_SIZE, str_entries_format, nvsManager->nvsStats.total_entries);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, buf, nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, buf) - CORNER_OFFSET, yOff);

            yOff += nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y;
            int16_t xStart = CORNER_OFFSET + nvsManager->ibm_vga8.h + SUMMARY_LINE_BREAK_Y;
            int16_t xEnd = nvsManager->disp->w - CORNER_OFFSET - nvsManager->ibm_vga8.h - SUMMARY_LINE_BREAK_Y;
            fillDisplayArea(nvsManager->disp, xStart, yOff, xStart + roundf((float_t)nvsManager->nvsStats.used_entries / nvsManager->nvsStats.total_entries * (xEnd - xStart)), yOff + nvsManager->ibm_vga8.h, color_summary_used);
            fillDisplayArea(nvsManager->disp, xEnd - roundf((float_t)nvsManager->nvsStats.free_entries / nvsManager->nvsStats.total_entries * (xEnd - xStart)), yOff, xEnd, yOff + nvsManager->ibm_vga8.h, color_summary_free);

            break;
        }
        case NVS_WARNING:
        {
            led_t leds[NUM_LEDS];
            for(uint8_t i = 0; i < NUM_LEDS; i++)
            {
                leds[i].r = 0xFF;
                leds[i].g = 0x00;
                leds[i].b = 0x00;
            }
            setLeds(leds, NUM_LEDS);

            fillDisplayArea(nvsManager->disp, 0, 0, nvsManager->disp->w, nvsManager->disp->h, color_warning_bg);

            int16_t yOff = CORNER_OFFSET;
            drawText(nvsManager->disp, &nvsManager->mm, color_warning_text, str_warning, (nvsManager->disp->w - textWidth(&nvsManager->mm, str_warning)) / 2, yOff);

            yOff += nvsManager->mm.h + nvsManager->ibm_vga8.h * 2 + 3;
            int16_t xOff = CORNER_OFFSET;
            drawTextWordWrap(nvsManager->disp, &nvsManager->ibm_vga8, color_warning_text, str_warning_text, &xOff, &yOff, nvsManager->disp->w - CORNER_OFFSET, nvsManager->disp->h - CORNER_OFFSET);
            break;
        }
        case NVS_MANAGE_KEY:
        {
            nvsManager->disp->clearPx();

            led_t leds[NUM_LEDS];
            memset(leds, 0, NUM_LEDS * sizeof(led_t));
            setLeds(leds, NUM_LEDS);

            nvs_entry_info_t entryInfo = nvsManager->nvsEntryInfos[nvsManager->menu->selectedRow];

            // Key
            int16_t yOff = CORNER_OFFSET;
            const int16_t afterLongestLabel = CORNER_OFFSET + textWidth(&nvsManager->ibm_vga8, str_used_space) + textWidth(&nvsManager->ibm_vga8, " ") + 1;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_key, CORNER_OFFSET, yOff);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_key, entryInfo.key, afterLongestLabel, yOff);

            // Namespace
            yOff +=  nvsManager->ibm_vga8.h + LINE_BREAK_Y;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_namespace, CORNER_OFFSET, yOff);
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_namespace, entryInfo.namespace_name, afterLongestLabel, yOff);

            // Type
            yOff +=  nvsManager->ibm_vga8.h + LINE_BREAK_Y;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_type, CORNER_OFFSET, yOff);
            const char* typeName = getNvsTypeName(entryInfo.type);
            char typeAsHex[5];
            if(typeName[0] == '\0')
            {
                snprintf(typeAsHex, 5, str_hex_format, entryInfo.type);
            }
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_type, typeName[0] == '\0' ? typeAsHex : typeName, afterLongestLabel, yOff);

            // Prepare for getting the value, and pagination
            bool readSuccess = false;
            if (nvsManager->loadedRow != nvsManager->menu->selectedRow)
            {

                nvsManager->curPage = NULL;
                nvsManager->curPageNum = 0;
                clear(&nvsManager->pages);

                if (nvsManager->blobStr != NULL)
                {
                    free(nvsManager->blobStr);
                    nvsManager->blobStr = NULL;
                }

                nvsManager->loadedRow = nvsManager->menu->selectedRow;
                nvsManager->keyUsedEntries = 0;

                // Get the value
                bool foundType = true;

                switch(entryInfo.type)
                {
                    case NVS_TYPE_I32:
                    {
                        int32_t val;
                        readSuccess = readNvs32(entryInfo.key, &val);
                        if(readSuccess)
                        {
                            snprintf(nvsManager->numStr, MAX_INT_STRING_LENGTH, str_i_dec_format, val);
                            nvsManager->keyUsedEntries = 1;
                        }
                        break;
                    }
                    case NVS_TYPE_BLOB:
                    {
                        size_t length;
                        readSuccess = readNvsBlob(entryInfo.key, NULL, &length);
                        if(readSuccess)
                        {
                            char* blob = calloc(1, length);
                            readSuccess = readNvsBlob(entryInfo.key, blob, &length);
                            if(readSuccess)
                            {
                                nvsManager->blobStr = blobToStrWithPrefix(blob, length);
                                /**
                                 * When the ESP32 is storing blobs, it uses 1 entry to index chunks,
                                 * 1 entry per chunk, then 1 entry for every 32 bytes of data, rounding up.
                                 *
                                 * I don't know how to find out how many chunks the ESP32 would split
                                 * certain length blobs into, so for now I'm assuming 1 chunk per blob.
                                 *
                                 * TODO: find a way to get number of entries or chunks in the blob
                                 */
                                nvsManager->keyUsedEntries = 2 + ceil((float) length / NVS_ENTRY_BYTES);
                            }
                            free(blob);
                        }
                        break;
                    }
                    case NVS_TYPE_U8:
                    case NVS_TYPE_I8:
                    case NVS_TYPE_U16:
                    case NVS_TYPE_I16:
                    case NVS_TYPE_U32:
                    case NVS_TYPE_U64:
                    case NVS_TYPE_I64:
                    case NVS_TYPE_STR:
                    case NVS_TYPE_ANY:
                    default:
                    {
                        foundType = false;
                        break;
                    }
                }

                if(foundType)
                {
                    if(readSuccess)
                    {
                        switch(entryInfo.type)
                        {
                            case NVS_TYPE_U8:
                            case NVS_TYPE_I8:
                            case NVS_TYPE_U16:
                            case NVS_TYPE_I16:
                            case NVS_TYPE_U32:
                            case NVS_TYPE_I32:
                            case NVS_TYPE_U64:
                            case NVS_TYPE_I64:
                            {
                                nvsManager->pageStr = nvsManager->numStr;
                                break;
                            }

                            case NVS_TYPE_BLOB:
                            {
                                nvsManager->pageStr = nvsManager->blobStr;
                                break;
                            }

                            case NVS_TYPE_STR:
                            case NVS_TYPE_ANY:
                            default:
                            {
                                nvsManager->pageStr = str_unknown_type;
                                break;
                            }
                        }
                    }
                    else
                    {
                        nvsManager->pageStr = str_read_failed;
                    }
                }
                else
                {
                    nvsManager->pageStr = str_unknown_type;
                }
            }

            // Used space
            yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_used_space, CORNER_OFFSET, yOff);
            if(nvsManager->keyUsedEntries == 1)
            {
                drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_used_entries, str_1_entry, afterLongestLabel, yOff);
            }
            else if(nvsManager->keyUsedEntries > 1)
            {
                char buf[ENTRIES_BUF_SIZE];
                snprintf(buf, ENTRIES_BUF_SIZE, str_entries_format, nvsManager->keyUsedEntries);
                drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_used_entries, buf, afterLongestLabel, yOff);
            }
            else
            {
                drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_error, str_unknown, afterLongestLabel, yOff);
            }

            yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y + 1;
            plotLine(nvsManager->disp, 0, yOff, nvsManager->disp->w, yOff, color_summary_h_rule, 0);

            //////////////////////////////////////////////
            // Value
            //////////////////////////////////////////////
            yOff += LINE_BREAK_Y + 1;
            int16_t xOff = CORNER_OFFSET;
            int16_t newYOff = nvsManager->disp->h - CORNER_OFFSET - nvsManager->ibm_vga8.h * 2 - LINE_BREAK_Y * 3 - 2;
            const char* nextText;
            if (nvsManager->curPage == NULL)
            {
                // Add the beginning of the text as the first page
                push(&nvsManager->pages, nvsManager->pageStr);
                nvsManager->curPage = nvsManager->pages.first;
                nvsManager->curPageNum = 1;
            }

            // Now, draw the text, which will always be in the current page
            nextText = drawTextWordWrap(nvsManager->disp, &nvsManager->ibm_vga8, color_value, (const char*)(nvsManager->curPage->val),
                                                        &xOff, &yOff, nvsManager->disp->w - CORNER_OFFSET, newYOff);

            if (nextText != NULL && nvsManager->curPage->next == NULL)
            {
                // Save the next page if it hasn't already been saved
                push(&nvsManager->pages, nextText);
            }

            // if(nextText != NULL)
            // {
            //     drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_value, "...", nvsManager->disp->w - CORNER_OFFSET, yOff);
            // }

            // Pagination controls, page number
            yOff = newYOff + LINE_BREAK_Y;
            if(nvsManager->curPage->next != NULL || nvsManager->curPage->prev != NULL)
            {
                drawWsg(nvsManager->disp, &nvsManager->ibm_vga8_arrow, CORNER_OFFSET, yOff, false, false, 0);
                char buf[ENTRIES_BUF_SIZE];
                snprintf(buf, ENTRIES_BUF_SIZE, str_page_format, nvsManager->curPageNum);
                drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, buf, (nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, buf)) / 2, yOff);
                drawWsg(nvsManager->disp, &nvsManager->ibm_vga8_arrow, nvsManager->disp->w - nvsManager->ibm_vga8_arrow.w - CORNER_OFFSET, yOff, false, true, 0);
            }

            yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y + 1;
            plotLine(nvsManager->disp, 0, yOff, nvsManager->disp->w, yOff, color_summary_h_rule, 0);

            // Controls
            yOff += LINE_BREAK_Y + 1;
            // If the read failed or the key's namespace isn't the one we have access to, hide all actions except "Back"
            if(nvsManager->pageStr == str_read_failed || strcmp(entryInfo.namespace_name, NVS_NAMESPACE_NAME))
            {
                nvsManager->manageKeyAction = NVS_ACTION_BACK;
                nvsManager->lockManageKeyAction = true;
            }
            if(!nvsManager->lockManageKeyAction)
            {
                drawWsg(nvsManager->disp, &nvsManager->ibm_vga8_arrow, CORNER_OFFSET, yOff, false, false, 270);
            }
            const char* actionStr;
            switch(nvsManager->manageKeyAction)
            {
                case NVS_ACTION_ERASE:
                {
                    if(nvsManager->eraseDataConfirm)
                    {
                        actionStr = str_confirm_yes;
                    }
                    else if(nvsManager->eraseDataSelected)
                    {
                        actionStr = str_confirm_no;
                    }
                    else
                    {
                        actionStr = str_erase;
                    }
                    break;
                }
                case NVS_ACTION_SEND:
                {
                    actionStr = str_send;
                }
                case NVS_ACTION_BACK:
                {
                    actionStr = str_back;
                    break;
                }
                default:
                {
                    actionStr = "";
                    break;
                }
            }
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, actionStr, (nvsManager->disp->w - textWidth(&nvsManager->ibm_vga8, actionStr)) / 2, yOff);
            if(!nvsManager->lockManageKeyAction)
            {
                drawWsg(nvsManager->disp, &nvsManager->ibm_vga8_arrow, nvsManager->disp->w - nvsManager->ibm_vga8_arrow.h - CORNER_OFFSET, yOff, false, false, 90);
            }

            break;
        }
        case NVS_SEND:
        case NVS_RECEIVE:
        default:
        {
            nvsManager->disp->clearPx();
            break;
        }
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
void nvsManagerEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
    // Forward to p2p
    p2pRecvCb(&nvsManager->p2p, mac_addr, (const uint8_t*)data, len, rssi);
}

/**
 * This function is called whenever an ESP-NOW packet is sent.
 * It is just a status callback whether or not the packet was actually sent.
 * This will be called after calling espNowSend()
 *
 * @param mac_addr The MAC address which the data was sent to
 * @param status   The status of the transmission
 */
void nvsManagerEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    // Forward to p2p
    p2pSendCb(&nvsManager->p2p, mac_addr, status);
}

void nvsManagerP2pConnect()
{
    // Initialize p2p
    p2pDeinit(&(nvsManager->p2p));
    p2pInitialize(&(nvsManager->p2p), nvsManager->screen == NVS_SEND ? 'N' : 'O', nvsManagerP2pConCbFn, nvsManagerP2pMsgRxCbFn, -35);
    p2pSetAsymmetric(&paintShare->p2pInfo, nvsManager->screen == NVS_SEND ? 'O' : 'N');
    // Start the connection
    p2pStartConnection(&nvsManager->p2p);

    // Turn off LEDs
    led_t leds[NUM_LEDS] = {0};
    setLeds(leds, NUM_LEDS);
}

/**
 * @brief This is the p2p connection callback
 *
 * @param p2p The p2p struct for this connection
 * @param evt The connection event that occurred
 */
void nvsManagerP2pConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    switch(evt)
    {
        case CON_STARTED:
        case RX_GAME_START_ACK:
        case RX_GAME_START_MSG:
        {
            break;
        }
        case CON_ESTABLISHED:
        {
            
            break;
        }
        case CON_LOST:
        {
            nvsManagerP2pConnect();

            // We don't want to time out while waiting for a connection
            nvsManager->timeSincePacket = 0;
            if (isSender())
            {
                nvsManager->shareState = SHARE_SEND_WAIT_FOR_CONN;
            }
            else
            {
                nvsManager->shareState = SHARE_RECV_WAIT_FOR_CONN;
            }
            break;
        }
    }
}

/**
 * @brief This is the p2p message receive callback
 *
 * @param p2p The p2p struct for this connection
 * @param payload The payload for the message
 * @param len The length of the message
 */
void nvsManagerP2pMsgRxCbFn(p2pInfo* p2p, const uint8_t* payload, uint8_t len)
{
    switch(nvsManager->screen)
    {
        case NVS_SEND:
        {
            // Check what was received
            switch(payload[0])
            {
                case NVS_PACKET_VERSION:
                {
                    if(0 == memcmp(&payload[1], FTR_VERSION, VER_LEN))
                    {
                        // Connection established, show character select screen
                        setFighterMultiplayerCharSelMenu(true);
                    }
                    else
                    {
                        // Version mismatch, show warning
                        nvsManager->ftrConnectingStringTop = str_version_mismatch;
                        nvsManager->ftrConnectingStringBottom = str_visit_swadge_com;
                    }

                    // Reply with version
                    if(GOING_SECOND == p2pGetPlayOrder(p2p))
                    {
                        nvsManagerSendVersionToOther();
                    }
                    break;
                }
                case NVS_PACKET_NUM_PAIRS_AND_ENTRIES:
                {
                    break;
                }
                case NVS_PACKET_PAIR_HEADER:
                {
                    break;
                }
                case NVS_PACKET_VALUE_DATA:
                {
                    break;
                }
                case NVS_PACKET_ACK:
                {
                    break;
                }
                case NVS_PACKET_NAK:
                {
                    break;
                }
                case NVS_PACKET_ERROR:
                {
                    break;
                }
            }
            break;
        }
        case FIGHTER_GAME:
        {
            if(BUTTON_INPUT_MSG == payload[0])
            {
                // Receive button inputs, so save them
                fighterRxButtonInput(payload[1]);
            }
            break;
        }
        case NVS_RECEIVE:
        {

        }
        case NVS_MENU:
        case NVS_WARNING:
        case NVS_SUMMARY:
        case NVS_WARNING:
        {
            // These screens don't receive packets
            break;
        }
    }
}

/**
 * @brief This is the p2p message sent callback
 *
 * @param p2p The p2p struct for this connection
 * @param status The status of the transmission
 * @param data Data received in the ACK, may be NULL
 * @param dataLen Length of the data received in the ACK, may be 0
 */
void nvsManagerP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status, const uint8_t* data, uint8_t dataLen)
{
    // Check what was ACKed or failed
    switch(status)
    {
        case MSG_ACKED:
        {
            // After character or stage selection, check if the game can begin
            if (nvsManager->lastSentMsg == CHAR_SEL_MSG || nvsManager->lastSentMsg == STAGE_SEL_MSG)
            {
                fighterCheckGameBegin();
            }
            // If a button input message was acked while playing the game
            else if((nvsManager->lastSentMsg == BUTTON_INPUT_MSG) && (FIGHTER_GAME == nvsManager->screen))
            {
                // If there is any data
                if(dataLen > 0)
                {
                    // If this is a scene
                    if(MP_COMPOSED_SCENE_MSG == data[0])
                    {
                        // Pull composed scene out of the ack, setup for render
                        fighterRxScene((const fighterScene_t*) data, dataLen);
                        // Then send buttons again
                        fighterSendButtonsToOther(fighterGetButtonState());
                    }
                    // Or if this is a game over
                    else if(MP_GAME_OVER_MSG == data[0])
                    {
                        // Show the result
                        const fighterMpGameResult_t* res = (const fighterMpGameResult_t*)data;
                        initFighterMpResult(nvsManager->disp, &nvsManager->mmFont, res->roundTimeMs,
                                            res->other, res->otherKOs, res->otherDmg,
                                            res->self, res->selfKOs, res->selfDmg, MULTIPLAYER);
                        nvsManager->screen = FIGHTER_MP_RESULT;

                        // Deinit the game
                        fighterExitGame();
                    }
                }
                else
                {
                    // There was no data in the ACK, but keep the loop
                    // alive by sending button state again
                    fighterSendButtonsToOther(fighterGetButtonState());
                }
            }
            break;
        }
        case MSG_FAILED:
        {
            setFighterMainMenu(false);
            break;
        }
    }
}

/**
 * @brief Send a packet to the other swadge with this NVS manager's version
 */
void nvsManagerSendVersionToOther(void)
{
    uint8_t payload[1 + NVS_VER_LEN] = {VERSION_MSG};
    memcpy(&payload[1], NVS_VERSION, NVS_VER_LEN);
    // Send button state to the other swawdge
    p2pSendMsg(&fm->p2p, payload, sizeof(payload), nvsManagerP2pMsgTxCbFn);
    fm->lastSentMsg = VERSION_MSG;
}

uint32_t nvsCalcCrc32(const uint8_t* buffer, size_t length)
{
    return (~crc32_le((uint32_t)~(0xffffffff), buffer, length))^0xffffffff;
}

/**
 * Set up the top level menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void nvsManagerSetUpTopMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(nvsManager->menu, modeNvsManager.modeName, nvsManagerTopLevelCb);
    addRowToMeleeMenu(nvsManager->menu, str_summary);
    addRowToMeleeMenu(nvsManager->menu, str_manage_data);
    addRowToMeleeMenu(nvsManager->menu, str_send_all);
    addRowToMeleeMenu(nvsManager->menu, str_receive);

    // Add the row for factory resetting the Swadge
    if (nvsManager->eraseDataSelected)
    {
        if (nvsManager->eraseDataConfirm)
        {
            addRowToMeleeMenu(nvsManager->menu, str_confirm_yes);
        }
        else
        {
            addRowToMeleeMenu(nvsManager->menu, str_confirm_no);
        }
    }
    else
    {
        addRowToMeleeMenu(nvsManager->menu, str_factory_reset);
    }

    addRowToMeleeMenu(nvsManager->menu, str_exit);

    // Set the position
    if(resetPos)
    {
        nvsManager->topLevelPos = 0;
    }
    nvsManager->menu->selectedRow = nvsManager->topLevelPos;
    nvsManager->menu->usePerRowXOffsets = true;

    nvsManager->screen = NVS_MENU;
}

/**
 * Callback for the top level menu
 *
 * @param opt The menu option which was selected
 */
void nvsManagerTopLevelCb(const char* opt)
{
    // Save the position
    nvsManager->topLevelPos = nvsManager->menu->selectedRow;

    // Handle the option
    if(str_summary == opt)
    {
        nvsManager->screen = NVS_SUMMARY;
    }
    else if(str_manage_data == opt)
    {
        if(!nvsManager->warningAccepted)
        {
            nvsManager->screen = NVS_WARNING;
        }
        else
        {
            nvsManagerSetUpManageDataMenu(true);
        }
    }
    else if(str_send_all == opt)
    {
        nvsManager->fnReturnMenu = nvsManagerSetUpTopMenu;
        nvsManagerSetUpSendConnMenu(true);
    }
    else if(str_receive == opt)
    {
        nvsManagerSetUpReceiveMenu(true);
    }
    else if(str_factory_reset == opt)
    {
        nvsManager->eraseDataSelected = true;
        nvsManagerSetUpTopMenu(false);
    }
    else if (str_confirm_yes == opt)
    {
        // If this succeeds, we shouldn't let someone back into the main menu, so switch to test mode immediately
        // If this fails, we have no idea what state NVS is in, so send them back to the main menu and pray
        // The emulator will encounter issues too unless it's immediately closed
        if(eraseNvs())
        {
#ifdef EMU
            emu_quit();
#else
            switchToSwadgeMode(&modeTest);
#endif
        }
        else
        {
#ifdef EMU
            exit(1);
#else
            switchToSwadgeMode(&modeMainMenu);
#endif
        }
        nvsManager->eraseDataConfirm = false;
        nvsManager->eraseDataSelected = false;
    }
    else if (str_confirm_no == opt)
    {
        nvsManager->eraseDataSelected = false;
        nvsManager->eraseDataConfirm = false;
        nvsManagerSetUpTopMenu(false);
    }
    else if(str_exit == opt)
    {
        switchToSwadgeMode(&modeMainMenu);
    }
}

/**
 * Set up the data management menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void nvsManagerSetUpManageDataMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(nvsManager->menu, str_manage_data, nvsManagerManageDataCb);

    for(size_t i = 0; i < nvsManager->nvsNumEntryInfos; i++)
    {
        if(nvsManager->nvsEntryInfos[i].key[0] != '\0')
        {
            addRowToMeleeMenu(nvsManager->menu, nvsManager->nvsEntryInfos[i].key);
        }
    }

    addRowToMeleeMenu(nvsManager->menu, str_back);

    // Set the position
    if(resetPos)
    {
        nvsManager->manageDataPos = 0;
    }
    nvsManager->menu->selectedRow = nvsManager->manageDataPos;
    nvsManager->menu->usePerRowXOffsets = false;
    nvsManager->eraseDataSelected = false;
    nvsManager->eraseDataConfirm = false;
}

/**
 * Callback for the data management menu
 *
 * @param opt The menu option which was selected
 */
void nvsManagerManageDataCb(const char* opt)
{
    // Save the position
    nvsManager->manageDataPos = nvsManager->menu->selectedRow;

    // Handle the option
    if(str_back == opt)
    {
        nvsManagerSetUpTopMenu(false);
    }
    else
    {
        nvsManager->loadedRow = UINT16_MAX;
        nvsManager->manageKeyAction = 0;
        nvsManager->lockManageKeyAction = false;
        nvsManager->screen = NVS_MANAGE_KEY;
    }
}

/**
 * Set up the receive menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void nvsManagerSetUpReceiveMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(nvsManager->menu, str_receive, nvsManagerReceiveMenuCb);
    addRowToMeleeMenu(nvsManager->menu, str_receive_all);
    addRowToMeleeMenu(nvsManager->menu, str_receive_multi);
    addRowToMeleeMenu(nvsManager->menu, str_receive_one);
    addRowToMeleeMenu(nvsManager->menu, str_back);

    // Set the position
    if(resetPos)
    {
        nvsManager->receiveMenuPos = 0;
    }
    nvsManager->menu->selectedRow = nvsManager->receiveMenuPos;
}

/**
 * Callback for the receive menu
 *
 * @param opt The menu option which was selected
 */
void nvsManagerReceiveMenuCb(const char* opt)
{
    // Save the position
    nvsManager->receiveMenuPos = nvsManager->menu->selectedRow;

    // Handle the option
    if(str_receive_all == opt)
    {
        nvsManager->receiveMode = NVS_RECV_MODE_ALL;
        nvsManagerSetUpRecvConnMenu(true);
    }
    else if(str_receive_multi == opt)
    {
        nvsManager->receiveMode = NVS_RECV_MODE_MULTI;
        nvsManagerSetUpRecvConnMenu(true);
    }
    else if(str_receive_one == opt)
    {
        nvsManager->receiveMode = NVS_RECV_MODE_ALL;
        nvsManagerSetUpRecvConnMenu(true);
    }
    else if(str_back == opt)
    {
        nvsManagerSetUpTopMenu(false);
    }
}

/**
 * Set up the sending connection menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void nvsManagerSetUpSendConnMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(nvsManager->menu, str_connection, nvsManagerSendConnMenuCb);
    addRowToMeleeMenu(nvsManager->menu, str_wireless);
    addRowToMeleeMenu(nvsManager->menu, str_wired);
    addRowToMeleeMenu(nvsManager->menu, str_back);

    // Set the position
    if(resetPos)
    {
        nvsManager->connMenuPos = 0;
    }
    nvsManager->menu->selectedRow = nvsManager->connMenuPos;
}

/**
 * Callback for the sending connection menu
 *
 * @param opt The menu option which was selected
 */
void nvsManagerSendConnMenuCb(const char* opt)
{
    // Save the position
    nvsManager->connMenuPos = nvsManager->menu->selectedRow;

    // Handle the option
    if(str_wireless == opt)
    {
        nvsManager->wired = false;
        espNowUseWireless();
        nvsManager->screen = NVS_SEND;
    }
    else if(str_wired == opt)
    {
        nvsManager->wired = true;
        espNowUseWired();
        nvsManager->screen = NVS_SEND;
    }
    else if(str_back == opt)
    {
        nvsManagerSetUpTopMenu(false);
    }
}

/**
 * Set up the receiving connection menu
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void nvsManagerSetUpRecvConnMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(nvsManager->menu, str_connection, nvsManagerRecvConnMenuCb);
    addRowToMeleeMenu(nvsManager->menu, str_wireless);
    addRowToMeleeMenu(nvsManager->menu, str_wired);
    addRowToMeleeMenu(nvsManager->menu, str_back);

    // Set the position
    if(resetPos)
    {
        nvsManager->connMenuPos = 0;
    }
    nvsManager->menu->selectedRow = nvsManager->connMenuPos;
}

/**
 * Callback for the receiving connection menu
 *
 * @param opt The menu option which was selected
 */
void nvsManagerRecvConnMenuCb(const char* opt)
{
    // Save the position
    nvsManager->connMenuPos = nvsManager->menu->selectedRow;

    // Handle the option
    if(str_wireless == opt)
    {
        nvsManager->wired = false;
        espNowUseWireless();
        nvsManager->screen = NVS_RECEIVE;
    }
    else if(str_wired == opt)
    {
        nvsManager->wired = true;
        espNowUseWired();
        nvsManager->screen = NVS_RECEIVE;
    }
    else if(str_back == opt)
    {
        nvsManagerSetUpRecvConnMenu(false);
    }
}

bool nvsManagerReadAllNvsEntryInfos()
{
    // Save the existing number of allocated entry infos so we know if we can reuse the old allocation
    size_t oldNvsNumEntryInfos = nvsManager->nvsNumEntryInfos;

    // Get the number of entry infos so we can allocate the memory
    if(!readAllNvsEntryInfos(&nvsManager->nvsStats, NULL, &nvsManager->nvsNumEntryInfos))
    {
        return false;
    }

    // If nvsEntryInfos is already allocated and it's not going to be big enough, free it first
    if(nvsManager->nvsEntryInfos != NULL && oldNvsNumEntryInfos < nvsManager->nvsNumEntryInfos)
    {
        free(nvsManager->nvsEntryInfos);
        nvsManager->nvsEntryInfos = NULL;
    }

    // If we just freed nvsEntryInfos or it wasn't allocated to begin with, allocate it
    if(nvsManager->nvsEntryInfos == NULL)
    {
        nvsManager->nvsEntryInfos = calloc(nvsManager->nvsNumEntryInfos, sizeof(nvs_entry_info_t));
    }

    // Do the actual read of entry infos
    return readAllNvsEntryInfos(&nvsManager->nvsStats, &nvsManager->nvsEntryInfos, &nvsManager->nvsNumEntryInfos);
}

/**
 * @brief Convert a blob to a hex string, with "0x" prefix
 *
 * @param value The blob
 * @param length The length of the blob
 * @return char* An allocated hex string, must be free()'d when done
 */
char* blobToStrWithPrefix(const void * value, size_t length)
{
    const uint8_t * value8 = (const uint8_t *)value;
    char * blobStr = malloc((length * 2) + 3);
    size_t i = 0;
    sprintf(&blobStr[i*2], "%s", "0x");
    for(i++; i < length; i++)
    {
        sprintf(&blobStr[i*2], "%02X", value8[i]);
    }
    return blobStr;
}

const char* getNvsTypeName(nvs_type_t type)
{
    switch(type)
    {
#ifdef USING_MORE_THAN_I32_AND_BLOB
        case NVS_TYPE_U8:
        {
            return "8-bit unsigned integer";
        }
        case NVS_TYPE_I8:
        {
            return "8-bit signed integer";
        }
        case NVS_TYPE_U16:
        {
            return "16-bit unsigned integer";
        }
        case NVS_TYPE_I16:
        {
            return "16-bit signed integer";
        }
        case NVS_TYPE_U32:
        {
            return "32-bit unsigned integer";
        }
#endif
        case NVS_TYPE_I32:
        {
            return "32-bit signed integer";
        }
#ifdef USING_MORE_THAN_I32_AND_BLOB
        case NVS_TYPE_U64:
        {
            return "64-bit unsigned integer";
        }
        case NVS_TYPE_I64:
        {
            return "64-bit signed integer";
        }
        case NVS_TYPE_STR:
        {
            return "String";
        }
#endif
        case NVS_TYPE_BLOB:
        {
            return "Blob";
        }
        case NVS_TYPE_ANY:
        default:
        {
            return "";
        }
    }
}
