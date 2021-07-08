/* PlantUML documentation

== Connection ==

group Part 1
"Swadge_AB:AB:AB:AB:AB:AB" ->  "Swadge_12:12:12:12:12:12" : "p2p_con" (broadcast)
"Swadge_12:12:12:12:12:12" ->  "Swadge_AB:AB:AB:AB:AB:AB" : "p2p_str_00_AB:AB:AB:AB:AB:AB"
note left: Stop Broadcasting, set p2p->cnc.rxGameStartMsg
"Swadge_AB:AB:AB:AB:AB:AB" ->  "Swadge_12:12:12:12:12:12" : "p2p_ack_00_12:12:12:12:12:12"
note right: set p2p->cnc.rxGameStartAck
end

group Part 2
"Swadge_12:12:12:12:12:12" ->  "Swadge_AB:AB:AB:AB:AB:AB" : "p2p_con" (broadcast)
"Swadge_AB:AB:AB:AB:AB:AB" ->  "Swadge_12:12:12:12:12:12" : "p2p_str_01_12:12:12:12:12:12"
note right: Stop Broadcasting, set p2p->cnc.rxGameStartMsg, become CLIENT
"Swadge_12:12:12:12:12:12" ->  "Swadge_AB:AB:AB:AB:AB:AB" : "p2p_ack_01_AB:AB:AB:AB:AB:AB"
note left: set p2p->cnc.rxGameStartAck, become SERVER
end

== Unreliable Communication Example ==

group Retries & Sequence Numbers
"Swadge_AB:AB:AB:AB:AB:AB" ->x "Swadge_12:12:12:12:12:12" : "p2p_cnt_04_12:12:12:12:12:12_up"
note right: msg not received
"Swadge_AB:AB:AB:AB:AB:AB" ->  "Swadge_12:12:12:12:12:12" : "p2p_cnt_04_12:12:12:12:12:12_up"
note left: first retry, up to five retries
"Swadge_12:12:12:12:12:12" ->x "Swadge_AB:AB:AB:AB:AB:AB" : "p2p_ack_04_AB:AB:AB:AB:AB:AB"
note left: ack not received
"Swadge_AB:AB:AB:AB:AB:AB" ->  "Swadge_12:12:12:12:12:12" : "p2p_cnt_04_12:12:12:12:12:12_up"
note left: second retry
note right: duplicate seq num, ignore message
"Swadge_12:12:12:12:12:12" ->  "Swadge_AB:AB:AB:AB:AB:AB" : "p2p_ack_05_AB:AB:AB:AB:AB:AB"
end

*/

/*============================================================================
 * Includes
 *==========================================================================*/

#include <stdint.h>
#include <string.h>

#include <esp_wifi.h>
#include <esp_random.h>

#include "espNowUtils.h"
#include "p2pConnection.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define P2P_DEBUG_PRINT
#ifdef P2P_DEBUG_PRINT
    #define p2p_printf(...) printf(__VA_ARGS__)
#else
    #define p2p_printf(...)
#endif

// The time we'll spend retrying messages
#define RETRY_TIME_US 3000000

// Time to wait between connection events and game rounds.
// Transmission can be 3s (see above), the round @ 12ms period is 3.636s
// (240 steps of rotation + (252/4) steps of decay) * 12ms
#define FAILURE_RESTART_US 8000000

// Indices into messages
#define CMD_IDX 4
#define SEQ_IDX 8
#define MAC_IDX 11
#define EXT_IDX 29

/*============================================================================
 * Variables
 *==========================================================================*/

// Messages to send.
const char p2pConnectionMsgFmt[] = "%s_con";
const char p2pNoPayloadMsgFmt[]  = "%s_%s_%02d_%02X:%02X:%02X:%02X:%02X:%02X";
const char p2pPayloadMsgFmt[]    = "%s_%s_%02d_%02X:%02X:%02X:%02X:%02X:%02X_%s";
const char p2pMacFmt[] = "%02X:%02X:%02X:%02X:%02X:%02X";

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void p2pConnectionTimeout(void* arg);
void p2pTxAllRetriesTimeout(void* arg);
void p2pTxRetryTimeout(void* arg);
void p2pRestart(void* arg);
void p2pStartRestartTimer(void* arg);
void p2pProcConnectionEvt(p2pInfo* p2p, connectionEvt_t event);
void p2pGameStartAckRecv(void* arg);
void p2pSendAckToMac(p2pInfo* p2p, const uint8_t* mac_addr);
void p2pSendMsgEx(p2pInfo* p2p, char* msg, uint16_t len,
                  bool shouldAck, void (*success)(void*), void (*failure)(void*));
