/*
 * mode_nvs_manager.c
 *
 *  Created on: 3 Dec 2022
 *      Author: bryce
 */

// mandatory to-dos to get comms working properly
// TODO: fix totalNumPackets, numPacketsRemaining being something like 4 million when they should be on the order of tens to thousands at MOST
// TODO: investigate sendEntryInfoIdces[n] being 0 when it should not be
// TODO: investigate incomingEntryInfoIdx ending up at 29 somewhere
// TODO: send NVS_ERROR_USER_ABORT errors at appropriate points

// optional to-dos to increase other modes' robustness
// TODO: paint data recovery in the event one or more NVS keys are missing
// TODO: sort scores in tiltrads on each NVS load
// TODO: memset progress data in picross to 0 when NVS read fails

// TODO: disable this when finished debugging
#define DEBUG

/*==============================================================================
 * Includes
 *============================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

//#include <esp32/rom/crc.h>

#include "bresenham.h"
#include "display.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "espNowUtils.h"
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
#ifdef DEBUG
#include "esp_log.h"
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
#define NVS_MAX_DATA_BYTES_PER_PACKET P2P_MAX_DATA_LEN - sizeof(nvsPacket_t)
#define NVS_MAX_ERROR_MESSAGE_LEN P2P_MAX_DATA_LEN - sizeof(nvsPacket_t) - sizeof(nvsCommError_t)
#define NVS_TOTAL_PACKETS_UP_TO_INCL_NUM_PAIRS_ENTRIES 5 // Does not include the ACK for NVS_PACKET_NUM_PAIRS_AND_ENTRIES
#define NVS_MIN_PACKETS_PER_PAIR 8

/// Helper macro to return an integer clamped within a range (MIN to MAX)
//#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the lowest of two integers
#define MIN(X, Y) ( ((X) < (Y)) ? (X) : (Y) )
/// Helper macro to return the highest of two integers
//#define MAX(X, Y) ( ((X) > (Y)) ? (X) : (Y) )
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/**
 * When the ESP32 is storing blobs, it uses 1 entry to index chunks,
 * 1 entry per chunk, then 1 entry for every 32 bytes of data, rounding up.
 *
 * I don't know how to find out how many chunks the ESP32 would split
 * certain length blobs into, so for now I'm assuming 1 chunk per blob.
 *
 * TODO: find a way to get number of entries or chunks in the blob
 */
#define getNumBlobEntries(length) 2 + ceil((float) length / NVS_ENTRY_BYTES)

#define getNumPacketsNeededToSendPair(bytes) (NVS_MIN_PACKETS_PER_PAIR + 4 /*NVS_PACKET_READY, ACK, NVS_PACKET_DATA_VALUE, ACK*/ * (ceil(bytes / (float) NVS_MAX_DATA_BYTES_PER_PACKET) - 1 /*first data value loop already counted in NVS_MIN_PACKETS_PER_PAIR*/))

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

/// @brief Defines each send mode in NVS manager
typedef enum
{
    // Send all NVS key/value pairs to another Swadge
    NVS_SEND_MODE_ALL,
    // Send one NVS key/value pair yo another Swadge
    NVS_SEND_MODE_ONE,
} nvsSendMode_t;

/// @brief Defines each receive mode in NVS manager
typedef enum
{
    // Receive all NVS key/value pairs from another Swadge
    NVS_RECV_MODE_ALL,
    // Receive several individual NVS key/value pairs from another Swadge. Pairs will be applied to NVS after each transaction, and comms will be restarted without making the user enter settings again
    NVS_RECV_MODE_MULTI,
    // Receive one NVS key/value pair from another Swadge
    NVS_RECV_MODE_ONE,
} nvsReceiveMode_t;

/// @brief Defines each packet type in NVS manager
typedef enum
{
    // Handshake to choose latest common protocol version. 7-byte version
    NVS_PACKET_VERSION,
    // 4-byte number of key/value pairs sender has to send, 4-byte total number of NVS entries occupied by pairs, 4-byte total number of packets in full transaction
    NVS_PACKET_NUM_PAIRS_AND_ENTRIES,
    // Metatada about the key/value pair being sent. 16-byte namespace, 1-byte NVS data type, 4-byte number of data bytes in value, 16-byte key
    NVS_PACKET_PAIR_HEADER,
    // Up to (P2P_MAX_DATA_LEN - 1) bytes data
    NVS_PACKET_VALUE_DATA,
    // Signal readiness for next packet in sequence
    NVS_PACKET_READY,
    // Indicate unrecoverable p2p error. Both Swadges end session and clean up. 1-byte error ID, NVS_MAX_ERROR_MESSAGE_LEN bytes text
    NVS_PACKET_ERROR,
} nvsPacket_t;

/// @brief Defines each p2p error type in NVS manager
typedef enum
{
    // Sent by newer NVS manager when support for communicating with old versions has been removed, and the older NVS manager's protocol is too old
    NVS_ERROR_NO_COMMON_VERSION,
    // Sending Swadge was unable to find any sendable key/value pairs
    NVS_ERROR_NO_SENDABLE_PAIRS,
    // Receiving Swadge does not have enough total entries (free + used) in NVS to fit everything the sender wants to send
    NVS_ERROR_INSUFFICIENT_SPACE,
    // Receiving Swadge is set to the wrong receive mode for number of key/value pairs the sender wants to send
    NVS_ERROR_WRONG_RECEIVE_MODE,
    // Either Swadge has received a packet that contained unexpected data or packet type for the current state
    NVS_ERROR_UNEXPECTED_PACKET,
    // User has aborted the transfer from either side
    NVS_ERROR_USER_ABORT,
} nvsCommError_t;

/*
/// @brief Defines each packet decode error type in NVS manager, including a 0 "OK" value
typedef enum
{
    // No error was encountered while decoding the packet
    NVS_OK = 0,
    // Packet contained unexpected data or packet type for the packet decoding function
    NVS_PKTERR_UNEXPECTED = NVS_ERROR_UNEXPECTED_PACKET,
} nvsPacketDecodeError_t;
*/

/// @brief Defines each state of p2p in NVS manager
typedef enum
{
    // Both Swages, no connection yet
    NVS_STATE_NOT_CONNECTED,
    // Both Swadges, initiating connection
    NVS_STATE_WAITING_FOR_CONNECTION,
    // Both Swadges, connected, waiting for version
    NVS_STATE_WAITING_FOR_VERSION,
    // Sending Swadge, sent NVS_PACKET_NUM_PAIRS_AND_ENTRIES
    NVS_STATE_SENT_NUM_PAIRS_ENTRIES,
    // Sending Swadge, sent NVS_PACKET_PAIR_HEADER
    NVS_STATE_SENT_PAIR_HEADER,
    // Sending Swadge, sent NVS_PACKET_VALUE_DATA
    NVS_STATE_SENT_VALUE_DATA,
    // Receiving Swadge, waiting for NVS_PACKET_NUM_PAIRS_AND_ENTRIES
    NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES,
    // Receiving Swadge, waiting for NVS_PACKET_PAIR_HEADER
    NVS_STATE_WAITING_FOR_PAIR_HEADER,
    // Receiving Swadge, waiting for NVS_PACKET_VALUE_DATA
    NVS_STATE_WAITING_FOR_VALUE_DATA,
    // Both Swadges, communication is done
    NVS_STATE_DONE,
    // Both Swadges, communication failed
    NVS_STATE_FAILED,
} nvsCommState_t;

/// @brief Defines each state of per-packet p2p ACKs in NVS manager
typedef enum
{
    NVS_PSTATE_NOTHING_SENT_YET,
    NVS_PSTATE_WAIT_FOR_ACK,
    NVS_PSTATE_SUCCESS,
} nvsPacketState_t;

/*==============================================================================
 * Structs
 *============================================================================*/

#ifdef EMU
typedef struct
#else
typedef struct __attribute__((packed))
#endif
{
    char version[NVS_VER_LEN];
} nvsPacketVersion_t;

typedef struct __attribute__((packed))
{
    uint32_t numPairs;
    uint32_t numEntries; // Used solely for checking NVS partition space
    uint32_t numPackets; // Used solely for drawing progress bar
} nvsPacketNumPairsEntries_t;

typedef struct __attribute__((packed))
{
    char namespace[NVS_KEY_NAME_MAX_SIZE]; // Least significant byte
    nvs_type_t nvsType;
    uint32_t numValueBytes;
    char key[NVS_KEY_NAME_MAX_SIZE]; // Most significant byte
} nvsPacketPairHeader_t;

typedef struct __attribute__((packed))
{
    nvsCommError_t error;
    char message[NVS_MAX_ERROR_MESSAGE_LEN];
} nvsPacketError_t;

typedef struct
{
    char* data;
    size_t numBytes;
} nvsValue_t;

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
    uint16_t connMenuPos;
    uint16_t applyMenuPos;
    nvsKeyAction_t manageKeyAction;
    bool lockManageKeyAction;
    nvsScreen_t screen; // The screen within NVS manager that the user is in
    bool eraseDataSelected;
    bool eraseDataConfirm;
    bool warningAccepted;
    void (*fnReturnMenu)(bool resetPos);
    void (*fnNextMenu)(bool resetPos);

#ifdef NVS_MANAGER_TOUCH
    // Track touch
    bool touchHeld;
    int32_t touchPosition;
    int32_t touchIntensity;
#endif

    ///////////////////
    // Cached NVS info
    ///////////////////

    // General NVS info
    nvs_stats_t nvsStats;
    nvs_entry_info_t* nvsEntryInfos;
    uint32_t nvsNumEntryInfos;
    uint32_t nvsAllocatedEntryInfos;

    // Per-key NVS info
    char* blobStr;
    char numStr[MAX_INT_STRING_LENGTH];
    uint16_t keyUsedEntries;

    // Paginated text cache
    list_t pages;
    node_t* curPage;
    uint32_t curPageNum;
    uint16_t loadedRow;
    // Just points to blobStr or numStr
    const char* pageStr;

    ///////////////////
    // P2P
    ///////////////////

    p2pInfo p2p;
    nvsSendMode_t sendMode;
    nvsReceiveMode_t receiveMode;
    bool wired;
    uint32_t timeSincePacket;
    nvsCommState_t commState;
    nvsPacketState_t packetState;
    char commError[NVS_MAX_ERROR_MESSAGE_LEN];
    bool commErrorIsFromHere;

    uint8_t* lastPacketSent;
    size_t lastPacketLen;

    uint32_t totalNumPairs;
    size_t totalNumBytesInPair;
    uint32_t totalNumPackets; // Used solely for drawing progress bar
    uint32_t* sendableEntryInfoIndices;
    uint32_t curSendableEntryInfoIndicesIdx;

    uint32_t numPairsRemaining; // Does not include pair being actively sent. To check if sending is done, check both this and `numBytesRemainingInPair`
    size_t numBytesRemainingInPair;
    uint32_t numPacketsRemaining; // Used solely for drawing progress bar

    char* blob;
    int32_t num;

    nvs_entry_info_t* incomingNvsEntryInfos;
    uint32_t numIncomingEntryInfos;
    nvsValue_t* incomingNvsValues;
    bool appliedAnyData;
    bool appliedLatestData;
} nvsManager_t;

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

