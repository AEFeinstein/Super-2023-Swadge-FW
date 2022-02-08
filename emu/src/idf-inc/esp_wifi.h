#ifndef _ESP_WIFI_H_
#define _ESP_WIFI_H_

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ESP_IF_WIFI_STA = 0,     /**< ESP32 station interface */
    ESP_IF_WIFI_AP,          /**< ESP32 soft-AP interface */
    ESP_IF_ETH,              /**< ESP32 ethernet interface */
    ESP_IF_MAX
} esp_interface_t;

typedef enum {
    WIFI_IF_STA = ESP_IF_WIFI_STA,
    WIFI_IF_AP  = ESP_IF_WIFI_AP,
} wifi_interface_t;

esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]);

#endif