void p2pModeMsgSuccess(void* arg);
void p2pModeMsgFailure(void* arg);

/*============================================================================
 * Functions
 *==========================================================================*/

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
 *                      swadge, 0 is first one to see around 55 the swadges need
 *                      to be right next to eachother.
 */
void p2pInitialize(p2pInfo* p2p, char* msgId,
                   p2pConCbFn conCbFn,
                   p2pMsgRxCbFn msgRxCbFn, uint8_t connectionRssi)
{
    p2p_printf("%s\n", __func__);
    // Make sure everything is zero!
    memset(p2p, 0, sizeof(p2pInfo));

    // Set the callback functions for connection and message events
    p2p->conCbFn = conCbFn;
    p2p->msgRxCbFn = msgRxCbFn;

    // Set the initial sequence number at 255 so that a 0 received is valid.
    p2p->cnc.lastSeqNum = 255;

    // Set the connection Rssi, the higher the value, the closer the swadges
    // need to be.
    p2p->connectionRssi = connectionRssi;

    // Set the three character message ID
    strncpy(p2p->msgId, msgId, sizeof(p2p->msgId));

    // Get and save the string form of our MAC address
    uint8_t mymac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mymac);
    snprintf(p2p->cnc.macStr, sizeof(p2p->cnc.macStr), p2pMacFmt,
             mymac[0],
             mymac[1],
             mymac[2],
             mymac[3],
             mymac[4],
             mymac[5]);

    // Set up the connection message
    snprintf(p2p->conMsg, sizeof(p2p->conMsg), p2pConnectionMsgFmt,
             p2p->msgId);

    // Set up dummy ACK message
    snprintf(p2p->ackMsg, sizeof(p2p->ackMsg), p2pNoPayloadMsgFmt,
             p2p->msgId,
             "ack",
             0,
             0xFF,
             0xFF,
             0xFF,
             0xFF,
             0xFF,
             0xFF);

    // Set up dummy start message
    snprintf(p2p->startMsg, sizeof(p2p->startMsg), p2pNoPayloadMsgFmt,
             p2p->msgId,
             "str",
             0,
             0xFF,
             0xFF,
             0xFF,
             0xFF,
             0xFF,
             0xFF);

    // Set up a timer for acking messages
    esp_timer_create_args_t p2pTxRetryTimeoutArgs =
    {
        .callback = p2pTxRetryTimeout,
        .arg = p2p,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "p2ptrt",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&p2pTxRetryTimeoutArgs, &p2p->tmr.TxRetry);

    // Set up a timer for when a message never gets ACKed
    esp_timer_create_args_t p2pTxAllRetriesTimeoutArgs =
    {
        .callback = p2pTxAllRetriesTimeout,
        .arg = p2p,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "p2ptart",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&p2pTxAllRetriesTimeoutArgs, &p2p->tmr.TxAllRetries);

    // Set up a timer to restart after abject failure
    esp_timer_create_args_t p2pRestartArgs =
    {
        .callback = p2pRestart,
        .arg = p2p,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "p2pr",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&p2pRestartArgs, &p2p->tmr.Reinit);

    // Set up a timer to do an initial connection
    esp_timer_create_args_t p2pConnectionTimeoutArgs =
    {
        .callback = p2pConnectionTimeout,
        .arg = p2p,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "p2pct",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&p2pConnectionTimeoutArgs, &p2p->tmr.Connection);
}

/**
 * Start the connection process by sending broadcasts and notify the mode
 *
 * @param p2p The p2pInfo struct with all the state information
 */
void p2pStartConnection(p2pInfo* p2p)
{
    p2p_printf("%s\n", __func__);
    esp_timer_start_once(p2p->tmr.Connection, 1000);

    if(NULL != p2p->conCbFn)
    {
        p2p->conCbFn(p2p, CON_STARTED);
    }
}

/**
 * Stop up all timers
 *
 * @param p2p The p2pInfo struct with all the state information
 */
