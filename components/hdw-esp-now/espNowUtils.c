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
#include <esp_private/wifi.h>

#include "espNowUtils.h"
#include "../../main/p2pConnection.h"

//==============================================================================
// Defines
//==============================================================================

#define ESPNOW_CHANNEL 11
#define WIFI_RATE WIFI_PHY_RATE_MCS6_SGI

// Three random bytes used as 'start' bytes for packets over serial
#define FRAMING_START_1 251
#define FRAMING_START_2  63
#define FRAMING_START_3 114

#define ESP_NOW_SERIAL_RX_BUF_SIZE  256
#define ESP_NOW_SERIAL_RINGBUF_SIZE 256

//==============================================================================
// Enums
//==============================================================================

typedef enum {
    EU_PARSING_FR_START_1,
    EU_PARSING_FR_START_2,
    EU_PARSING_FR_START_3,
    EU_PARSING_MAC,
    EU_PARSING_LEN,
    EU_PARSING_PAYLOAD,
} decodeState_t;

//==============================================================================
// Variables
//==============================================================================

/// This is the MAC address to transmit to for broadcasting
const uint8_t espNowBroadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct {
    uint8_t myMac[6];

    hostEspNowRecvCb_t hostEspNowRecvCb;
    hostEspNowSendCb_t hostEspNowSendCb;

    xQueueHandle esp_now_queue;

    bool isSerial;
    gpio_num_t rxGpio;
    gpio_num_t txGpio;
    uint32_t uartNum;
    wifiMode_t mode;

    // A ringbuffer for esp-now serial communication
    char ringBuf[ESP_NOW_SERIAL_RINGBUF_SIZE];
    int16_t rBufHead;
    int16_t rBufTail;
} en = {0};

//==============================================================================
// Prototypes
//==============================================================================

void espNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, int data_len);
void espNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize ESP-NOW and attach callback functions.
 * This uses wifi by default, but espNowUseSerial() may be called later to 
 * communicate over the given UART instead
 * 
 * @param recvCb A callback to call when data is sent
 * @param sendCb A callback to call when data is received
 * @param rx The receive pin when using serial communication instead of wifi
 * @param tx The transmit pin when using serial communication instead of wifi
 * @param uart The UART to use for serial communication
 */