// ESP-NOW/P2P functions
void nvsManagerEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
void nvsManagerEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);
void nvsManagerP2pConnect(void);
void nvsManagerP2pSendMsg(p2pInfo* p2p, const uint8_t* payload, uint16_t len, p2pMsgTxCbFn msgTxCbFn);
void nvsManagerP2pConCbFn(p2pInfo* p2p, connectionEvt_t evt);
void nvsManagerP2pMsgRxCbFn(p2pInfo* p2p, const uint8_t* payload, uint8_t len);
void nvsManagerP2pMsgTxCbFn(p2pInfo* p2p, messageStatus_t status, const uint8_t* data, uint8_t dataLen);
void nvsManagerStopP2p(void);
void nvsManagerFailP2p(void);
void nvsManagerInitIncomingNvsImage(uint32_t numPairs);
void nvsManagerDeinitIncomingNvsImage(void);
bool applyIncomingNvsImage(void);
//uint32_t nvsCalcCrc32(const uint8_t* buffer, size_t length);
void nvsManagerSendVersion(void);
uint8_t* nvsManagerEncodePacketVersion(nvsPacketVersion_t packetAsStruct, size_t* outLength);
void nvsManagerDecodePacketVersion(const uint8_t* packetAsBytes, size_t length, nvsPacketVersion_t* outPacketAsStruct);
void nvsManagerSendNumPairsEntries(void);
uint8_t* nvsManagerEncodePacketNumPairsEntries(nvsPacketNumPairsEntries_t packetAsStruct, size_t* outLength);
void nvsManagerDecodePacketNumPairsEntries(const uint8_t* packetAsBytes, size_t length, nvsPacketNumPairsEntries_t* outPacketAsStruct);
void nvsManagerSendPairHeader(nvs_entry_info_t* entryInfo);
uint8_t* nvsManagerEncodePacketPairHeader(nvsPacketPairHeader_t packetAsStruct, size_t* outLength);
void nvsManagerDecodePacketPairHeader(const uint8_t* packetAsBytes, size_t length, nvsPacketPairHeader_t* outPacketAsStruct);
void nvsManagerSendValueData(uint8_t* data, size_t dataLength);
uint8_t* nvsManagerEncodePacketValueData(uint8_t* data, size_t dataLength, size_t* outLength);
void nvsManagerDecodePacketValueData(const uint8_t* packetAsBytes, size_t length, uint8_t* outData, size_t* outDataLength);
void nvsManagerSendReady(void);
void nvsManagerSendError(nvsCommError_t error, const char* message);
uint8_t* nvsManagerEncodePacketError(nvsPacketError_t packetAsStruct, size_t* outLength);
void nvsManagerDecodePacketError(const uint8_t* packetAsBytes, size_t length, nvsPacketError_t* outPacketAsStruct);
void nvsManagerComposeUnexpectedPacketErrorMessage(nvsPacket_t receivedPacketType, bool receivedAck, char** outMessage);
void nvsManagerComposeAndSendUnexpectedPacketErrorMessage(nvsPacket_t receivedPacketType, bool receivedAck);

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
void nvsManagerSetUpRecvApplyMenu(bool resetPos);
void nvsManagerRecvApplyMenuCb(const char* opt);

/// NVS functions
bool nvsManagerReadAllNvsEntryInfos(void);
char* blobToStrWithPrefix(const void * value, size_t length);
bool nvsManagerReadNvsKeyValuePair(nvs_entry_info_t* entryInfo, int32_t* outNum, char** outBlobPtr, uint16_t* outEntries, size_t* outBytes);
const char* getNvsTypeName(nvs_type_t type);

//Debug functions
#ifdef DEBUG
void printSendPacketDebug(const uint8_t* payload);
void printRecvPacketDebug(const uint8_t* payload);
void printSendStateDebug(void);
void printRecvStateDebug(void);
#endif

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

// Send/receive internals
const char NVS_VERSION[] = "221212a"; // Must be seven chars! yymmddl. No NULL char. Use memcmp, NOT ==, and NOT strcmp.

// Send/receive menus
const char str_receive_all[] = "Receive All";
const char str_receive_multi[] = "Receive Multi";
const char str_receive_one[] = "Receive One";
const char str_connection[] = "Connection";
const char str_wireless[] = "Wireless";
const char str_wired[] = "Wired";
const char str_apply_data[] = "Apply Data";
const char str_choose_one[] = "Factory Reset?:";
const char str_reset_first[] = "Reset first";
const char str_apply_wo_reset[] = "Apply w/o reset";

// Send/receive errors
const char str_no_sendable_pairs[] = "Unable to find any sendable key/value pairs";
const char str_empty_key[] = "Received empty key";
const char str_wrong_receive_mode[] = "Wrong recieve mode (Sender is set to Send All, so Receiver should be set to Receive All)";
const char str_ack_of[] = "ACK of ";
const char str_ack[] = "ACK";
const char str_nothing[] = "nothing";
const char str_version[] = "version";
const char str_num_pairs_entries[] = "num pairs and entries";
const char str_pair_header[] = "pair header";
const char str_value_data[] = "value data";
const char str_ready[] = "ready";
const char str_error[] = "error";
const char str_unknown_lowercase[] = "unknown";
const char str_packet[] = " packet";
const char str_insufficient_space[] = "Insufficient total space in NVS partition";
const char str_zero_pairs[] = "Received 0 pairs";
const char str_zero_entries[] = "Received 0 entries";
const char str_few[] = "few";
const char str_many[] = "many";
const char str_first_version_error_format[] = "Received NVS manager protocol version %s, which is earlier than first version %s"; // received version, first version
const char str_unexpected_packet_msg_format[] = "Expected %s%s%s, but received %s%s"; // str_ack_of|"", what was expected, str_packet|"", str_ack|what was received, str_packet|""
const char str_not_enough_packets_format[] = "Received too small number of packets (%u) for number of pairs (%u)"; // number packets, number pairs
const char str_less_entries_than_pairs_format[] = "Received smaller number entries (%u) than number of pairs (%u)"; // number entries, number pairs
const char str_inaccessible_namespace_format[] = "Expected namespace \"%s\", but received namespace \"%s\""; // expected namespace, received namespace
const char str_unknown_type_format[] = "Received unknown NVS data type 0x%x"; // nvs_type_t
const char str_too_x_bytes_for_type_format[] = "Received too %s bytes (%u) for NVS data type (%s)"; // str_few|str_more, received bytes, NVS type string from `getNvsTypeName`
const char str_value_data_too_x_bytes[] = "Value data packet contained too %s bytes (expected %zu, but received %zu)"; // str_few|str_more, expected byes, received bytes