void p2pDeinit(p2pInfo* p2p)
{
    p2p_printf("%s\n", __func__);

    esp_timer_stop(p2p->tmr.Connection);
    esp_timer_stop(p2p->tmr.TxRetry);
    esp_timer_stop(p2p->tmr.Reinit);
    esp_timer_stop(p2p->tmr.TxAllRetries);
}

/**
 * Send a broadcast connection message
 *
 * Called periodically, with some randomness mixed in from the tmr.Connection
 * timer. The timer is set when connection starts and is stopped when we
 * receive a response to our connection broadcast
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pConnectionTimeout(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    // Send a connection broadcast
    p2pSendMsgEx(p2p, p2p->conMsg, strlen(p2p->conMsg), false, NULL, NULL);

    // esp_random returns a 32 bit number, so this is [500ms,1500ms]
    uint32_t timeoutUs = 1000 * (100 * (5 + (esp_random() % 11)));

    // Start the timer again
    p2p_printf("retry broadcast in %dus\n", timeoutUs);
    esp_timer_start_once(p2p->tmr.Connection, timeoutUs);
}

/**
 * Retries sending a message to be acked
 *
 * Called from the tmr.TxRetry timer. The timer is set when a message to be
 * ACKed is sent and cleared when an ACK is received
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pTxRetryTimeout(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;

    if(p2p->ack.msgToAckLen > 0)
    {
        p2p_printf("Retrying message \"%s\"\n", p2p->ack.msgToAck);
        p2pSendMsgEx(p2p, p2p->ack.msgToAck, p2p->ack.msgToAckLen, true, p2p->ack.SuccessFn, p2p->ack.FailureFn);
    }
}

/**
 * Stops a message transmission attempt after all retries have been exhausted
 * and calls p2p->ack.FailureFn() if a function was given
 *
 * Called from the tmr.TxAllRetries timer. The timer is set when a message to
 * be ACKed is sent for the first time and cleared when the message is ACKed.
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pTxAllRetriesTimeout(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;

    // Disarm all timers
    esp_timer_stop(p2p->tmr.TxRetry);
    esp_timer_stop(p2p->tmr.TxAllRetries);

    // Call the failure function
    p2p_printf("Message totally failed \"%s\"\n", p2p->ack.msgToAck);
    if(NULL != p2p->ack.FailureFn)
    {
        p2p->ack.FailureFn(p2p);
    }

    // Clear out the ack variables
    memset(&p2p->ack, 0, sizeof(p2p->ack));
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
void p2pSendMsg(p2pInfo* p2p, char* msg, char* payload,
                uint16_t len, p2pMsgTxCbFn msgTxCbFn)
{
    p2p_printf("%s\n", __func__);

    char builtMsg[64] = {0};

    if(NULL == payload || len == 0)
    {
        snprintf(builtMsg, sizeof(builtMsg), p2pNoPayloadMsgFmt,
                 p2p->msgId,
                 msg,
                 0, // sequence number
                 p2p->cnc.otherMac[0],
                 p2p->cnc.otherMac[1],
                 p2p->cnc.otherMac[2],
                 p2p->cnc.otherMac[3],
                 p2p->cnc.otherMac[4],
                 p2p->cnc.otherMac[5]);
    }
    else
    {
        snprintf(builtMsg, sizeof(builtMsg), p2pPayloadMsgFmt,
                 p2p->msgId,
                 msg,
                 0, // sequence number, filled in later
                 p2p->cnc.otherMac[0],
                 p2p->cnc.otherMac[1],
                 p2p->cnc.otherMac[2],
                 p2p->cnc.otherMac[3],
                 p2p->cnc.otherMac[4],
                 p2p->cnc.otherMac[5],
                 payload);
    }

    p2p->msgTxCbFn = msgTxCbFn;
    p2pSendMsgEx(p2p, builtMsg, strlen(builtMsg), true, p2pModeMsgSuccess, p2pModeMsgFailure);
}

/**
 * Callback function for when a message sent by the Swadge mode, not during
 * the connection process, is ACKed
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pModeMsgSuccess(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    if(NULL != p2p->msgTxCbFn)
    {
        p2p->msgTxCbFn(p2p, MSG_ACKED);
    }
}

/**
 * Callback function for when a message sent by the Swadge mode, not during
 * the connection process, is dropped
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pModeMsgFailure(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    if(NULL != p2p->msgTxCbFn)
    {
        p2p->msgTxCbFn(p2p, MSG_FAILED);
    }
}

/**
 * Wrapper for sending an ESP-NOW message. Handles ACKing and retries for
 * non-broadcast style messages
 *
 * @param p2p       The p2pInfo struct with all the state information
 * @param msg       The message to send, may contain destination MAC
 * @param len       The length of the message to send
 * @param shouldAck true if this message should be acked, false if we don't care
 * @param success   A callback function if the message is acked. May be NULL
 * @param failure   A callback function if the message isn't acked. May be NULL
 */
