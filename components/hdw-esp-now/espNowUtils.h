/*
 * espNowUtils.h
 *
 *  Created on: Oct 29, 2018
 *      Author: adam
 */

#ifndef USER_ESPNOWUTILS_H_
#define USER_ESPNOWUTILS_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <esp_now.h>
#include <hal/gpio_types.h>
#include <driver/uart.h>

//==============================================================================
// Structs
//==============================================================================

typedef struct __attribute__((packed))
{
    uint8_t macHeader[24];
    uint8_t categoryCode;
    uint8_t organizationIdentifier[3];
    uint8_t randomValues[4];
    uint8_t elementID;
    uint8_t length;
    uint8_t organizationIdentifier_2[3];
    uint8_t type;
    uint8_t version;
}
espNowHeader_t;

//==============================================================================
// Prototypes
//==============================================================================

typedef void (*hostEspNowRecvCb_t)(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
typedef void (*hostEspNowSendCb_t)(const uint8_t* mac_addr, esp_now_send_status_t status);

void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb,
    gpio_num_t rx, gpio_num_t tx, uart_port_t uart);
void espNowDeinit(void);

void espNowUseWireless(void);
void espNowUseSerial(bool crossoverPins);

void espNowSend(const char* data, uint8_t len);
void checkEspNowRxQueue(void);

#endif /* USER_ESPNOWUTILS_H_ */