// Send/receive screen text
const char str_not_connected[] = "Not connected";
const char str_connecting[] = "Connecting";
const char str_exchanging_version_info[] = "Exchanging version info";
const char str_sending_format[] = "Sending %s";
const char str_receiving_format[] = "Receiving %s";
const char str_done[] = "Done";
const char str_failed[] = "Failed";
const char str_other_swadge_sent_error[] = "Other Swadge sent error:";
const char str_any_button_to_return[] = "Press any button to return";
const char str_any_button_to_continue[] = "Press any button to continue";
const char str_b_to_cancel[] = "Press B to cancel";
const char str_received_data_applied_successfully[] = "Received data applied successfully";

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

    nvsManagerStopP2p();

    if(nvsManager->nvsEntryInfos != NULL)
    {
        free(nvsManager->nvsEntryInfos);
        nvsManager->nvsEntryInfos = NULL;
    }

    if (nvsManager->blobStr != NULL)
    {
        free(nvsManager->blobStr);
        nvsManager->blobStr = NULL;
    }

    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
        nvsManager->lastPacketSent = NULL;
    }

    if(nvsManager->sendableEntryInfoIndices != NULL)
    {
        free(nvsManager->sendableEntryInfoIndices);
        nvsManager->sendableEntryInfoIndices = NULL;
    }

    nvsManagerDeinitIncomingNvsImage();

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
                        nvsManager->fnNextMenu(true);
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
                            case NVS_ACTION_SEND:
                            {
                                nvsManagerSetUpSendConnMenu(true);
                                break;
                            }
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
                            case NVS_ACTION_SEND:
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
                                nvsManager->sendMode = NVS_SEND_MODE_ONE;
                                nvsManager->fnReturnMenu = nvsManagerSetUpManageDataMenu;
                                nvsManagerSetUpSendConnMenu(true);
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
                        nvsManagerStopP2p();
                        nvsManagerSetUpSendConnMenu(false);
                        break;
                    }
                    case BTN_A:
                    {
                        // If failed, reconnect. If done, exit. Otherwise, do nothing
                        if(nvsManager->commState == NVS_STATE_FAILED)
                        {
                            nvsManagerP2pConnect();
                        }
                        else if(nvsManager->commState == NVS_STATE_DONE)
                        {
                            nvsManagerSetUpSendConnMenu(false);
                        }

                        break;
                    }
                    default:
                    {
                        if(nvsManager->commState == NVS_STATE_DONE || nvsManager->commState == NVS_STATE_FAILED)
                        {
                            nvsManagerSetUpSendConnMenu(false);
                        }
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
                        nvsManagerStopP2p();
                        nvsManagerSetUpRecvConnMenu(false);
                        break;
                    }
                    case BTN_A:
                    {
                        if(nvsManager->receiveMode == NVS_RECV_MODE_ALL && nvsManager->commState == NVS_STATE_DONE)
                        {
                            nvsManagerSetUpRecvApplyMenu(true);
                        }
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

                // Get the value
                int32_t val;
                char* blob = NULL;
                size_t blobLen;

                readSuccess = nvsManagerReadNvsKeyValuePair(&entryInfo, &val, &blob, &nvsManager->keyUsedEntries, &blobLen);

                if(typeName[0] != '\0')
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
                                snprintf(nvsManager->numStr, MAX_INT_STRING_LENGTH, str_i_dec_format, val);
                                nvsManager->pageStr = nvsManager->numStr;
                                break;
                            }
                            case NVS_TYPE_BLOB:
                            {
                                nvsManager->blobStr = blobToStrWithPrefix(blob, blobLen);
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

                if(blob != NULL)
                {
                    free(blob);
                    blob = NULL;
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
                snprintf(buf, ENTRIES_BUF_SIZE, str_entries_format, (size_t) nvsManager->keyUsedEntries);
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
            if(nvsManager->pageStr == str_read_failed || strncmp(entryInfo.namespace_name, NVS_NAMESPACE_NAME, NVS_KEY_NAME_MAX_SIZE))
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
                    break;
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
        {
            led_t leds[NUM_LEDS];
            memset(leds, 0, NUM_LEDS * sizeof(led_t));
            setLeds(leds, NUM_LEDS);

            // Set background to red if comms failed
            fillDisplayArea(nvsManager->disp, 0, 0, nvsManager->disp->w, nvsManager->disp->h, nvsManager->commState == NVS_STATE_FAILED ? color_warning_bg : c000);
            
            // Title
            int16_t yOff = CORNER_OFFSET;
            const char* strToDisplay;
            if(nvsManager->screen == NVS_SEND)
            {
                switch(nvsManager->sendMode)
                {
                    case NVS_SEND_MODE_ALL:
                    {
                        strToDisplay = str_send_all;
                        break;
                    }
                    case NVS_SEND_MODE_ONE:
                    {
                        strToDisplay = str_send;
                        break;
                    }
                }
            }
            else
            {
                switch(nvsManager->receiveMode)
                {
                    case NVS_RECV_MODE_ALL:
                    {
                        strToDisplay = str_receive_all;
                        break;
                    }
                    case NVS_RECV_MODE_MULTI:
                    {
                        strToDisplay = str_receive_multi;
                        break;
                    }
                    case NVS_RECV_MODE_ONE:
                    {
                        strToDisplay = str_receive_one;
                        break;
                    }
                }
            }
            drawText(nvsManager->disp, &nvsManager->mm, color_summary_text, strToDisplay, (nvsManager->disp->w - textWidth(&nvsManager->mm, strToDisplay)) / 2, yOff);

            yOff += nvsManager->mm.h + SUMMARY_LINE_BREAK_Y + 1;
            plotLine(nvsManager->disp, CORNER_OFFSET, yOff, nvsManager->disp->w - CORNER_OFFSET, yOff, color_summary_h_rule, 0);

            // Actually apply received data
            if(nvsManager->commState == NVS_STATE_DONE && nvsManager->screen == NVS_RECEIVE && nvsManager->receiveMode != NVS_RECV_MODE_ALL)
            {
                if(!nvsManager->appliedLatestData)
                {
                    if(!applyIncomingNvsImage())
                    {
                        nvsManager->commState = NVS_STATE_FAILED;
                    }
                    nvsManager->appliedAnyData = true;
                    nvsManager->appliedLatestData = true;
                }
            }

            // TODO: give user option to apply partially received data from NVS_RECV_MODE_ALL

            // Restart connection if this is NVS_RECV_MODE_MULTI
            if(nvsManager->appliedLatestData && nvsManager->commState == NVS_STATE_DONE && nvsManager->screen == NVS_RECEIVE && nvsManager->receiveMode == NVS_RECV_MODE_MULTI)
            {
                nvsManagerP2pConnect();
            }

            // Comm state
            yOff += SUMMARY_LINE_BREAK_Y + 1;
            bool showProgressBar = true;
            bool showText = true;
            if(nvsManager->screen == NVS_SEND)
            {
                switch(nvsManager->commState)
                {
                    case NVS_STATE_NOT_CONNECTED:
                    {
                        strToDisplay = str_not_connected;
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_CONNECTION:
                    {
                        strToDisplay = str_connecting;
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_VERSION:
                    {
                        strToDisplay = str_exchanging_version_info;
                        break;
                    }
                    case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
                    {
                        strToDisplay = "";
                        break;
                    }
                    case NVS_STATE_SENT_PAIR_HEADER:
                    case NVS_STATE_SENT_VALUE_DATA:
                    {
                        strToDisplay = NULL;
                        break;
                    }
                    case NVS_STATE_DONE:
                    {
                        strToDisplay = str_done;
                        showProgressBar = false;
                        break;
                    }
                    case NVS_STATE_FAILED:
                    {
                        strToDisplay = str_failed;
                        showProgressBar = false;
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                    case NVS_STATE_WAITING_FOR_PAIR_HEADER:
                    case NVS_STATE_WAITING_FOR_VALUE_DATA:
                    default:
                    {
                        showText = false;
                        showProgressBar = false;
                        break;
                    }
                } // switch(nvsManager->commState)
            } // if(nvsManager->screen == NVS_SEND)
            else
            {
                switch(nvsManager->commState)
                {
                    case NVS_STATE_NOT_CONNECTED:
                    {
                        strToDisplay = str_not_connected;
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_CONNECTION:
                    {
                        strToDisplay = str_connecting;
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_VERSION:
                    {
                        strToDisplay = str_exchanging_version_info;
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                    {
                        strToDisplay = "";
                        break;
                    }
                    case NVS_STATE_WAITING_FOR_PAIR_HEADER:
                    case NVS_STATE_WAITING_FOR_VALUE_DATA:
                    {
                        strToDisplay = NULL;
                        break;
                    }
                    case NVS_STATE_DONE:
                    {
                        strToDisplay = str_done;
                        showProgressBar = false;
                        break;
                    }
                    case NVS_STATE_FAILED:
                    {
                        strToDisplay = str_failed;
                        showProgressBar = false;
                        break;
                    }
                    case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
                    case NVS_STATE_SENT_PAIR_HEADER:
                    case NVS_STATE_SENT_VALUE_DATA:
                    default:
                    {
                        showText = false;
                        showProgressBar = false;
                        break;
                    }
                } // switch(nvsManager->commState)
            } // if(nvsManager->screen != NVS_SEND)
            if(showText)
            {
#define keyStrLen lengthof(str_receiving_format) + NVS_KEY_NAME_MAX_SIZE - 3 /*"%s" and format string's null terminator*/
                char keyStr[keyStrLen];
                if(strToDisplay == NULL)
                {
                    snprintf(keyStr, keyStrLen, nvsManager->screen == NVS_SEND ? str_sending_format : str_receiving_format,
                             nvsManager->nvsEntryInfos[nvsManager->totalNumPairs - nvsManager->numPairsRemaining].key);
                }
                int16_t afterText = drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, strToDisplay != NULL ? strToDisplay : keyStr, CORNER_OFFSET, yOff);
                if(nvsManager->commState == NVS_STATE_WAITING_FOR_CONNECTION)
                {
                    drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, nvsManager->wired ? str_wired : str_wireless, afterText + textWidth(&nvsManager->ibm_vga8, " "), yOff);
                }

                yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y;

                // TODO: draw a progress bar - steal from paint?
            }

            // Error, if failed
            if(nvsManager->commState == NVS_STATE_FAILED)
            {
                if(!nvsManager->commErrorIsFromHere)
                {
                    drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_other_swadge_sent_error, CORNER_OFFSET, yOff);

                    yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y;
                }

                int16_t xOff = CORNER_OFFSET;
                drawTextWordWrap(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, nvsManager->commError, &xOff, &yOff, nvsManager->disp->w - CORNER_OFFSET, nvsManager->disp->h - CORNER_OFFSET);
                if(xOff == CORNER_OFFSET)
                {
                    yOff += LINE_BREAK_Y - 1;
                }
                else
                {
                    yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y;
                }
            }

            // Success message
            if(nvsManager->commState == NVS_STATE_DONE && nvsManager->screen == NVS_RECEIVE && nvsManager->appliedLatestData)
            {
                yOff += nvsManager->ibm_vga8.h + LINE_BREAK_Y;
                drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_received_data_applied_successfully, CORNER_OFFSET, yOff);
            }

            // TODO: show how many pairs NVS_RECV_MODE_MULTI applied

            // Bottom text for control(s)
            yOff = nvsManager->disp->h - CORNER_OFFSET - nvsManager->ibm_vga8.h;
            drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_b_to_cancel, CORNER_OFFSET, yOff);

            if(nvsManager->commState == NVS_STATE_DONE)
            {
                yOff -= LINE_BREAK_Y - nvsManager->ibm_vga8.h;
                if(nvsManager->screen == NVS_SEND || nvsManager->receiveMode != NVS_RECV_MODE_ONE)
                {
                    drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_any_button_to_return, CORNER_OFFSET, yOff);
                }
                else
                {
                    drawText(nvsManager->disp, &nvsManager->ibm_vga8, color_summary_text, str_any_button_to_continue, CORNER_OFFSET, yOff);
                }

                // TODO: one-time verification of incoming image
            }
            break;
        }
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

void nvsManagerP2pConnect(void)
{
    // Reset state variables
    // Don't change `nvsManager->sendMode`, `nvsManager->receiveMode`, or `nvsManager->wired`, they should already be set properly
    nvsManager->commErrorIsFromHere = false;
    nvsManager->timeSincePacket = 0;
    nvsManager->commState = NVS_STATE_WAITING_FOR_CONNECTION;
    nvsManager->packetState = NVS_PSTATE_NOTHING_SENT_YET;
    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
        nvsManager->lastPacketSent = NULL;
    }
    nvsManager->lastPacketLen = 0;
    if((nvsManager->screen == NVS_SEND && nvsManager->sendMode == NVS_SEND_MODE_ONE) ||
       (nvsManager->screen == NVS_RECEIVE && (nvsManager->receiveMode == NVS_RECV_MODE_ONE || nvsManager->receiveMode == NVS_RECV_MODE_MULTI)))
    {
        nvsManager->totalNumPairs = 1;

        // We already know everything about the key/value pair we want to send, so load it now
        nvsManagerReadNvsKeyValuePair(&(nvsManager->nvsEntryInfos[nvsManager->loadedRow]), &nvsManager->num, &nvsManager->blob, &nvsManager->keyUsedEntries, &nvsManager->totalNumBytesInPair);

        nvsManager->totalNumPackets = NVS_TOTAL_PACKETS_UP_TO_INCL_NUM_PAIRS_ENTRIES + 1 /*ACK for NVS_PACKET_NUM_PAIRS_AND_ENTRIES*/ + getNumPacketsNeededToSendPair(nvsManager->totalNumBytesInPair);
    }
    else
    {
        // Zero all this and load it later
        nvsManager->totalNumPairs = 0;
        nvsManager->totalNumBytesInPair = 0;
        nvsManager->totalNumPackets = 0;
    }
    nvsManager->numPairsRemaining = nvsManager->totalNumPairs;
    nvsManager->numBytesRemainingInPair = nvsManager->totalNumBytesInPair;
    nvsManager->numPacketsRemaining = nvsManager->totalNumPackets;
    if(nvsManager->sendableEntryInfoIndices != NULL)
    {
        free(nvsManager->sendableEntryInfoIndices);
        nvsManager->sendableEntryInfoIndices = NULL;
    }
    if(nvsManager->sendableEntryInfoIndices == NULL)
    {
        nvsManager->sendableEntryInfoIndices = heap_caps_calloc(nvsManager->nvsNumEntryInfos, sizeof(nvsManager->curSendableEntryInfoIndicesIdx), MALLOC_CAP_SPIRAM);
    }
    nvsManager->curSendableEntryInfoIndicesIdx = 0;

    nvsManagerDeinitIncomingNvsImage();
    nvsManager->appliedLatestData = false;

    // Initialize p2p
    p2pDeinit(&(nvsManager->p2p));
    p2pInitialize(&(nvsManager->p2p), nvsManager->screen == NVS_SEND ? 'N' : 'O', nvsManagerP2pConCbFn, nvsManagerP2pMsgRxCbFn, -35);
    p2pSetAsymmetric(&(nvsManager->p2p), nvsManager->screen == NVS_SEND ? 'O' : 'N');
    // Start the connection
    p2pStartConnection(&nvsManager->p2p);

    // Turn off LEDs
    led_t leds[NUM_LEDS] = {0};
    setLeds(leds, NUM_LEDS);
}

void nvsManagerP2pSendMsg(p2pInfo* p2p, const uint8_t* payload, uint16_t len, p2pMsgTxCbFn msgTxCbFn)
{
    if(nvsManager->commState != NVS_STATE_FAILED && nvsManager->commState != NVS_STATE_DONE)
    {
#ifdef DEBUG
        printSendPacketDebug(payload);
#endif
        p2pSendMsg(p2p, payload, len, msgTxCbFn);
    }
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
            switch(nvsManager->screen)
            {
                case NVS_SEND:
                {
                    nvsManagerSendVersion();
                    break;
                }
                case NVS_RECEIVE:
                {
                    nvsManager->commState = NVS_STATE_WAITING_FOR_VERSION;
                    nvsManager->packetState = NVS_PSTATE_NOTHING_SENT_YET;
                    break;
                }
                case NVS_MENU:
                case NVS_SUMMARY:
                case NVS_WARNING:
                case NVS_MANAGE_KEY:
                default:
                {
                    // How did we get here?
                    nvsManager->commState = NVS_STATE_NOT_CONNECTED;
                    nvsManager->packetState = NVS_PSTATE_NOTHING_SENT_YET;
                    nvsManagerStopP2p();
                    break;
                }
            } // switch(nvsManager->screen)

            break;
        }
        case CON_LOST:
        {
            switch(nvsManager->commState)
            {
                case NVS_STATE_NOT_CONNECTED:
                case NVS_STATE_DONE:
                {
                    // Silently close connection
                    nvsManagerStopP2p();
                    break;
                }
                case NVS_STATE_WAITING_FOR_CONNECTION:
                {
                    // Retry connection
                    nvsManagerP2pConnect();
                    break;
                }
                case NVS_STATE_WAITING_FOR_VERSION:
                case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
                case NVS_STATE_SENT_PAIR_HEADER:
                case NVS_STATE_SENT_VALUE_DATA:
                case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                case NVS_STATE_WAITING_FOR_PAIR_HEADER:
                case NVS_STATE_WAITING_FOR_VALUE_DATA:
                case NVS_STATE_FAILED: // We shouldn't be here, but if we are, set other state appropriately and close connection
                default:
                {
                    // Fail, and let the main loop pick up the pieces according to state and give the user options
                    nvsManagerFailP2p();
                    break;
                }
            } // switch(nvsManager->commState)

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
#ifdef DEBUG
    printRecvPacketDebug(payload);
#endif

    if(payload[0] == NVS_PACKET_ERROR && (nvsManager->screen == NVS_SEND || nvsManager->screen == NVS_RECEIVE))
    {
        nvsPacketError_t packetAsStruct;
        nvsManagerDecodePacketError(payload, len, &packetAsStruct);
        memcpy(nvsManager->commError, packetAsStruct.message, NVS_MAX_ERROR_MESSAGE_LEN);
        
        nvsManagerFailP2p();
        return;
    }

    nvsManager->numPacketsRemaining--;

    switch(nvsManager->screen)
    {
        case NVS_SEND:
        {
            switch(nvsManager->commState)
            {
                case NVS_STATE_WAITING_FOR_VERSION:
                {
                    if(payload[0] != NVS_PACKET_VERSION || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    nvsPacketVersion_t packet;
                    nvsManagerDecodePacketVersion(payload, len, &packet);

                     // Our version is newer, check if we support other Swadge's version
                    if(memcmp(packet.version, NVS_VERSION, NVS_VER_LEN) < 0)
                    {
                        // This is the first protocol version, so how could there be an older one?
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        char otherVer[NVS_VER_LEN + 1] = {0};
                        char myVer[NVS_VER_LEN + 1] = {0};
                        memcpy(otherVer, &packet.version, NVS_VER_LEN);
                        memcpy(myVer, NVS_VERSION, NVS_VER_LEN);
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_first_version_error_format, otherVer, myVer);
                        nvsManagerSendError(NVS_ERROR_NO_COMMON_VERSION, message);
                    }
                    // Either other Swadge's version is newer and other Swadge knows that and will use our version, or versions are identical
                    // If other Swadge was unable to use our version, it would have sent NVS_ERROR_NO_COMMON_VERSION instead of its version
                    else
                    {
                        nvsManagerSendNumPairsEntries();
                    }

                    break;
                } // NVS_STATE_WAITING_FOR_VERSION
                case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
                {
                    if(payload[0] != NVS_PACKET_READY || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    // If NVS_SEND_MODE_ONE, nvsManager->loadedRow is set in nvsManagerConnect
                    // If NVS_SEND_MODE_ALL, nvsManager->loadedRow is set in nvsManagerSendNumPairsEntries()
                    nvsManagerSendPairHeader(&(nvsManager->nvsEntryInfos[nvsManager->loadedRow]));
                    break;
                }
                case NVS_STATE_SENT_PAIR_HEADER:
                {
                    if(payload[0] != NVS_PACKET_READY || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    size_t dataLength = MIN(nvsManager->numBytesRemainingInPair, NVS_MAX_DATA_BYTES_PER_PACKET);
                    nvsManagerSendValueData(nvsManager->blob != NULL ? (uint8_t*) &(nvsManager->blob[nvsManager->totalNumBytesInPair - nvsManager->numBytesRemainingInPair]) : (uint8_t*) &nvsManager->num, dataLength);
                    break;
                }
                case NVS_STATE_SENT_VALUE_DATA:
                {
                    if(payload[0] != NVS_PACKET_READY || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    if(nvsManager->numBytesRemainingInPair == 0)
                    {
                        if(nvsManager->numPairsRemaining == 0)
                        {
                            // Comms should already be done, why are we receiving a packet?
                            nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                            break;
                        }
                        else
                        {
                            // Increment to next readable/sendable key/value pair
                            nvsManager->curSendableEntryInfoIndicesIdx++;
                            nvsManager->loadedRow = nvsManager->sendableEntryInfoIndices[nvsManager->curSendableEntryInfoIndicesIdx];

                            // Send pair header packet
                            nvsManagerSendPairHeader(&(nvsManager->nvsEntryInfos[nvsManager->loadedRow]));
                        }
                    }
                    else
                    {
                        size_t dataLength = MIN(nvsManager->numBytesRemainingInPair, NVS_MAX_DATA_BYTES_PER_PACKET);
                        nvsManagerSendValueData(nvsManager->blob != NULL ? (uint8_t*) &(nvsManager->blob[nvsManager->totalNumBytesInPair - nvsManager->numBytesRemainingInPair]) : (uint8_t*) &nvsManager->num, dataLength);
                    }
                    
                    break;
                }
                case NVS_STATE_DONE:
                {
                    // Bad state, but likely on the other Swadge
                    
                    // Save current state
                    nvsCommState_t cstate = nvsManager->commState;
                    nvsPacketState_t pstate = nvsManager->packetState;

                    nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);

                    // Restore state
                    nvsManager->commState = cstate;
                    nvsManager->packetState = pstate;

                    break;
                }
                case NVS_STATE_NOT_CONNECTED:
                case NVS_STATE_WAITING_FOR_CONNECTION:
                case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                case NVS_STATE_WAITING_FOR_PAIR_HEADER:
                case NVS_STATE_WAITING_FOR_VALUE_DATA:
                case NVS_STATE_FAILED:
                default:
                {
                    // Bad state on this Swadge
                    nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                    break;
                }
            }

            break;
        } // NVS_SEND
        case NVS_RECEIVE:
        {
            switch(nvsManager->commState)
            {
                case NVS_STATE_WAITING_FOR_VERSION:
                {
                    if(payload[0] != NVS_PACKET_VERSION)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    nvsPacketVersion_t packet;
                    nvsManagerDecodePacketVersion(payload, len, &packet);

                    // Our version is newer, check if we support other Swadge's version
                    if(memcmp(packet.version, NVS_VERSION, NVS_VER_LEN) < 0)
                    {
                        // This is the first protocol version, so how could there be an older one?
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        char otherVer[NVS_VER_LEN + 1] = {0};
                        char myVer[NVS_VER_LEN + 1] = {0};
                        memcpy(otherVer, &packet.version, NVS_VER_LEN);
                        memcpy(myVer, NVS_VERSION, NVS_VER_LEN);
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_first_version_error_format, otherVer, myVer);
                        nvsManagerSendError(NVS_ERROR_NO_COMMON_VERSION, message);
                    }
                    // Either other Swadge's version is newer and other Swadge doesn't know yet, or versions are identical
                    else
                    {
                        nvsManagerSendVersion();
                    }

                    break;
                } // NVS_STATE_WAITING_FOR_VERSION
                case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                {
                    if(payload[0] != NVS_PACKET_NUM_PAIRS_AND_ENTRIES || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    nvsPacketNumPairsEntries_t packetAsStruct;
                    nvsManagerDecodePacketNumPairsEntries(payload, len, &packetAsStruct);

                    if(packetAsStruct.numEntries > nvsManager->nvsStats.total_entries)
                    {
                        nvsManagerSendError(NVS_ERROR_INSUFFICIENT_SPACE, str_insufficient_space);
                        break;
                    }

                    if(packetAsStruct.numPairs == 0)
                    {
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, str_zero_pairs);
                        break;
                    }

                    if(packetAsStruct.numEntries == 0)
                    {
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, str_zero_entries);
                        break;
                    }

                    if(packetAsStruct.numPackets < NVS_TOTAL_PACKETS_UP_TO_INCL_NUM_PAIRS_ENTRIES + 1 /*ACK for PACKET_NUM_PAIRS_AND_ENTRIES*/ + NVS_MIN_PACKETS_PER_PAIR * packetAsStruct.numPairs)
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_not_enough_packets_format, packetAsStruct.numPackets, packetAsStruct.numPairs);
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }

                    if(packetAsStruct.numEntries < packetAsStruct.numPairs)
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_less_entries_than_pairs_format, packetAsStruct.numEntries, packetAsStruct.numPairs);
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }

                    if(packetAsStruct.numPairs > 1 && nvsManager->receiveMode != NVS_RECV_MODE_ALL)
                    {
                        nvsManagerSendError(NVS_ERROR_WRONG_RECEIVE_MODE, str_wrong_receive_mode);
                        break;
                    }

                    nvsManager->totalNumPairs = packetAsStruct.numPairs;
                    nvsManager->totalNumPackets = packetAsStruct.numPackets;
                    nvsManager->numPairsRemaining = nvsManager->totalNumPairs;
                    nvsManager->numPacketsRemaining = nvsManager->totalNumPackets - NVS_TOTAL_PACKETS_UP_TO_INCL_NUM_PAIRS_ENTRIES - 1 /*ACK for NVS_PACKET_NUM_PAIRS_AND_ENTRIES*/;

                    nvsManagerInitIncomingNvsImage(nvsManager->totalNumPairs);

                    nvsManager->commState = NVS_STATE_WAITING_FOR_PAIR_HEADER;
                    nvsManagerSendReady();

                    break;
                }
                case NVS_STATE_WAITING_FOR_PAIR_HEADER:
                {
                    if(payload[0] != NVS_PACKET_PAIR_HEADER || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    nvsPacketPairHeader_t packetAsStruct;
                    nvsManagerDecodePacketPairHeader(payload, len, &packetAsStruct);

                    // Check for empty key
                    if(packetAsStruct.key[0] == '\0')
                    {
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, str_empty_key);
                        break;
                    }

                    // Check for inaccessible namespace
                    if(strncmp(packetAsStruct.namespace, NVS_NAMESPACE_NAME, NVS_KEY_NAME_MAX_SIZE))
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_inaccessible_namespace_format, NVS_NAMESPACE_NAME, packetAsStruct.namespace);
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }

                    // Check for unknown NVS type
                    if(getNvsTypeName(packetAsStruct.nvsType)[0] == '\0')
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_unknown_type_format, packetAsStruct.nvsType);
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }

                    // Check for too few bytes
                    if((packetAsStruct.nvsType == NVS_TYPE_I32 && packetAsStruct.numValueBytes < sizeof(int32_t)) ||
                       (packetAsStruct.nvsType == NVS_TYPE_BLOB && packetAsStruct.numValueBytes < 1))
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_too_x_bytes_for_type_format, str_few, packetAsStruct.numValueBytes, getNvsTypeName(packetAsStruct.nvsType));
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }
                    // Check for too many bytes
                    else if((packetAsStruct.nvsType == NVS_TYPE_I32 && packetAsStruct.numValueBytes > sizeof(int32_t)) ||
                            (packetAsStruct.nvsType == NVS_TYPE_BLOB && packetAsStruct.numValueBytes > NVS_MAX_BLOB_BYTES))
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_too_x_bytes_for_type_format, str_many, packetAsStruct.numValueBytes, getNvsTypeName(packetAsStruct.nvsType));
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }

                    // Copy key/value pair info into incoming NVS image
                    nvs_entry_info_t* entryInfo = &(nvsManager->incomingNvsEntryInfos[nvsManager->totalNumPairs - nvsManager->numPairsRemaining]);
                    memcpy(&entryInfo->namespace_name, &packetAsStruct.namespace, NVS_KEY_NAME_MAX_SIZE);
                    entryInfo->type = packetAsStruct.nvsType;
                    nvsManager->totalNumBytesInPair = packetAsStruct.numValueBytes;
                    nvsManager->numBytesRemainingInPair = nvsManager->totalNumBytesInPair;
                    memcpy(&entryInfo->key, &packetAsStruct.key, NVS_KEY_NAME_MAX_SIZE);

                    // Allocate memory for the incoming value next packet
                    nvsValue_t* value = &(nvsManager->incomingNvsValues[nvsManager->totalNumPairs - nvsManager->numPairsRemaining]);
                    value->data = heap_caps_calloc(sizeof(char), nvsManager->totalNumBytesInPair, MALLOC_CAP_SPIRAM);
                    value->numBytes = nvsManager->totalNumBytesInPair;

                    nvsManager->commState = NVS_STATE_WAITING_FOR_VALUE_DATA;
                    nvsManagerSendReady();

                    break;
                }
                case NVS_STATE_WAITING_FOR_VALUE_DATA:
                {
                    if(payload[0] != NVS_PACKET_VALUE_DATA || nvsManager->packetState != NVS_PSTATE_SUCCESS)
                    {
                        nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                        break;
                    }

                    // Get data length for integrity checks
                    size_t dataLength;
                    nvsManagerDecodePacketValueData(payload, len, NULL, &dataLength);

                    size_t expectedDataLength = MIN(nvsManager->numBytesRemainingInPair, NVS_MAX_DATA_BYTES_PER_PACKET);

                    // Check for too few bytes
                    if(dataLength < expectedDataLength)
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_value_data_too_x_bytes, str_few, expectedDataLength, dataLength);
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }
                    // Check for too many bytes
                    else if(dataLength > expectedDataLength)
                    {
                        char message[NVS_MAX_ERROR_MESSAGE_LEN];
                        snprintf(message, NVS_MAX_ERROR_MESSAGE_LEN, str_value_data_too_x_bytes, str_many, expectedDataLength, dataLength);
                        nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
                        break;
                    }

                    // Copy data to incoming NVS image
                    nvsManagerDecodePacketValueData(payload, len, (uint8_t*) &(nvsManager->incomingNvsValues[nvsManager->totalNumPairs - nvsManager->numPairsRemaining].data[nvsManager->totalNumBytesInPair - nvsManager->numBytesRemainingInPair]), &dataLength);

                    nvsManager->numBytesRemainingInPair -= dataLength;

                    if(nvsManager->numBytesRemainingInPair == 0)
                    {
                        nvsManager->numPairsRemaining--;

                        if(nvsManager->numPairsRemaining == 0)
                        {
                            // NVS_RECV_MODE_MULTI will be handled in the main loop

                            nvsManager->commState = NVS_STATE_DONE;
                            nvsManagerStopP2p();

                            break;
                        }

                        nvsManager->commState = NVS_STATE_WAITING_FOR_PAIR_HEADER;
                    }

                    // If we reach this, we should be ready for either the next `NVS_PACKET_VALUE_DATA` or the next `NVS_PACKET_PAIR_HEADER`
                    nvsManagerSendReady();

                    break;
                }
                case NVS_STATE_NOT_CONNECTED:
                case NVS_STATE_WAITING_FOR_CONNECTION:
                case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
                case NVS_STATE_SENT_PAIR_HEADER:
                case NVS_STATE_SENT_VALUE_DATA:
                case NVS_STATE_DONE:
                case NVS_STATE_FAILED:
                {
                    // Bad state on this Swadge
                    nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
                    break;
                }
            }

            break;
        } // NVS_RECEIVE
        case NVS_MENU:
        case NVS_WARNING:
        case NVS_SUMMARY:
        case NVS_MANAGE_KEY:
        default:
        {
            // These screens don't receive packets
            nvsManagerComposeAndSendUnexpectedPacketErrorMessage(payload[0], false);
            break;
        }
    } // switch(nvsManager->screen)
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
    if(nvsManager->commState == NVS_STATE_FAILED)
    {
        return;
    }

    // Check what was ACKed or failed
    switch(status)
    {
        case MSG_ACKED:
        {
#ifdef DEBUG
            ESP_LOGD("NM", "Receiving ACK");
#endif
            // ACKs are too inconsistent to use this
            // if(nvsManager->packetState != NVS_PSTATE_WAIT_FOR_ACK)
            // {
            //     nvsManagerComposeAndSendUnexpectedPacketErrorMessage(0, true);
            //     break;
            // }

            // ACK was expected

            nvsManager->packetState = NVS_PSTATE_SUCCESS;
            
            switch(nvsManager->screen)
            {
                case NVS_SEND:
                {
                    nvsManager->numPacketsRemaining--;

                    switch(nvsManager->commState)
                    {
                        case NVS_STATE_WAITING_FOR_VERSION: // Next packet we receive should be `NVS_PACKET_VERSION`
                        case NVS_STATE_SENT_NUM_PAIRS_ENTRIES: // Next packet we receive should be `NVS_PACKET_READY`
                        case NVS_STATE_SENT_PAIR_HEADER: // Next packet we receive should be `NVS_PACKET_READY`
                        {
                            // Packet state change already done above, so nothing to do for now
                            break;
                        }
                        case NVS_STATE_SENT_VALUE_DATA:
                        {
                            if(nvsManager->numBytesRemainingInPair == 0 && nvsManager->numPairsRemaining == 0)
                            {
                                // Woohoo! Communication with the other Swadge was successful, time to clean up
                                nvsManager->commState = NVS_STATE_DONE;
                                nvsManagerStopP2p();
                            }
                            
                            break;
                        }
                        case NVS_STATE_FAILED:
                        {
                            break;
                        }
                        case NVS_STATE_NOT_CONNECTED:
                        case NVS_STATE_WAITING_FOR_CONNECTION:
                        case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                        case NVS_STATE_WAITING_FOR_PAIR_HEADER:
                        case NVS_STATE_WAITING_FOR_VALUE_DATA:
                        case NVS_STATE_DONE:
                        default:
                        {
                            // Bad state on this Swadge
                            nvsManagerComposeAndSendUnexpectedPacketErrorMessage(0, true);
                            break;
                        }
                    } // switch(nvsManager->commState)

                    break;
                } // NVS_SEND
                case NVS_RECEIVE:
                {
                    switch(nvsManager->commState)
                    {
                        case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
                        {
                            // We just sent our `NVS_PACKET_VERSION` and were waiting for the ACK
                            // Next packet we receive should be the one our state says we're waiting for
                            // Packet state change already done above, and still unknown totalNumPackets, so nothing to do for now
                            break;
                        }
                        case NVS_STATE_WAITING_FOR_PAIR_HEADER: // We just sent our `NVS_PACKET_READY` and were waiting for the ACK
                        case NVS_STATE_WAITING_FOR_VALUE_DATA: // We just sent our `NVS_PACKET_READY` and were waiting for the ACK
                        {
                            // Next packet we receive should be the one our state says we're waiting for
                            // Packet state change already done above

                            nvsManager->numPacketsRemaining--;
                            break;
                        }
                        case NVS_STATE_FAILED:
                        {
                            break;
                        }
                        case NVS_STATE_NOT_CONNECTED:
                        case NVS_STATE_WAITING_FOR_CONNECTION:
                        case NVS_STATE_WAITING_FOR_VERSION:
                        case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
                        case NVS_STATE_SENT_PAIR_HEADER:
                        case NVS_STATE_SENT_VALUE_DATA:
                        case NVS_STATE_DONE:
                        default:
                        {
                            // Bad state on this Swadge
                            nvsManagerComposeAndSendUnexpectedPacketErrorMessage(0, true);
                            break;
                        }
                    } // switch(nvsManager->commState)

                    break;
                } // NVS_RECEIVE
                case NVS_MENU:
                case NVS_SUMMARY:
                case NVS_WARNING:
                case NVS_MANAGE_KEY:
                default:
                {
                    // These screens don't send packets
                    nvsManagerComposeAndSendUnexpectedPacketErrorMessage(0, true);
                    break;
                }
            } // switch(nvsManager->screen)

            break;
        } // MSG_ACKED
        case MSG_FAILED:
        {
            // P2P has already retried sending this message, and it has not been ACK'd for 3 seconds, so let's not retry this again
            //nvsManagerP2pSendMsg(&nvsManager->p2p, nvsManager->lastPacketSent, nvsManager->lastPacketLen, nvsManagerP2pMsgTxCbFn);

            // Fail, and let the main loop pick up the pieces according to state and give the user options
            nvsManagerFailP2p();
            break;
        }
    } // switch(status)
}