void p2pSendMsgEx(p2pInfo* p2p, char* msg, uint16_t len,
                  bool shouldAck, void (*success)(void*), void (*failure)(void*))
{
    p2p_printf("%s\n", __func__);

    // If this is a first time message and longer than a connection message
    if( (p2p->ack.msgToAck != msg) && strlen(p2p->conMsg) < len)
    {
        // Insert a sequence number
        msg[SEQ_IDX + 0] = '0' + (p2p->cnc.mySeqNum / 10);
        msg[SEQ_IDX + 1] = '0' + (p2p->cnc.mySeqNum % 10);

        // Increment the sequence number, 0-99
        p2p->cnc.mySeqNum++;
        if(100 == p2p->cnc.mySeqNum++)
        {
            p2p->cnc.mySeqNum = 0;
        }
    }

#ifdef P2P_DEBUG_PRINT
    char* dbgMsg = (char*)malloc(sizeof(char) * (len + 1));
    memcpy(dbgMsg, msg, len);
    dbgMsg[len] = 0;
    p2p_printf("%s: %s\n", __func__, dbgMsg);
    free(dbgMsg);
#endif

    if(shouldAck)
    {
        // Set the state to wait for an ack
        p2p->ack.isWaitingForAck = true;

        // If this is not a retry
        if(p2p->ack.msgToAck != msg)
        {
            p2p_printf("sending for the first time\n");

            // Store the message for potential retries
            memcpy(p2p->ack.msgToAck, msg, len);
            p2p->ack.msgToAckLen = len;
            p2p->ack.SuccessFn = success;
            p2p->ack.FailureFn = failure;

            // Start a timer to retry for 3s total
            esp_timer_stop(p2p->tmr.TxAllRetries);
            esp_timer_start_once(p2p->tmr.TxAllRetries, RETRY_TIME_US);
        }
        else
        {
            p2p_printf("this is a retry\n");
        }

        // Mark the time this transmission started, the retry timer gets
        // started in p2pSendCb()
        p2p->ack.timeSentUs = esp_timer_get_time();
    }
    espNowSend((const uint8_t*)msg, len);
}

/**
 * This function must be called whenever RSSI data is received for a promiscuous
 * packet. The packet must be scanned for the conMsg to see if it's for p2p.
 *
 * @param p2p The p2pInfo struct with all the state information
 * @param pkt The promiscuous packet, including payload and RSSI information
 */
void p2pRssiCb(p2pInfo* p2p, const wifi_promiscuous_pkt_t* pkt)
{
    // TODO ESP-NOW payload starts at pkt->payload[39]. Always like that??
    if(NULL != memmem(pkt->payload, pkt->rx_ctrl.sig_len, p2p->conMsg, strlen(p2p->conMsg)))
    {
        // TODO WIFI find and save MAC if RSSI is good
        printf("RSSI %d from [%02X:%02X:%02X:%02X:%02X:%02X]\n", pkt->rx_ctrl.rssi,
               pkt->payload[10],
               pkt->payload[11],
               pkt->payload[12],
               pkt->payload[13],
               pkt->payload[14],
               pkt->payload[15]);

        // char dbg[512] = {0};
        // snprintf(dbg, sizeof(dbg) - 1, "RSSI %d ~~ ", pkt->rx_ctrl.rssi);

        // char tmp[16] = {0};
        // for(int i = 0; i < pkt->rx_ctrl.sig_len; i++)
        // {
        //     snprintf(tmp, sizeof(tmp) - 1, "%02X ", pkt->payload[i]);
        //     strncat(dbg, tmp, sizeof(dbg) - strlen(dbg) - 1);
        // }
        // printf("%s\n", dbg);
    }
}

