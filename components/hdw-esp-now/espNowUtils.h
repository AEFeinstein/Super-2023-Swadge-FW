/*
 * espNowUtils.h
 *
 *  Created on: Oct 29, 2018
 *      Author: adam
 */

#ifndef USER_ESPNOWUTILS_H_
#define USER_ESPNOWUTILS_H_

/*============================================================================
 * Prototypes
 *==========================================================================*/

#include <esp_now.h>

typedef void (*hostEspNowRecvCb_t)(const uint8_t* mac_addr, const uint8_t* data, uint8_t len);
typedef void (*hostEspNowSendCb_t)(const uint8_t* mac_addr, esp_now_send_status_t status);
typedef void (*hostEspNowRssiCb_t)(const wifi_promiscuous_pkt_t* pkt);

void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb, hostEspNowRssiCb_t rssiCb);
void espNowDeinit(void);

void espNowSend(const uint8_t* data, uint8_t len);

#endif /* USER_ESPNOWUTILS_H_ */
