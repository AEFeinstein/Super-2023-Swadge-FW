//==============================================================================
// Includes
//==============================================================================

#include "emu_esp.h"
#include "esp_log.h"
#include "esp_wifi.h"

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

/**
  * @brief     Get mac of specified interface
  *
  * @param      ifx  interface
  * @param[out] mac  store mac of the interface ifx
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
  *    - ESP_ERR_INVALID_ARG: invalid argument
  *    - ESP_ERR_WIFI_IF: invalid interface
  */
esp_err_t esp_wifi_get_mac(wifi_interface_t ifx UNUSED, uint8_t mac[6])
{
    // Spoof a MAC
    mac[0] = 0x01;
    mac[1] = 0x23;
    mac[2] = 0x45;
    mac[3] = 0x67;
    mac[4] = 0x89;
    mac[5] = 0xAB;
    return ESP_OK;
}