/**
 * This function must be called whenever an ESP NOW packet is received
 *
 * @param p2p      The p2pInfo struct with all the state information
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 * @return false if the message was processed here,
 *         true if the message should be processed by the swadge mode
 */
void p2pRecvCb(p2pInfo* p2p, const uint8_t* mac_addr, const uint8_t* data, uint8_t len)
{
    // TODO WIFI holy moly this works???
    wifi_pkt_rx_ctrl_t* pkt = (wifi_pkt_rx_ctrl_t*)&data[-sizeof(wifi_pkt_rx_ctrl_t) - 39];
    printf(" ~~ RSSI %d\n", pkt->rssi);

#ifdef P2P_DEBUG_PRINT
    char* dbgMsg = (char*)malloc(sizeof(char) * (len + 1));
    memcpy(dbgMsg, data, len);
    dbgMsg[len] = 0;
    p2p_printf("%s: %s\n", __func__, dbgMsg);
    free(dbgMsg);
#endif

    // Check if this message matches our message ID
    if(len < CMD_IDX ||
            (0 != memcmp(data, p2p->conMsg, CMD_IDX)))
    {
        // This message is too short, or does not match our message ID
        p2p_printf("DISCARD: Not a message for '%s'\n", p2p->msgId);
        return;
    }

    // If this message has a MAC, check it
    if(len >= strlen(p2p->ackMsg) &&
            0 != memcmp(&data[MAC_IDX], p2p->cnc.macStr, strlen(p2p->cnc.macStr)))
    {
        // This MAC isn't for us
        p2p_printf("DISCARD: Not for our MAC\n");
        return;
    }

    // If this is anything besides a broadcast, check the other MAC
    if(p2p->cnc.otherMacReceived &&
            len > strlen(p2p->conMsg) &&
            0 != memcmp(mac_addr, p2p->cnc.otherMac, sizeof(p2p->cnc.otherMac)))
    {
        // This isn't from the other known swadge
        p2p_printf("DISCARD: Not from the other MAC\n");
        return;
    }

    // By here, we know the received message matches our message ID, either a
    // broadcast or for us. If this isn't an ack message, ack it
    if(len >= SEQ_IDX &&
            0 != memcmp(data, p2p->ackMsg, SEQ_IDX))
    {
        p2pSendAckToMac(p2p, mac_addr);
    }

    // After ACKing the message, check the sequence number to see if we should
    // process it or ignore it (we already did!)
    if(len >= strlen(p2p->ackMsg))
    {
        // Extract the sequence number
        uint8_t theirSeq = 0;
        theirSeq += (data[SEQ_IDX + 0] - '0') * 10;
        theirSeq += (data[SEQ_IDX + 1] - '0');

        // Check it against the last known sequence number
        if(theirSeq == p2p->cnc.lastSeqNum)
        {
            p2p_printf("DISCARD: Duplicate sequence number\n");
            return;
        }
        else
        {
            p2p->cnc.lastSeqNum = theirSeq;
            p2p_printf("Store lastSeqNum %d\n", p2p->cnc.lastSeqNum);
        }
    }

    // ACKs can be received in any state
    if(p2p->ack.isWaitingForAck)
    {
        // Check if this is an ACK
        if(strlen(p2p->ackMsg) == len &&
                0 == memcmp(data, p2p->ackMsg, SEQ_IDX))
        {
            p2p_printf("ACK Received\n");

            // Call the function after receiving the ack
            if(NULL != p2p->ack.SuccessFn)
            {
                p2p->ack.SuccessFn(p2p);
            }

            // Clear ack timeout variables
            esp_timer_stop(p2p->tmr.TxRetry);
            // Disarm the whole transmission ack timer
            esp_timer_stop(p2p->tmr.TxAllRetries);
            // Clear out ACK variables
            memset(&p2p->ack, 0, sizeof(p2p->ack));

            p2p->ack.isWaitingForAck = false;
        }
        // Don't process anything else when waiting for an ack
        return;
    }

    if(false == p2p->cnc.isConnected)
    {
        // Received another broadcast, Check if this RSSI is strong enough
        if(!p2p->cnc.broadcastReceived &&
                // rssi > p2p->connectionRssi && // TODO WIFI check RSSI
                strlen(p2p->conMsg) == len &&
                0 == memcmp(data, p2p->conMsg, len))
        {

            // We received a broadcast, don't allow another
            p2p->cnc.broadcastReceived = true;

            // Save the other ESP's MAC
            memcpy(p2p->cnc.otherMac, mac_addr, sizeof(p2p->cnc.otherMac));
            p2p->cnc.otherMacReceived = true;

            // Send a message to that ESP to start the game.
            snprintf(p2p->startMsg, sizeof(p2p->startMsg), p2pNoPayloadMsgFmt,
                     p2p->msgId,
                     "str",
                     0,
                     mac_addr[0],
                     mac_addr[1],
                     mac_addr[2],
                     mac_addr[3],
                     mac_addr[4],
                     mac_addr[5]);

            // If it's acked, call p2pGameStartAckRecv(), if not reinit with p2pRestart()
            p2pSendMsgEx(p2p, p2p->startMsg, strlen(p2p->startMsg), true, p2pGameStartAckRecv, p2pRestart);
        }
        // Received a response to our broadcast
        else if (!p2p->cnc.rxGameStartMsg &&
                 strlen(p2p->startMsg) == len &&
                 0 == memcmp(data, p2p->startMsg, SEQ_IDX))
        {
            p2p_printf("Game start message received, ACKing\n");

            // This is another swadge trying to start a game, which means
            // they received our p2p->conMsg. First disable our p2p->conMsg
            esp_timer_stop(p2p->tmr.Connection);

            // And process this connection event
            p2pProcConnectionEvt(p2p, RX_GAME_START_MSG);
        }
        return;
    }
    else
    {
        p2p_printf("cnc.isconnected is true\n");
        // Let the mode handle it
        if(NULL != p2p->msgRxCbFn)
        {
            p2p_printf("letting mode handle message\n");
            char msgType[4] = {0};
            memcpy(msgType, &data[CMD_IDX], 3 * sizeof(char));
            p2p->msgRxCbFn(p2p, msgType, &data[EXT_IDX], len - EXT_IDX);
        }
    }
}