void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb,
    gpio_num_t rx, gpio_num_t tx, uart_port_t uart, wifiMode_t mode)
{
    // Save callback functions
    en.hostEspNowRecvCb = recvCb;
    en.hostEspNowSendCb = sendCb;

    // Save serial config
    en.rxGpio = rx;
    en.txGpio = tx;
    en.uartNum = uart;
    en.mode = mode;

    if (ESP_NOW_IMMEDIATE != mode)
    {
        // Create a queue to move packets from the receive callback to the main task
        en.esp_now_queue = xQueueCreate(10, sizeof(p2pPacket_t));
    }

    esp_err_t err;

    // Initialize wifi
    wifi_init_config_t conf = WIFI_INIT_CONFIG_DEFAULT();
    conf.ampdu_rx_enable = 0;
    conf.ampdu_tx_enable = 0;
    if (ESP_OK != (err = esp_wifi_init(&conf)))
    {
        ESP_LOGW("ESPNOW", "Couldn't init wifi %s", esp_err_to_name(err));
        return;
    }

    // Save our MAC address for serial mode
    esp_wifi_get_mac(WIFI_IF_STA, en.myMac);

    if (ESP_OK != (err = esp_wifi_set_storage(WIFI_STORAGE_RAM)))
    {
        ESP_LOGW("ESPNOW", "Couldn't set wifi storage %s", esp_err_to_name(err));
        return;
    }

    // Set up all the wifi station mode configs
    if(ESP_OK != (err = esp_wifi_set_mode(WIFI_MODE_STA)))
    {
        ESP_LOGW("ESPNOW", "Could not set as station mode");
        return;
    }

    wifi_config_t config =
    {
        .sta =
        {
            .ssid = "",                    /**< SSID of target AP. */
            .password = "",                /**< Password of target AP. */
            .scan_method = WIFI_FAST_SCAN, /**< do all channel scan or fast scan */
            .bssid_set = false,            /**< whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0 = 0, and it needs to be 1 only when users need to check the MAC address of the AP.*/
            .bssid = {0, 0, 0, 0, 0, 0},   /**< MAC address of target AP*/
            .channel = ESPNOW_CHANNEL,     /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/
            .listen_interval = 3,          /**< Listen interval for ESP32 station to receive beacon when WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3 if set to 0. */
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL, /**< sort the connect AP in the list by rssi or security mode */
            .threshold.authmode = WIFI_AUTH_OPEN, /**< When sort_method is set, only APs which have an auth mode that is more secure than the selected auth mode and a signal stronger than the minimum RSSI will be used. */
            .threshold.rssi = 0,
            .pmf_cfg.capable = false,      /**< Configuration for Protected Management Frame. Will be advertized in RSN Capabilities in RSN IE. */
            .pmf_cfg.required = false,
            .rm_enabled = false,           /**< Whether Radio Measurements are enabled for the connection */
            .btm_enabled = false,          /**< Whether BSS Transition Management is enabled for the connection */
            .mbo_enabled = false,          /**< Whether MBO is enabled for the connection */
            .reserved = 0                  /**< Reserved for future feature set */
        },
    };
    if(ESP_OK != (err = esp_wifi_set_config(ESP_IF_WIFI_STA, &config)))
    {
        ESP_LOGW("ESPNOW", "Couldn't set station config");
        return;
    }

    if(ESP_OK != (err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N)))
    {
        ESP_LOGW("ESPNOW", "Couldn't set protocol %s", esp_err_to_name(err));
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

    if(ESP_OK != (err = esp_wifi_config_80211_tx_rate(ESP_IF_WIFI_STA, WIFI_RATE)))
    {
        ESP_LOGW("ESPNOW", "Couldn't set PHY rate %s", esp_err_to_name(err));
        return;
    }

    if(ESP_OK != (err = esp_wifi_start()))
    {
        ESP_LOGW("ESPNOW", "Couldn't start wifi %s", esp_err_to_name(err));
        return;
    }

    if(ESP_OK != (err = esp_wifi_config_espnow_rate(ESP_IF_WIFI_STA, WIFI_RATE)))
    {
        ESP_LOGW("ESPNOW", "Couldn't set PHY rate %s", esp_err_to_name(err));
        return;
    }

    // Set the channel
    if(ESP_OK != (err = esp_wifi_set_channel( ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE )))
    {
        ESP_LOGD("ESPNOW", "Couldn't set channel");
        return;
    }

    // Set data rate
    if(ESP_OK != (err = esp_wifi_internal_set_fix_rate(ESP_IF_WIFI_STA, true, WIFI_RATE)))
    {
        ESP_LOGW("ESPNOW", "Couldn't set data rate");
        return;
    }

    // Don't scan in STA mode
    if(ESP_OK != (err = esp_wifi_scan_stop()))
    {
        ESP_LOGW("ESPNOW", "Couldn't stop scanning");
        return;
    }

    // Commented out but for future consideration.
    //if(ESP_OK != esp_wifi_set_max_tx_power(84)) //78 ~= 19.5dB
    //{
    //    ESP_LOGW("ESPNOW", "Couldn't set max power");
    //    return;
    //}

    // This starts ESP-NOW
    en.isSerial = true;
    espNowUseWireless();
}

/**
 * Start wifi and use it for communication
 */
