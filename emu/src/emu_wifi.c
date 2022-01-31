//==============================================================================
// Includes
//==============================================================================

#include "emu_esp.h"
#include "esp_log.h"

#include "espNowUtils.h"
#include "p2pConnection.h"

//==============================================================================
// ESP Now
//==============================================================================

/**
 * Initialize ESP-NOW and attach callback functions
 *
 * @param recvCb A callback to call when data is sent
 * @param sendCb A callback to call when data is received
 */
void espNowInit(hostEspNowRecvCb_t recvCb UNUSED, hostEspNowSendCb_t sendCb UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * This function is automatically called to de-initialize ESP-NOW
 */
void espNowDeinit(void)
{
    WARN_UNIMPLEMENTED();
}

/**
 * This is a wrapper for esp_now_send. It also sets the wifi power with
 * wifi_set_user_fixed_rate()
 *
 * @param data The data to broadcast using ESP NOW
 * @param len  The length of the data to broadcast
 */
void espNowSend(const uint8_t* data UNUSED, uint8_t len UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * Check the ESP NOW receive queue. If there are any received packets, send
 * them to hostEspNowRecvCb()
 */
void checkEspNowRxQueue(void)
{
    WARN_UNIMPLEMENTED();
}

//==============================================================================
// P2P Connection
//==============================================================================

/**
 * @brief Initialize the p2p connection protocol
 *
 * @param p2p           The p2pInfo struct with all the state information
 * @param msgId         A three character, null terminated message ID. Must be
 *                      unique per-swadge mode.
 * @param conCbFn A function pointer which will be called when connection
 *                      events occur
 * @param msgRxCbFn A function pointer which will be called when a packet
 *                      is received for the swadge mode
 * @param connectionRssi The strength needed to start a connection with another
 *                      swadge. A positive value means swadges are quite close.
 *                      I've seen RSSI go as low as -70
 */
void p2pInitialize(p2pInfo* p2p UNUSED, char* msgId UNUSED,
                   p2pConCbFn conCbFn UNUSED,
                   p2pMsgRxCbFn msgRxCbFn UNUSED, int8_t connectionRssi UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * Stop up all timers
 *
 * @param p2p The p2pInfo struct with all the state information
 */
void p2pDeinit(p2pInfo* p2p UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * Start the connection process by sending broadcasts and notify the mode
 *
 * @param p2p The p2pInfo struct with all the state information
 */
void p2pStartConnection(p2pInfo* p2p UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * Send a message from one Swadge to another. This must not be called before
 * the CON_ESTABLISHED event occurs. Message addressing, ACKing, and retries
 * all happen automatically
 *
 * @param p2p       The p2pInfo struct with all the state information
 * @param msg       The mandatory three char message type
 * @param payload   An optional message payload string, may be NULL, up to 32 chars
 * @param len       The length of the optional message payload string. May be 0
 * @param msgTxCbFn A callback function when this message is ACKed or dropped
 */
void p2pSendMsg(p2pInfo* p2p UNUSED, char* msg UNUSED, char* payload UNUSED,
    uint16_t len UNUSED, p2pMsgTxCbFn msgTxCbFn UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * This must be called by whatever function is registered to the Swadge mode's
 * fnEspNowSendCb
 *
 * This is called after an attempted transmission. If it was successful, and the
 * message should be acked, start a retry timer. If it wasn't successful, just
 * try again
 *
 * @param p2p      The p2pInfo struct with all the state information
 * @param mac_addr unused
 * @param status   Whether the transmission succeeded or failed
 */
void p2pSendCb(p2pInfo* p2p UNUSED, const uint8_t* mac_addr UNUSED,
    esp_now_send_status_t status UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * This function must be called whenever an ESP NOW packet is received
 *
 * @param p2p      The p2pInfo struct with all the state information
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 * @param rssi     The rssi of the received data
 * @return false if the message was processed here,
 *         true if the message should be processed by the swadge mode
 */
void p2pRecvCb(p2pInfo* p2p UNUSED, const uint8_t* mac_addr UNUSED,
    const uint8_t* data UNUSED, uint8_t len UNUSED, int8_t rssi UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * After the swadge is connected to another, return whether this Swadge is
 * player 1 or player 2. This can be used to determine client/server roles
 *
 * @param p2p The p2pInfo struct with all the state information
 * @return    GOING_SECOND, GOING_FIRST, or NOT_SET
 */
playOrder_t p2pGetPlayOrder(p2pInfo* p2p UNUSED)
{
    WARN_UNIMPLEMENTED();
    return NOT_SET;
}

/**
 * Override whether the Swadge is player 1 or player 2. You probably shouldn't
 * do this, but you might want to for single player modes
 *
 * @param p2p The p2pInfo struct with all the state information
 */
void p2pSetPlayOrder(p2pInfo* p2p UNUSED, playOrder_t order UNUSED)
{
    WARN_UNIMPLEMENTED();
}
