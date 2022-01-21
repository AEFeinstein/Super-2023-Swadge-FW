#include "rmt.h"
#include "touch_pad.h"
#include "esp_efuse.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gpio_types.h"
#include "spi_types.h"
#include "sdkconfig.h"
#include "rmt_reg.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"

/**
 * @brief TODO
 * 
 * @param log_scheme 
 * @return esp_err_t 
 */
esp_err_t esp_efuse_set_rom_log_scheme(esp_efuse_rom_log_scheme_t log_scheme)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return ESP_FAIL;
}

/**
 * @brief TODO
 * 
 * @return int64_t 
 */
int64_t esp_timer_get_time(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return 0;
}

/**
 * @brief TODO
 * 
 * @return esp_err_t 
 */
esp_err_t esp_timer_init(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return ESP_FAIL;
}

/**
 * @brief TODO
 * 
 * @param config 
 * @return esp_err_t 
 */
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return ESP_FAIL;
}

/**
 * @brief TODO
 * 
 * @param report 
 */
void tud_gamepad_report(hid_gamepad_report_t * report)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool tud_ready(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

/**
 * @brief TODO
 * 
 */
void taskYIELD(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param pvTaskCode 
 * @param pcName 
 * @param usStackDepth 
 * @param pvParameters 
 * @param uxPriority 
 * @param pxCreatedTask 
 * @return BaseType_t 
 */
BaseType_t xTaskCreate( TaskFunction_t pvTaskCode, const char * const pcName,
    const uint32_t usStackDepth, void * const pvParameters, 
    UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return 0;
}