void espNowUseWireless(void)
{
    if(true == en.isSerial)
    {
        // First make sure the UART isn't being used
        uart_driver_delete(en.uartNum);
        en.isSerial = false;

        // Then start ESP NOW
        esp_err_t err;
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
                .channel = ESPNOW_CHANNEL,
                .ifidx = ESP_IF_WIFI_STA,
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

        //Appears to set gain "offset" like what it reports as gain?  Does not actually impact real gain.
        //But when paired with the second write command, it seems to have the intended impact.
        //This number is in ~1/2dB.  So this accounts for a 10dB muting.
        const int igi_reduction = 20;
        volatile uint32_t * test = (uint32_t*)0x6001c02c; //Should be the "RSSI Offset" but seems to do more.
        *test = (*test & 0xffffff00) + igi_reduction; 

        // Make sure the first takes effect.
        vTaskDelay( 0 );

        //No idea  Somehow applies setting of 0x6001c02c  (Ok, actually I don't know what the right-most value should be but 0xff in the MSB seems to work?
        test = (uint32_t*)0x6001c0a0;
        *test = (*test & 0xffffff) | 0xff00000000; 
    }
}

/**
 * Start the UART and use it for communication
 * 
 * @param crossoverPins true to crossover the rx and tx pins, false to use them
 *                      as normal.
 */