/**
 * Helper function to send an ACK message to the given MAC
 *
 * @param p2p      The p2pInfo struct with all the state information
 * @param mac_addr The MAC to address this ACK to
 */
void p2pSendAckToMac(p2pInfo* p2p, const uint8_t* mac_addr)
{
    p2p_printf("%s\n", __func__);

    snprintf(p2p->ackMsg, sizeof(p2p->ackMsg), p2pNoPayloadMsgFmt,
             p2p->msgId,
             "ack",
             0,
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5]);
    p2pSendMsgEx(p2p, p2p->ackMsg, strlen(p2p->ackMsg), false, NULL, NULL);
}

/**
 * This is called when p2p->startMsg is acked and processes the connection event
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pGameStartAckRecv(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    p2pProcConnectionEvt(p2p, RX_GAME_START_ACK);
}

/**
 * Two steps are necessary to establish a connection in no particular order.
 * 1. This swadge has to receive a start message from another swadge
 * 2. This swadge has to receive an ack to a start message sent to another swadge
 * The order of events determines who is the 'client' and who is the 'server'
 *
 * @param p2p   The p2pInfo struct with all the state information
 * @param event The event that occurred
 */
void p2pProcConnectionEvt(p2pInfo* p2p, connectionEvt_t event)
{
    p2p_printf("%s evt: %d, p2p->cnc.rxGameStartMsg %d, p2p->cnc.rxGameStartAck %d\n", __func__, event,
               p2p->cnc.rxGameStartMsg, p2p->cnc.rxGameStartAck);

    switch(event)
    {
        case RX_GAME_START_MSG:
        {
            // Already received the ack, become the client
            if(!p2p->cnc.rxGameStartMsg && p2p->cnc.rxGameStartAck)
            {
                p2p->cnc.playOrder = GOING_SECOND;
            }
            // Mark this event
            p2p->cnc.rxGameStartMsg = true;
            break;
        }
        case RX_GAME_START_ACK:
        {
            // Already received the msg, become the server
            if(!p2p->cnc.rxGameStartAck && p2p->cnc.rxGameStartMsg)
            {
                p2p->cnc.playOrder = GOING_FIRST;
            }
            // Mark this event
            p2p->cnc.rxGameStartAck = true;
            break;
        }
        case CON_STARTED:
        case CON_ESTABLISHED:
        case CON_LOST:
        default:
        {
            break;
        }
    }

    if(NULL != p2p->conCbFn)
    {
        p2p->conCbFn(p2p, event);
    }

    // If both the game start messages are good, start the game
    if(p2p->cnc.rxGameStartMsg && p2p->cnc.rxGameStartAck)
    {
        // Connection was successful, so disarm the failure timer
        esp_timer_stop(p2p->tmr.Reinit);

        p2p->cnc.isConnected = true;

        // tell the mode it's connected
        if(NULL != p2p->conCbFn)
        {
            p2p->conCbFn(p2p, CON_ESTABLISHED);
        }
    }
    else
    {
        // Start a timer to reinit if we never finish connection
        p2pStartRestartTimer(p2p);
    }
}

