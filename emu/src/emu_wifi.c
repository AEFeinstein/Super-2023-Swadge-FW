#include "esp_emu.h"
#include "esp_log.h"

#include "espNowUtils.h"
#include "p2pConnection.h"

//==============================================================================
// ESP Now
//==============================================================================

/**
 * @brief TODO
 * 
 * @param recvCb 
 * @param sendCb 
 */
void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 */
void espNowDeinit(void)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param data 
 * @param len 
 */
void espNowSend(const uint8_t* data, uint8_t len)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 */
void checkEspNowRxQueue(void)
{
    WARN_UNIMPLEMENTED();
}

//==============================================================================
// P2P Connection
//==============================================================================

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param msgId 
 * @param conCbFn 
 * @param msgRxCbFn 
 * @param connectionRssi 
 */
void p2pInitialize(p2pInfo* p2p, char* msgId,
                   p2pConCbFn conCbFn,
                   p2pMsgRxCbFn msgRxCbFn, int8_t connectionRssi)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 */
void p2pDeinit(p2pInfo* p2p)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 */
void p2pStartConnection(p2pInfo* p2p)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param msg 
 * @param payload 
 * @param len 
 * @param msgTxCbFn 
 */
void p2pSendMsg(p2pInfo* p2p, char* msg, char* payload, uint16_t len, p2pMsgTxCbFn msgTxCbFn)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param mac_addr 
 * @param status 
 */
void p2pSendCb(p2pInfo* p2p, const uint8_t* mac_addr, esp_now_send_status_t status)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param mac_addr 
 * @param data 
 * @param len 
 * @param rssi 
 */
void p2pRecvCb(p2pInfo* p2p, const uint8_t* mac_addr, const uint8_t* data, uint8_t len, int8_t rssi)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @return playOrder_t 
 */
playOrder_t p2pGetPlayOrder(p2pInfo* p2p)
{
    WARN_UNIMPLEMENTED();
    return NOT_SET;
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param order 
 */
void p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order)
{
    WARN_UNIMPLEMENTED();
}