void nvsManagerStopP2p(void)
{
    nvsManager->p2p.cnc.isActive = false;
    nvsManager->p2p.cnc.isConnected = false;
    p2pDeinit(&nvsManager->p2p);
}

void nvsManagerFailP2p(void)
{
    nvsManager->commState = NVS_STATE_FAILED;
    nvsManager->packetState = NVS_PSTATE_NOTHING_SENT_YET;
    nvsManagerStopP2p();
}

void nvsManagerInitIncomingNvsImage(uint32_t numPairs)
{
    nvsManagerDeinitIncomingNvsImage();

    nvsManager->incomingNvsEntryInfos = heap_caps_calloc(numPairs, sizeof(nvs_entry_info_t), MALLOC_CAP_SPIRAM);
    nvsManager->numIncomingEntryInfos = numPairs;
    nvsManager->incomingNvsValues = heap_caps_calloc(numPairs, sizeof(nvsValue_t), MALLOC_CAP_SPIRAM);
}

void nvsManagerDeinitIncomingNvsImage(void)
{
    if(nvsManager->incomingNvsEntryInfos != NULL)
    {
        free(nvsManager->incomingNvsEntryInfos);
        nvsManager->incomingNvsEntryInfos = NULL;
    }

    for(uint32_t i = 0; i < nvsManager->numIncomingEntryInfos; i++)
    {
        if(nvsManager->incomingNvsValues[i].data != NULL)
        {
            free(nvsManager->incomingNvsValues[i].data);
            nvsManager->incomingNvsValues[i].data = NULL;
        }
    }

    if(nvsManager->incomingNvsValues != NULL)
    {
        free(nvsManager->incomingNvsValues);
        nvsManager->incomingNvsValues = NULL;
    }

    nvsManager->numIncomingEntryInfos = 0;
}

