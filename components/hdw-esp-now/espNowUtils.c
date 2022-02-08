/*
 * espNowUtils.c
 *
 *  Created on: Oct 29, 2018
 *      Author: adam
 */

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_err.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "espNowUtils.h"
#include "../../main/p2pConnection.h"

//==============================================================================
// Defines
//==============================================================================

#define SOFTAP_CHANNEL 11

//==============================================================================
// Variables
//==============================================================================

/// This is the MAC address to transmit to for broadcasting
const uint8_t espNowBroadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

hostEspNowRecvCb_t hostEspNowRecvCb = NULL;
hostEspNowSendCb_t hostEspNowSendCb = NULL;

static xQueueHandle esp_now_queue = NULL;

//==============================================================================
// Prototypes
//==============================================================================

void espNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, int data_len);
void espNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize ESP-NOW and attach callback functions
 *
 * @param recvCb A callback to call when data is sent
 * @param sendCb A callback to call when data is received
 */
void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb)
{
    hostEspNowRecvCb = recvCb;
    hostEspNowSendCb = sendCb;

    // Create a queue to move packets from the receive callback to the main task
    esp_now_queue = xQueueCreate(10, sizeof(p2pPacket_t));

    esp_err_t err;

    if (ESP_OK != (err = esp_netif_init()))
    {
        ESP_LOGD("ESPNOW", "Couldn't init netif %s", esp_err_to_name(err));
        return;
    }

    if (ESP_OK != (err = esp_event_loop_create_default()))
    {
        ESP_LOGD("ESPNOW", "Couldn't create event loop %s", esp_err_to_name(err));
        return;
    }

    wifi_init_config_t conf = WIFI_INIT_CONFIG_DEFAULT();
    if (ESP_OK != (err = esp_wifi_init(&conf)))
    {
        ESP_LOGD("ESPNOW", "Couldn't init wifi %s", esp_err_to_name(err));
        return;
    }

    if (ESP_OK != (err = esp_wifi_set_storage(WIFI_STORAGE_RAM)))
    {
        ESP_LOGD("ESPNOW", "Couldn't set wifi storage %s", esp_err_to_name(err));
        return;
    }

    // Set up all the wifi softAP mode configs
    if(ESP_OK != (err = esp_wifi_set_mode(WIFI_MODE_AP)))
    {
        ESP_LOGD("ESPNOW", "Could not set as station mode");
        return;
    }

    wifi_config_t config =
    {
        .ap =
        {
            .ssid = "",                 /**< SSID of ESP32 soft-AP. If ssid_len field is 0, this must be a Null terminated string. Otherwise, length is set according to ssid_len. */
            .password = "",             /**< Password of ESP32 soft-AP. */
            .ssid_len = 0,              /**< Optional length of SSID field. */
            .channel = SOFTAP_CHANNEL,  /**< Channel of ESP32 soft-AP */
            .authmode = WIFI_AUTH_OPEN, /**< Auth mode of ESP32 soft-AP. Do not support AUTH_WEP in soft-AP mode */
            .ssid_hidden = 1,           /**< Broadcast SSID or not, default 0, broadcast the SSID */
            .max_connection = 0,        /**< Max number of stations allowed to connect in, default 4, max 10 */
            .beacon_interval = 60000,   /**< Beacon interval which should be multiples of 100. Unit: TU(time unit, 1 TU = 1024 us). Range: 100 ~ 60000. Default value: 100 */
            .pairwise_cipher = WIFI_CIPHER_TYPE_NONE, /**< pairwise cipher of SoftAP, group cipher will be derived using this. cipher values are valid starting from WIFI_CIPHER_TYPE_TKIP, enum values before that will be considered as invalid and default cipher suites(TKIP+CCMP) will be used. Valid cipher suites in softAP mode are WIFI_CIPHER_TYPE_TKIP, WIFI_CIPHER_TYPE_CCMP and WIFI_CIPHER_TYPE_TKIP_CCMP. */
            .ftm_responder = 0,         /**< Enable FTM Responder mode */
        }
    };
    if(ESP_OK != (err = esp_wifi_set_config(ESP_IF_WIFI_AP, &config)))
    {
        ESP_LOGD("ESPNOW", "Couldn't set softap config");
        return;
    }

    if(ESP_OK != (err = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N)))
    {
        ESP_LOGD("ESPNOW", "Couldn't set protocol %s", esp_err_to_name(err));
        return;
    }

    wifi_country_t usa =
    {
        .cc = "USA",
        .schan = 1,
        .nchan = 11,
        .max_tx_power = 84,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    if(ESP_OK != (err = esp_wifi_set_country(&usa)))
    {
        ESP_LOGD("ESPNOW", "Couldn't set country");
        return;
    }

    if(ESP_OK != (err = esp_wifi_config_espnow_rate(ESP_IF_WIFI_AP, WIFI_PHY_RATE_54M)))
    {
        ESP_LOGD("ESPNOW", "Couldn't set PHY rate %s", esp_err_to_name(err));
        return;
    }

    if(ESP_OK != (err = esp_wifi_start()))
    {
        ESP_LOGD("ESPNOW", "Couldn't start wifi %s", esp_err_to_name(err));
        return;
    }

    // Set the channel
    if(ESP_OK != (err = esp_wifi_set_channel( SOFTAP_CHANNEL, WIFI_SECOND_CHAN_BELOW )))
    {
        ESP_LOGD("ESPNOW", "Couldn't set channel");
        return;
    }

    if(ESP_OK == (err = esp_now_init()))
    {
        ESP_LOGD("ESPNOW", "ESP NOW init!");

        if(ESP_OK != (err = esp_now_register_recv_cb(espNowRecvCb)))
        {
            ESP_LOGD("ESPNOW", "recvCb NOT registered");
        }

        if(ESP_OK != (err = esp_now_register_send_cb(espNowSendCb)))
        {
            ESP_LOGD("ESPNOW", "sendCb NOT registered");
        }

        esp_now_peer_info_t broadcastPeer =
        {
            .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
            .lmk = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            .channel = SOFTAP_CHANNEL,
            .ifidx = ESP_IF_WIFI_AP,
            .encrypt = 0,
            .priv = NULL
        };
        if(ESP_OK != (err = esp_now_add_peer(&broadcastPeer)))
        {
            ESP_LOGD("ESPNOW", "peer NOT added");
        }
    }
    else
    {
        ESP_LOGD("ESPNOW", "esp now fail (%s)", esp_err_to_name(err));
    }
}