void espNowUseSerial(bool crossoverPins)
{
    if(false == en.isSerial)
    {
        // First make sure wireless isn't being used
        esp_now_unregister_recv_cb();
        esp_now_unregister_send_cb();
        esp_now_deinit();

        en.isSerial = true;

        // Initialize UART
        uart_config_t uart_config = {
            .baud_rate = 8 * 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_APB,
        };
        ESP_ERROR_CHECK(uart_driver_install(en.uartNum, ESP_NOW_SERIAL_RX_BUF_SIZE, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(en.uartNum, &uart_config));
    }

    if(crossoverPins)
    {
        ESP_ERROR_CHECK(uart_set_pin(en.uartNum, en.txGpio, en.rxGpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    else
    {
        ESP_ERROR_CHECK(uart_set_pin(en.uartNum, en.rxGpio, en.txGpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
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

    if (ESP_NOW_IMMEDIATE == en.mode)
    {
        en.hostEspNowRecvCb(mac_addr, (const char *)data, data_len, pkt->rssi);
    }
    else
    {
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
        xQueueSendFromISR(en.esp_now_queue, &packet, NULL);
    }
}

/**
 * Check the ESP NOW receive queue. If there are any received packets, send
 * them to hostEspNowRecvCb()
 */
void checkEspNowRxQueue(void)
{
    if(en.isSerial)
    {
        // Read bytes from the UART
        char bytesRead[256];
        int numBytesRead = uart_read_bytes(en.uartNum, &bytesRead, sizeof(bytesRead), 0);
        
        // Insert them into the en.ringBuffer
        for(uint16_t i = 0; i < numBytesRead; i++)
        {
            en.ringBuf[en.rBufTail] = bytesRead[i];
            en.rBufTail = (en.rBufTail + 1) % sizeof(en.ringBuf);
        }

        decodeState_t decodeState = EU_PARSING_FR_START_1;
        uint8_t rxMac[6];
        uint8_t rxMacIdx = 0;
        uint8_t payloadLen = 0;
        char payload[256];
        uint8_t payloadIdx = 0;

        // Check the en.ringBuffer for framed packets
        int16_t rBufTmpHead = en.rBufHead;
        while(rBufTmpHead != en.rBufTail)
        {
            switch(decodeState)
            {
                case EU_PARSING_FR_START_1:
                {
                    // Check for first framing byte
                    if(FRAMING_START_1 == en.ringBuf[rBufTmpHead])
                    {
                        decodeState = EU_PARSING_FR_START_2;
                    }
                    break;
                }
                case EU_PARSING_FR_START_2:
                {
                    // Check for second framing byte
                    if(FRAMING_START_2 == en.ringBuf[rBufTmpHead])
                    {
                        decodeState = EU_PARSING_FR_START_3;
                    }
                    break;
                }
                case EU_PARSING_FR_START_3:
                {
                    // Check for third framing byte
                    if(FRAMING_START_3 == en.ringBuf[rBufTmpHead])
                    {
                        decodeState = EU_PARSING_MAC;
                    }
                    break;
                }
                case EU_PARSING_MAC:
                {
                    // Save the MAC byte
                    rxMac[rxMacIdx++] = en.ringBuf[rBufTmpHead];
                    // If all MAC bytes have been read
                    if(6 == rxMacIdx)
                    {
                        decodeState = EU_PARSING_LEN;
                    }
                    break;
                }
                case EU_PARSING_LEN:
                {
                    // Save the length byte
                    payloadLen = en.ringBuf[rBufTmpHead];
                    // Immediately move to EU_PARSING_PAYLOAD
                    decodeState = EU_PARSING_PAYLOAD;
                    break;
                }
                case EU_PARSING_PAYLOAD:
                {
                    // Save the payload byte
                    payload[payloadIdx++] = en.ringBuf[rBufTmpHead];
                    // If all payload bytes have been read
                    if(payloadIdx == payloadLen)
                    {
                        en.hostEspNowRecvCb(rxMac, payload, payloadLen, 0);

                        // Move the en.ringBuf head, reset variables
                        en.rBufHead = rBufTmpHead;
                        rxMacIdx = 0;
                        payloadLen = 0;
                        payloadIdx = 0;
                        decodeState = EU_PARSING_FR_START_1;
                    }
                    break;
                }
            }
            rBufTmpHead = (rBufTmpHead + 1) % sizeof(en.ringBuf);
        }
    }
    else if (en.mode != ESP_NOW_IMMEDIATE)
    {
        p2pPacket_t packet;
        if (xQueueReceive(en.esp_now_queue, &packet, 0))
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

            en.hostEspNowRecvCb(packet.mac, (const char*)(&packet.data), packet.len, packet.rssi);
        }
    }
}

/**
 * This is a wrapper for esp_now_send. It also sets the wifi power with
 * wifi_set_user_fixed_rate()
 *
 * @param data The data to broadcast using ESP NOW
 * @param len  The length of the data to broadcast
 */
void espNowSend(const char* data, uint8_t len)
{
    if(en.isSerial)
    {
        // Frame the packet and add our MAC address
        uint16_t framedPacketLen = len + 4 + sizeof(en.myMac);
        char framedPacket[framedPacketLen];
        uint16_t framedPacketIdx = 0;
        framedPacket[framedPacketIdx++] = FRAMING_START_1;
        framedPacket[framedPacketIdx++] = FRAMING_START_2;
        framedPacket[framedPacketIdx++] = FRAMING_START_3;

        memcpy(&framedPacket[framedPacketIdx], en.myMac, sizeof(en.myMac));
        framedPacketIdx += sizeof(en.myMac);

        framedPacket[framedPacketIdx++] = len;

        memcpy(&framedPacket[framedPacketIdx], data, len);
        framedPacketIdx += len;

        // Send the bytes over serial
        if(framedPacketLen == uart_write_bytes(en.uartNum, framedPacket, framedPacketLen))
        {
            // Manually call the callback
            espNowSendCb(espNowBroadcastMac, ESP_NOW_SEND_SUCCESS);
        }
        else
        {
            espNowSendCb(espNowBroadcastMac, ESP_NOW_SEND_FAIL);
        }
    }
    else
    {
        // Send a packet
        esp_now_send((uint8_t*)espNowBroadcastMac, (uint8_t*)data, len);
    }
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
    ESP_LOGD("ESPNOW", "%s, MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             status == ESP_NOW_SEND_SUCCESS ? "ESP_NOW_SEND_SUCCESS" : "ESP_NOW_SEND_FAIL",
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

    en.hostEspNowSendCb(mac_addr, status);
}

/**
 * This function is automatically called to de-initialize ESP-NOW
 */
void espNowDeinit(void)
{
    if(en.isSerial)
    {
        uart_driver_delete(en.uartNum);
    }
    else
    {
        esp_now_unregister_recv_cb();
        esp_now_unregister_send_cb();
        esp_now_del_peer( espNowBroadcastMac );
        esp_now_deinit();
        esp_wifi_stop();
        esp_wifi_deinit();
    }
}