bool applyIncomingNvsImage(void)
{
    bool foundProblems = false;

    for(uint32_t i = 0; i < nvsManager->totalNumPairs - nvsManager->numPairsRemaining; i++)
    {
        nvs_entry_info_t* entryInfo = &nvsManager->incomingNvsEntryInfos[i];
        if(entryInfo->type == NVS_TYPE_I32)
        {
            if(!writeNvs32(entryInfo->key, *((int32_t*) nvsManager->incomingNvsValues[i].data)))
            {
                foundProblems = true;
            }
            // TODO: handle errors
        }
        else
        {
            if(!writeNvsBlob(entryInfo->key, nvsManager->incomingNvsValues[i].data, nvsManager->incomingNvsValues[i].numBytes))
            {
                foundProblems = true;
            }
            // TODO: handle errors
        }
    }

    // TODO: handle errors
    nvsManagerReadAllNvsEntryInfos();
    return foundProblems;
}

// uint32_t nvsCalcCrc32(const uint8_t* buffer, size_t length)
// {
//     return (~crc32_le((uint32_t)~(0xffffffff), buffer, length))^0xffffffff;
// }

/**
 * @brief Send a packet to the other swadge with this NVS manager's version
 */
void nvsManagerSendVersion(void)
{
    // Populate struct with data
    nvsPacketVersion_t packetAsStruct;
    memcpy(&(packetAsStruct.version), &NVS_VERSION, NVS_VER_LEN);

    size_t length;
    uint8_t* payload = nvsManagerEncodePacketVersion(packetAsStruct, &length);

    // Send packet to the other Swadge
    nvsManagerP2pSendMsg(&nvsManager->p2p, payload, length, nvsManagerP2pMsgTxCbFn);

    // Save sent packet
    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
    }
    nvsManager->lastPacketSent = payload;
    nvsManager->lastPacketLen = length;

    if(nvsManager->screen == NVS_SEND)
    {
        nvsManager->commState = NVS_STATE_WAITING_FOR_VERSION;
    }
    else
    {
        nvsManager->commState = NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES;
    }
    nvsManager->packetState = NVS_PSTATE_WAIT_FOR_ACK;
}