/**
 * This starts a timer to call p2pRestart(), used in case of a failure
 * The timer is set when one half of the necessary connection messages is received
 * The timer is disarmed when the connection is established
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pStartRestartTimer(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    // If the connection isn't established in FAILURE_RESTART_MS, restart
    esp_timer_start_once(p2p->tmr.Reinit, FAILURE_RESTART_US);
}

/**
 * Restart by deiniting then initing. Persist the msgId and p2p->conCbFn
 * fields
 *
 * @param arg The p2pInfo struct with all the state information
 */
void p2pRestart(void* arg)
{
    p2p_printf("%s\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;

    if(NULL != p2p->conCbFn)
    {
        p2p->conCbFn(p2p, CON_LOST);
    }

    char msgId[4] = {0};
    strncpy(msgId, p2p->msgId, sizeof(msgId));
    p2pDeinit(p2p);
    p2pInitialize(p2p, msgId, p2p->conCbFn, p2p->msgRxCbFn, p2p->connectionRssi);
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
void p2pSendCb(p2pInfo* p2p, const uint8_t* mac_addr, esp_now_send_status_t status)
{
    p2p_printf("%s\n", __func__);

    switch(status)
    {
        case ESP_NOW_SEND_SUCCESS:
        {
            if(0 != p2p->ack.timeSentUs)
            {
                uint32_t transmissionTimeUs = esp_timer_get_time() - p2p->ack.timeSentUs;
                p2p_printf("Transmission time %dus\n", transmissionTimeUs);
                // The timers are all millisecond, so make sure that
                // transmissionTimeUs is at least 1ms
                if(transmissionTimeUs < 1000)
                {
                    transmissionTimeUs = 1000;
                }

                // Round it to the nearest Ms, add 69ms (the measured worst case)
                // then add some randomness [0ms to 15ms random]
                uint32_t waitTimeUs = 1000 * (((transmissionTimeUs + 500) / 1000) + 69 + (esp_random() & 0b1111));

                // Start the timer
                p2p_printf("ack timer set for %dus\n", waitTimeUs);
                esp_timer_start_once(p2p->tmr.TxRetry, waitTimeUs);
            }
            break;
        }
        default:
        case ESP_NOW_SEND_FAIL:
        {
            // If a message is stored
            if(p2p->ack.msgToAckLen > 0)
            {
                // try again in 1ms
                esp_timer_start_once(p2p->tmr.TxRetry, 1000);
            }
            break;
        }
    }
}

/**
 * After the swadge is connected to another, return whether this Swadge is
 * player 1 or player 2. This can be used to determine client/server roles
 *
 * @param p2p The p2pInfo struct with all the state information
 * @return    GOING_SECOND, GOING_FIRST, or NOT_SET
 */
playOrder_t p2pGetPlayOrder(p2pInfo* p2p)
{
    if(p2p->cnc.isConnected)
    {
        return p2p->cnc.playOrder;
    }
    else
    {
        return NOT_SET;
    }
}

/**
 * Override whether the Swadge is player 1 or player 2. You probably shouldn't
 * do this, but you might want to for single player modes
 *
 * @param p2p The p2pInfo struct with all the state information
 */
void p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order)
{
    p2p->cnc.playOrder = order;
}