/**
 * This callback function is called whenever an ESP-NOW packet is received
 *
 * @param mac_addr The MAC address of the sender
 * @param data     The data which was received
 * @param len      The length of the data which was received
 */
void espNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
    // Negative index to get the ESP NOW header
    // espNowHeader_t * hdr = (espNowHeader_t *)&data[-sizeof(espNowHeader_t)];

    // Negative index further to get the WIFI header
    wifi_pkt_rx_ctrl_t* pkt = (wifi_pkt_rx_ctrl_t*)&data[-sizeof(espNowHeader_t) - sizeof(wifi_pkt_rx_ctrl_t)];

    /* The receiving callback function also runs from the Wi-Fi task. So, do not
     * do lengthy operations in the callback function. Instead, post the
     * necessary data to a queue and handle it from a lower priority task.
     */
    p2pPacket_t packet;

    // Copy the MAC
    memcpy(&packet.mac, mac_addr, sizeof(uint8_t) * 6);

    // Make sure the data fits, then copy it
    if(data_len > sizeof(packet.data))
    {
        data_len = sizeof(packet.data);
    }
    packet.len = data_len;
    memcpy(&packet.data, data, data_len);

    // Copy the RSSI
    packet.rssi = pkt->rssi;

    // Queue this packet
    xQueueSendFromISR(esp_now_queue, &packet, NULL);
}

/**
 * Check the ESP NOW receive queue. If there are any received packets, send
 * them to hostEspNowRecvCb()
 */
void checkEspNowRxQueue(void)
{
    p2pPacket_t packet;
    if (xQueueReceive(esp_now_queue, &packet, 0))
    {
        // Debug print the received payload
        // char dbg[256] = {0};
        // char tmp[8] = {0};
        // int i;
        // for (i = 0; i < packet.len; i++)
        // {
        //     sprintf(tmp, "%02X ", packet.data[i]);
        //     strcat(dbg, tmp);
        // }
        // ESP_LOGD("ESPNOW", "%s, MAC [%02X:%02X:%02X:%02X:%02X:%02X], Bytes [%s]",
        //        __func__,
        //        packet.mac[0],
        //        packet.mac[1],
        //        packet.mac[2],
        //        packet.mac[3],
        //        packet.mac[4],
        //        packet.mac[5],
        //        dbg);

        hostEspNowRecvCb(packet.mac, packet.data, packet.len, packet.rssi);
    }
}

/**
 * This is a wrapper for esp_now_send. It also sets the wifi power with
 * wifi_set_user_fixed_rate()
 *
 * @param data The data to broadcast using ESP NOW
 * @param len  The length of the data to broadcast
 */
void espNowSend(const uint8_t* data, uint8_t len)
{
    // Send a packet
    esp_now_send((uint8_t*)espNowBroadcastMac, (uint8_t*)data, len);
}

/**
 * This callback function is registered to be called after an ESP NOW
 * transmission occurs. It notifies the program if the transmission
 * was successful or not. It gives no information about if the transmission
 * was received
 *
 * @param mac_addr The MAC address which was transmitted to
 * @param status   MT_TX_STATUS_OK or MT_TX_STATUS_FAILED
 */
void espNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    ESP_LOGD("ESPNOW", "SEND MAC %02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0],
           mac_addr[1],
           mac_addr[2],
           mac_addr[3],
           mac_addr[4],
           mac_addr[5]);

    switch(status)
    {
        case ESP_NOW_SEND_SUCCESS:
        {
            // ESP_LOGD("ESPNOW", "ESP NOW MT_TX_STATUS_OK");
            break;
        }
        default:
        case ESP_NOW_SEND_FAIL:
        {
            ESP_LOGD("ESPNOW", "ESP NOW MT_TX_STATUS_FAILED");
            break;
        }
    }

    hostEspNowSendCb(mac_addr, status);
}

/**
 * This function is automatically called to de-initialize ESP-NOW
 */
void espNowDeinit(void)
{
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
}