uint8_t* nvsManagerEncodePacketVersion(nvsPacketVersion_t packetAsStruct, size_t* outLength)
{
    // Allocate space
    *outLength = sizeof(nvsPacket_t) + sizeof(nvsPacketVersion_t);
    uint8_t* payload = calloc(1, *outLength);

    // Copy data into packet
    payload[0] = NVS_PACKET_VERSION;
    memcpy(&payload[sizeof(nvsPacket_t)], &packetAsStruct, sizeof(nvsPacketVersion_t));

    return payload;
}

void nvsManagerDecodePacketVersion(const uint8_t* packetAsBytes, size_t length, nvsPacketVersion_t* outPacketAsStruct)
{

    // Copy data out of packet
    memcpy(outPacketAsStruct, &packetAsBytes[sizeof(nvsPacket_t)], sizeof(nvsPacketVersion_t));
}

/**
 * @brief If `nvsManager->sendMode` is `NVS_SEND_MODE_ONE`, send `NVS_PACKET_NUM_PAIRS_AND_ENTRIES` for loaded key/value pair.
 * If `nvsManager->sendMode` is `NVS_SEND_MODE_ALL`, check to see whether each pair is readable, and if so,
 * add its number of pairs and entries to running totals, then send those numbers in `NVS_PACKET_NUM_PAIRS_AND_ENTRIES`.
 * If at least one readable pair was found, save the index of the first one to `nvsManager->loadedRow`, but do not load its other data yet.
 * If no readable pairs were found, send `NVS_ERROR_NO_SENDABLE_PAIRS`
 */
void nvsManagerSendNumPairsEntries(void)
{
    // Populate struct with data
    nvsPacketNumPairsEntries_t packetAsStruct;
    bool foundFirstRow = false;
    switch(nvsManager->sendMode)
    {
        case NVS_SEND_MODE_ALL:
        {
            packetAsStruct.numPairs = 0;
            packetAsStruct.numEntries = 0;
            nvsManager->numPacketsRemaining = 0;
            nvsManager->loadedRow = UINT16_MAX;

            for(uint32_t i = 0; i < nvsManager->nvsNumEntryInfos; i++)
            {
                nvs_entry_info_t entryInfo = nvsManager->nvsEntryInfos[i];
                uint32_t sendableEntryInfoIndicesIdx = 0;

                // Only prepare to send real key/value pairs in the namespace we have access to
                if(entryInfo.key[0] != '\0' && !strncmp(entryInfo.namespace_name, NVS_NAMESPACE_NAME, NVS_KEY_NAME_MAX_SIZE))
                {
                    // Only prepare to send key/value pairs that can be read successfully
                    uint16_t entries;
                    size_t bytes;
                    if(nvsManagerReadNvsKeyValuePair(&entryInfo, NULL, NULL, &entries, &bytes))
                    {
                        if(!foundFirstRow)
                        {
                            nvsManager->loadedRow = i;
                            foundFirstRow = true;
                        }

                        packetAsStruct.numPairs++;
                        packetAsStruct.numEntries += entries;
                        nvsManager->numPacketsRemaining += getNumPacketsNeededToSendPair(bytes);
                        nvsManager->sendableEntryInfoIndices[sendableEntryInfoIndicesIdx] = i;
                        sendableEntryInfoIndicesIdx++;
                    }
                }
            }

            nvsManager->totalNumPairs = packetAsStruct.numPairs;
            nvsManager->numPairsRemaining = packetAsStruct.numPairs;
            nvsManager->totalNumPackets = NVS_TOTAL_PACKETS_UP_TO_INCL_NUM_PAIRS_ENTRIES + 1 /*ACK for NVS_PACKET_NUM_PAIRS_ENTRIES*/ + nvsManager->numPacketsRemaining;

            break;
        }
        case NVS_SEND_MODE_ONE:
        {
            packetAsStruct.numPairs = 1;
            packetAsStruct.numEntries = nvsManager->keyUsedEntries;
            nvsManager->numPacketsRemaining--;
            break;
        }
    }

    if(nvsManager->sendMode == NVS_SEND_MODE_ONE || foundFirstRow)
    {
        packetAsStruct.numPackets = nvsManager->totalNumPackets;

        size_t length;
        uint8_t* payload = nvsManagerEncodePacketNumPairsEntries(packetAsStruct, &length);

        // Send packet to the other Swadge
        nvsManagerP2pSendMsg(&nvsManager->p2p, payload, length, nvsManagerP2pMsgTxCbFn);

        // Save sent packet
        if(nvsManager->lastPacketSent != NULL)
        {
            free(nvsManager->lastPacketSent);
        }
        nvsManager->lastPacketSent = payload;
        nvsManager->lastPacketLen = length;

        nvsManager->commState = NVS_STATE_SENT_NUM_PAIRS_ENTRIES;
        nvsManager->packetState = NVS_PSTATE_WAIT_FOR_ACK;
    }
    else
    {
        nvsManagerSendError(NVS_ERROR_NO_SENDABLE_PAIRS, str_no_sendable_pairs);
    }
}

uint8_t* nvsManagerEncodePacketNumPairsEntries(nvsPacketNumPairsEntries_t packetAsStruct, size_t* outLength)
{
    // Allocate space
    *outLength = sizeof(nvsPacket_t) + sizeof(nvsPacketNumPairsEntries_t);
    uint8_t* payload = calloc(1, *outLength);

    // Copy data into packet
    payload[0] = NVS_PACKET_NUM_PAIRS_AND_ENTRIES;
    memcpy(&payload[sizeof(nvsPacket_t)], &packetAsStruct, sizeof(nvsPacketNumPairsEntries_t));

    return payload;
}

void nvsManagerDecodePacketNumPairsEntries(const uint8_t* packetAsBytes, size_t length, nvsPacketNumPairsEntries_t* outPacketAsStruct)
{
    // Copy data out of packet
    memcpy(outPacketAsStruct, &packetAsBytes[sizeof(nvsPacket_t)], sizeof(nvsPacketNumPairsEntries_t));
}

/**
 * @brief If `nvsManager->sendMode` is `NVS_SEND_MODE_ONE`, sends `NVS_PACKET_PAIR_HEADER` for the loaded key/value pair.
 * If `nvsManager->sendMode` is `NVS_SEND_MODE_ALL`, loads the pair based on given `entryInfo` and sends `NVS_PACKET_PAIR_HEADER`
 * 
 * @param entryInfo key/value pair to load and send header for
 */
