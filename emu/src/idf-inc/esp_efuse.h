#ifndef _ESP_EFUSE_H_
#define _ESP_EFUSE_H_

/**
 * @brief Type definition for ROM log scheme
 */
typedef enum {
    ESP_EFUSE_ROM_LOG_ALWAYS_ON,    /**< Always enable ROM logging */
    ESP_EFUSE_ROM_LOG_ON_GPIO_LOW,  /**< ROM logging is enabled when specific GPIO level is low during start up */
    ESP_EFUSE_ROM_LOG_ON_GPIO_HIGH, /**< ROM logging is enabled when specific GPIO level is high during start up */
    ESP_EFUSE_ROM_LOG_ALWAYS_OFF    /**< Disable ROM logging permanently */
} esp_efuse_rom_log_scheme_t;

#endif