void nvsManagerSendPairHeader(nvs_entry_info_t* entryInfo)
{
    if(nvsManager->sendMode == NVS_SEND_MODE_ALL)
    {
        nvsManagerReadNvsKeyValuePair(entryInfo, &nvsManager->num, &nvsManager->blob, &nvsManager->keyUsedEntries, &nvsManager->totalNumBytesInPair);
    }

    // Populate struct with data
    nvsPacketPairHeader_t packetAsStruct;
    memcpy(&packetAsStruct.namespace, &(entryInfo->namespace_name), NVS_KEY_NAME_MAX_SIZE);
    packetAsStruct.nvsType = entryInfo->type;
    packetAsStruct.numValueBytes = nvsManager->totalNumBytesInPair;
    memcpy(&packetAsStruct.key, &(entryInfo->key), NVS_KEY_NAME_MAX_SIZE);

    size_t length;
    uint8_t* payload = nvsManagerEncodePacketPairHeader(packetAsStruct, &length);

    // Send packet to the other Swadge
    nvsManagerP2pSendMsg(&nvsManager->p2p, payload, length, nvsManagerP2pMsgTxCbFn);

    // Save sent packet
    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
    }
    nvsManager->lastPacketSent = payload;
    nvsManager->lastPacketLen = length;

    nvsManager->numBytesRemainingInPair = nvsManager->totalNumBytesInPair;
    nvsManager->numPacketsRemaining--;

    nvsManager->commState = NVS_STATE_SENT_PAIR_HEADER;
    nvsManager->packetState = NVS_PSTATE_WAIT_FOR_ACK;
}

uint8_t* nvsManagerEncodePacketPairHeader(nvsPacketPairHeader_t packetAsStruct, size_t* outLength)
{
    // Allocate space
    *outLength = sizeof(nvsPacket_t) + sizeof(nvsPacketPairHeader_t);
    uint8_t* payload = calloc(1, *outLength);

    // Copy data into packet
    payload[0] = NVS_PACKET_PAIR_HEADER;
    memcpy(&payload[sizeof(nvsPacket_t)], &packetAsStruct, sizeof(nvsPacketPairHeader_t));

    return payload;
}

void nvsManagerDecodePacketPairHeader(const uint8_t* packetAsBytes, size_t length, nvsPacketPairHeader_t* outPacketAsStruct)
{
    // Copy data out of packet
    memcpy(outPacketAsStruct, &packetAsBytes[sizeof(nvsPacket_t)], sizeof(nvsPacketPairHeader_t));
}

/**
 * @brief sends `NVS_PACKET_VALUE_DATA` and subtracts `dataLength` from `nvsManager->numBytesRemainingInPair`
 * 
 * @param data data to send
 * @param dataLength number of bytes of data to send, up to `NVS_MAX_DATA_BYTES_PER_PACKET`
 */
void nvsManagerSendValueData(uint8_t* data, size_t dataLength)
{
    size_t length;
    uint8_t* payload = nvsManagerEncodePacketValueData(data, dataLength, &length);

    // Send packet to the other Swadge
    nvsManagerP2pSendMsg(&nvsManager->p2p, payload, length, nvsManagerP2pMsgTxCbFn);

    // Save sent packet
    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
    }
    nvsManager->lastPacketSent = payload;
    nvsManager->lastPacketLen = length;

    nvsManager->numBytesRemainingInPair -= dataLength;
    if(nvsManager->numBytesRemainingInPair == 0)
    {
        nvsManager->numPairsRemaining--;
    }
    nvsManager->numPacketsRemaining--;

    nvsManager->commState = NVS_STATE_SENT_VALUE_DATA;
    nvsManager->packetState = NVS_PSTATE_WAIT_FOR_ACK;
}

uint8_t* nvsManagerEncodePacketValueData(uint8_t* data, size_t dataLength, size_t* outLength)
{
    // Allocate space
    *outLength = sizeof(nvsPacket_t) + dataLength;
    uint8_t* payload = calloc(1, *outLength);

    // Copy data into packet
    payload[0] = NVS_PACKET_VALUE_DATA;
    memcpy(&payload[sizeof(nvsPacket_t)], data, dataLength);

    return payload;
}

/**
 * @brief Decode `NVS_PACKET_VALUE_DATA`. Can be called with `outData` set to `NULL` to get only `outDataLength`,
 * for allocating buffers or checking against expected length, prior to calling again with a pointer passed to `outData`
 * 
 * @param packetAsBytes payload as received from P2P RX callback function
 * @param length length as received from P2P RX callback function
 * @param outData location to write value data to, or `NULL` to skip writing data
 * @param outDataLength return number of data bytes contained in packet
 */
void nvsManagerDecodePacketValueData(const uint8_t* packetAsBytes, size_t length, uint8_t* outData, size_t* outDataLength)
{
    *outDataLength = length - sizeof(nvsPacket_t);

    // Allow the function to be called to get `outDataLength` only, without copying data
    if(outData != NULL)
    {
        // Copy data out of packet
        memcpy(outData, &packetAsBytes[sizeof(nvsPacket_t)], *outDataLength);
    }
}

// Sends a packet signaling readiness for the next packet in the sequence. Sets `nvsManager->packetState`, but does not set `nvsManager->commState`
void nvsManagerSendReady(void)
{
    size_t length = sizeof(nvsPacket_t);
    uint8_t* payload = calloc(1, length);
    payload[0] = NVS_PACKET_READY;

    // Send packet to the other Swadge
    nvsManagerP2pSendMsg(&nvsManager->p2p, payload, length, nvsManagerP2pMsgTxCbFn);

    // Save sent packet
    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
    }
    nvsManager->lastPacketSent = payload;
    nvsManager->lastPacketLen = length;

    nvsManager->packetState = NVS_PSTATE_WAIT_FOR_ACK;
}

void nvsManagerSendError(nvsCommError_t error, const char* message)
{
    // Populate struct with data
    nvsPacketError_t packetAsStruct;
    packetAsStruct.error = error;
    memcpy(&packetAsStruct.message, message, NVS_MAX_ERROR_MESSAGE_LEN);

    size_t length;
    uint8_t* payload = nvsManagerEncodePacketError(packetAsStruct, &length);

    // Send packet to the other Swadge
    nvsManagerP2pSendMsg(&nvsManager->p2p, payload, length, nvsManagerP2pMsgTxCbFn);

    // Save sent packet
    if(nvsManager->lastPacketSent != NULL)
    {
        free(nvsManager->lastPacketSent);
    }
    nvsManager->lastPacketSent = payload;
    nvsManager->lastPacketLen = length;

    // Save comm error
    memcpy(nvsManager->commError, message, NVS_MAX_ERROR_MESSAGE_LEN);
    nvsManager->commErrorIsFromHere = true;

    // End communication
    nvsManagerFailP2p();
}

uint8_t* nvsManagerEncodePacketError(nvsPacketError_t packetAsStruct, size_t* outLength)
{
    // Allocate space
    *outLength = sizeof(nvsPacket_t) + sizeof(nvsPacketError_t);
    uint8_t* payload = calloc(1, *outLength);

    // Copy data into packet
    payload[0] = NVS_PACKET_ERROR;
    memcpy(&payload[sizeof(nvsPacket_t)], &packetAsStruct, sizeof(nvsPacketError_t));

    return payload;
}

void nvsManagerDecodePacketError(const uint8_t* packetAsBytes, size_t length, nvsPacketError_t* outPacketAsStruct)
{
    // Copy data out of packet
    memcpy(outPacketAsStruct, &packetAsBytes[sizeof(nvsPacket_t)], sizeof(nvsPacketError_t));
}

void nvsManagerComposeUnexpectedPacketErrorMessage(nvsPacket_t receivedPacketType, bool receivedAck, char** outMessage)
{
    if(*outMessage != NULL)
    {
        free(*outMessage);
        *outMessage = NULL;
    }

    if(*outMessage == NULL)
    {
        *outMessage = calloc(sizeof(char), NVS_MAX_ERROR_MESSAGE_LEN);
    }

    const char* expectedPacket;
    const char* receivedPacket;
    bool expectedAck = nvsManager->packetState == NVS_PSTATE_WAIT_FOR_ACK;

    switch(nvsManager->commState)
    {
        case NVS_STATE_NOT_CONNECTED:
        case NVS_STATE_WAITING_FOR_CONNECTION:
        case NVS_STATE_DONE:
        case NVS_STATE_FAILED:
        {
            expectedPacket = str_nothing;
            break;
        }
        case NVS_STATE_WAITING_FOR_VERSION:
        {
            expectedPacket = str_version;
            break;
        }
        case NVS_STATE_SENT_NUM_PAIRS_ENTRIES:
        {
            if(expectedAck)
            {
                expectedPacket = str_num_pairs_entries;
            }
            else
            {
                expectedPacket = str_ready;
            }
            break;
        }
        case NVS_STATE_SENT_PAIR_HEADER:
        {
            if(expectedAck)
            {
                expectedPacket = str_pair_header;
            }
            else
            {
                expectedPacket = str_ready;
            }
            break;
        }
        case NVS_STATE_SENT_VALUE_DATA:
        {
            if(expectedAck)
            {
                expectedPacket = str_value_data;
            }
            else
            {
                expectedPacket = str_ready;
            }
            break;
        }
        case NVS_STATE_WAITING_FOR_NUM_PAIRS_ENTRIES:
        {
            expectedPacket = str_num_pairs_entries;
            break;
        }
        case NVS_STATE_WAITING_FOR_PAIR_HEADER:
        {
            expectedPacket = str_pair_header;
            break;
        }
        case NVS_STATE_WAITING_FOR_VALUE_DATA:
        {
            expectedPacket = str_value_data;
            break;
        }
        default:
        {
            expectedPacket = str_unknown_lowercase;
        }
    }

    switch(receivedPacketType)
    {
        case NVS_PACKET_VERSION:
        {
            receivedPacket = str_version;
            break;
        }
        case NVS_PACKET_NUM_PAIRS_AND_ENTRIES:
        {
            receivedPacket = str_num_pairs_entries;
            break;
        }
        case NVS_PACKET_PAIR_HEADER:
        {
            receivedPacket = str_pair_header;
            break;
        }
        case NVS_PACKET_VALUE_DATA:
        {
            receivedPacket = str_value_data;
            break;
        }
        case NVS_PACKET_READY:
        {
            receivedPacket = str_ready;
            break;
        }
        case NVS_PACKET_ERROR:
        {
            receivedPacket = str_error;
            break;
        }
        default:
        {
            receivedPacket = str_unknown_lowercase;
            break;
        }
    }

    snprintf(*outMessage, NVS_MAX_ERROR_MESSAGE_LEN, str_unexpected_packet_msg_format,
             expectedAck ? str_ack_of : "",
             expectedPacket,
             expectedPacket == str_nothing ? "" : str_packet,
             receivedAck ? str_ack : receivedPacket,
             (receivedAck || (receivedPacket == str_nothing)) ? "" : str_packet);
}

void nvsManagerComposeAndSendUnexpectedPacketErrorMessage(nvsPacket_t receivedPacketType, bool receivedAck)
{
#ifdef DEBUG
    if(nvsManager->screen == NVS_SEND)
    {
        printSendStateDebug();
    }
    else
    {
        printRecvStateDebug();
    }
#endif
    char* message = NULL;
    nvsManagerComposeUnexpectedPacketErrorMessage(receivedPacketType, receivedAck, &message);
    nvsManagerSendError(NVS_ERROR_UNEXPECTED_PACKET, message);
    free(message);
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
            nvsManager->fnNextMenu = nvsManagerSetUpManageDataMenu;
            nvsManager->screen = NVS_WARNING;
        }
        else
        {
            nvsManagerSetUpManageDataMenu(true);
        }
    }
    else if(str_send_all == opt)
    {
        nvsManager->sendMode = NVS_SEND_MODE_ALL;
        nvsManager->fnReturnMenu = nvsManagerSetUpTopMenu;
        nvsManagerSetUpSendConnMenu(true);
    }
    else if(str_receive == opt)
    {
        if(!nvsManager->warningAccepted)
        {
            nvsManager->fnNextMenu = nvsManagerSetUpReceiveMenu;
            nvsManager->screen = NVS_WARNING;
        }
        else
        {
            nvsManagerSetUpReceiveMenu(true);
        }
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
        if(eraseNvs())
        {
            switchToSwadgeMode(&modeTest);
        }
        else
        {
            switchToSwadgeMode(&modeMainMenu);
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

    for(uint32_t i = 0; i < nvsManager->nvsNumEntryInfos && nvsManager->menu->numRows < MAX_ROWS; i++)
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

    nvsManager->screen = NVS_MENU;
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

    nvsManager->screen = NVS_MENU;
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

    nvsManager->screen = NVS_MENU;
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
        nvsManager->appliedAnyData = false;
        espNowUseWireless();
        nvsManager->screen = NVS_SEND;
        nvsManagerP2pConnect();
    }
    else if(str_wired == opt)
    {
        nvsManager->wired = true;
        nvsManager->appliedAnyData = false;
        espNowUseSerial(nvsManager->screen == NVS_RECEIVE);
        nvsManager->screen = NVS_SEND;
        nvsManagerP2pConnect();
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

    nvsManager->screen = NVS_MENU;
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
        nvsManagerP2pConnect();
    }
    else if(str_wired == opt)
    {
        nvsManager->wired = true;
        espNowUseSerial(nvsManager->screen == NVS_RECEIVE);
        nvsManager->screen = NVS_RECEIVE;
        nvsManagerP2pConnect();
    }
    else if(str_back == opt)
    {
        nvsManagerSetUpRecvConnMenu(false);
    }
}

/**
 * Set up the menu to apply changes after receiving
 *
 * @param resetPos true to reset the position to 0, false to leave it where it is
 */
void nvsManagerSetUpRecvApplyMenu(bool resetPos)
{
    // Set up the menu
    resetMeleeMenu(nvsManager->menu, str_apply_data, nvsManagerRecvApplyMenuCb);
    addRowToMeleeMenu(nvsManager->menu, str_choose_one);
    addRowToMeleeMenu(nvsManager->menu, str_reset_first);
    addRowToMeleeMenu(nvsManager->menu, str_apply_wo_reset);
    addRowToMeleeMenu(nvsManager->menu, str_back);

    // Set the position
    if(resetPos)
    {
        nvsManager->applyMenuPos = 0;
    }
    nvsManager->menu->selectedRow = nvsManager->applyMenuPos;

    nvsManager->screen = NVS_MENU;
}

/**
 * Callback for the menu to apply changes after receiving
 *
 * @param opt The menu option which was selected
 */
void nvsManagerRecvApplyMenuCb(const char* opt)
{
    // Save the position
    nvsManager->applyMenuPos = nvsManager->menu->selectedRow;

    // Handle the option
    if(str_reset_first == opt)
    {
        if(eraseNvs())
        {
            applyIncomingNvsImage();
            // TODO: handle errors

            // TODO: exit more gracefully
            switchToSwadgeMode(&modeMainMenu);
        }
        else
        {
            // TODO: handle errors
        }
    }
    else if(str_apply_wo_reset == opt)
    {
        applyIncomingNvsImage();
        // TODO: handle errors

        // TODO: exit more gracefully
        nvsManagerSetUpTopMenu(true);
    }
    else if(str_back == opt)
    {
        // We do NOT change anything when returning!
        nvsManager->screen = NVS_RECEIVE;
    }
}

bool nvsManagerReadAllNvsEntryInfos(void)
{
    // Get the number of entry infos so we can allocate the memory
    if(!readAllNvsEntryInfos(&nvsManager->nvsStats, NULL, &nvsManager->nvsNumEntryInfos))
    {
        return false;
    }

    // If nvsEntryInfos is already allocated and it's not going to be big enough, free it first
    if(nvsManager->nvsEntryInfos != NULL && nvsManager->nvsAllocatedEntryInfos < nvsManager->nvsNumEntryInfos)
    {
        free(nvsManager->nvsEntryInfos);
        nvsManager->nvsEntryInfos = NULL;
    }

    // If we just freed nvsEntryInfos or it wasn't allocated to begin with, allocate it
    if(nvsManager->nvsEntryInfos == NULL)
    {
        nvsManager->nvsEntryInfos = calloc(nvsManager->nvsNumEntryInfos, sizeof(nvs_entry_info_t));
        nvsManager->nvsAllocatedEntryInfos = nvsManager->nvsNumEntryInfos;
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

/**
 * @brief Reads an NVS value based on a given entryInfo, and returns value and related info
 *
 * @param entryInfo entryInfo of NVS key/value pair to read
 * @param outNum return integer value if NVS key/value pair contained an integer, or 0 if NVS key/value pair contained a blob. If `NULL`, no value will be returned
 * @param outBlobPtr return pointer to newly allocated blob data if NVS key/value pair contained a blob, or `NULL` if NVS key/value pair contained an integer. If `outBlobPtr*` (pointer at the address passed) is not `NULL`, it will be freed. If `outBlobPtr` (direct value passed) is `NULL`, no value will be returned, and no memory will be allocated
 * @param outEntries return number of NVS entries occupied by key/value pair. If `NULL`, no value will be returned
 * @param outBytes return number of bytes occupied by value. If `NULL`, no value will be returned
 * @return whether the value was read successfully. If `false`, do not use the contents of `outNum` or `outBytes`
 */
bool nvsManagerReadNvsKeyValuePair(nvs_entry_info_t* entryInfo, int32_t* outNum, char** outBlobPtr, uint16_t* outEntries, size_t* outBytes)
{
    bool readSuccess = false;
    if(outBlobPtr != NULL && *outBlobPtr != NULL)
    {
        free(*outBlobPtr);
        *outBlobPtr = NULL;
    }

    switch(entryInfo->type)
    {
        case NVS_TYPE_I32:
        {
            if(outEntries != NULL)
            {
                *outEntries = 1;
            }
            *outBytes = sizeof(int32_t);

            int32_t val;
            readSuccess = readNvs32(entryInfo->key, &val);
            if(readSuccess && outNum != NULL)
            {
                *outNum = val;
            }
            break;
        }
        case NVS_TYPE_BLOB:
        {
            if(outNum != NULL)
            {
                *outNum = 0;
            }

            size_t length;
            readSuccess = readNvsBlob(entryInfo->key, NULL, &length);
            if(readSuccess)
            {
                if(outEntries != NULL)
                {
                    *outEntries = getNumBlobEntries(length);
                }
                if(outBytes != NULL)
                {
                    *outBytes = length;
                }

                char* blob = calloc(1, length);
                readSuccess = readNvsBlob(entryInfo->key, blob, &length);
                if(readSuccess && outBlobPtr != NULL)
                {
                    *outBlobPtr = blob;
                }
                else
                {
                    free(blob);
                }
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
        {
            if(outNum != NULL)
            {
                *outNum = 0;
            }
            if(outEntries != NULL)
            {
                *outEntries = 1;
            }
            if(outBytes != NULL)
            {
                *outBytes = 0;
            }
            break;
        }
        case NVS_TYPE_STR:
        case NVS_TYPE_ANY:
        default:
        {
            if(outNum != NULL)
            {
                *outNum = 0;
            }
            if(outEntries != NULL)
            {
                *outEntries = 0;
            }
            if(outBytes != NULL)
            {
                *outBytes = 0;
            }
            break;
        }
    }

    return readSuccess;
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

#ifdef DEBUG
void printSendPacketDebug(const uint8_t* payload)
{
    ESP_LOGD("NM", "Sending packet type %u", payload[0]);
}

void printRecvPacketDebug(const uint8_t* payload)
{
    ESP_LOGD("NM", "Receiving packet type %u", payload[0]);
}

void printSendStateDebug(void)
{
    ESP_LOGD("NM", "screen:%u, sendMode:%u, commState:%u, pState:%u, lastPktType:%u, lastPktLen:%zu, totalPairs:%u, totalBytes:%zu, totalPkts:%u, pairsRemain:%u, bytesRemain:%zu, pktsRemain:%u, sendEntryInfoIdcesIdx:%u, sendEntryInfoIdx:%u, key:%s",
             nvsManager->screen, nvsManager->sendMode, nvsManager->commState, nvsManager->packetState, (uint8_t) nvsManager->lastPacketSent[0], nvsManager->lastPacketLen, nvsManager->totalNumPairs, nvsManager->totalNumBytesInPair, nvsManager->totalNumPackets, nvsManager->numPairsRemaining, nvsManager->numBytesRemainingInPair, nvsManager->numPacketsRemaining, nvsManager->curSendableEntryInfoIndicesIdx, nvsManager->sendableEntryInfoIndices != NULL ? nvsManager->sendableEntryInfoIndices[nvsManager->curSendableEntryInfoIndicesIdx] : UINT32_MAX, nvsManager->sendableEntryInfoIndices != NULL ? nvsManager->nvsEntryInfos[nvsManager->sendableEntryInfoIndices[nvsManager->curSendableEntryInfoIndicesIdx]].key : "");
}

void printRecvStateDebug(void)
{
    ESP_LOGD("NM", "screen:%u, recvMode:%u, commState:%u, pState:%u, lastPktType:%u, lastPktLen:%zu, totalPairs:%u, totalBytes:%zu, totalPkts:%u, pairsRemain:%u, bytesRemain:%zu, pktsRemain:%u, incomingEntryInfoIdx:%u, key:%s",
             nvsManager->screen, nvsManager->receiveMode, nvsManager->commState, nvsManager->packetState, (uint8_t) nvsManager->lastPacketSent[0], nvsManager->lastPacketLen, nvsManager->totalNumPairs, nvsManager->totalNumBytesInPair, nvsManager->totalNumPackets, nvsManager->numPairsRemaining, nvsManager->numBytesRemainingInPair, nvsManager->numPacketsRemaining, nvsManager->totalNumPackets - nvsManager->numPacketsRemaining, nvsManager->incomingNvsEntryInfos != NULL ? nvsManager->incomingNvsEntryInfos[nvsManager->totalNumPackets - nvsManager->numPacketsRemaining].key : "");
}
#endif