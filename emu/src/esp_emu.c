#include <sys/time.h>
#include <time.h>

#include "list.h"
#include "esp_emu.h"
#include "emu_main.h"

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

static unsigned long boot_time_in_micros = 0;

/**
 * @brief Do nothing for the emulator
 * 
 * @param log_scheme unused
 * @return ESP_OK 
 */
esp_err_t esp_efuse_set_rom_log_scheme(esp_efuse_rom_log_scheme_t log_scheme UNUSED)
{
    return ESP_OK;
}

/**
 * @brief Set the time of 'boot'
 * 
 * @return ESP_OK 
 */
esp_err_t esp_timer_init(void)
{
    struct timespec ts;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
    {
        ESP_LOGE("EMU", "Clock err");
        return 0;
    }
    boot_time_in_micros = (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
    return ESP_OK;
}

/**
 * @brief Get the time since 'boot' in microseconds
 * 
 * @return the time since 'boot' in microseconds 
 */
int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
    {
        ESP_LOGE("EMU", "Clock err");
        return 0;
    }
    return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000)) - boot_time_in_micros;
}

/**
 * @brief TODO
 * 
 * @param config 
 * @return esp_err_t 
 */
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config UNUSED)
{
    WARN_UNIMPLEMENTED();
    return ESP_FAIL;
}

/**
 * @brief TODO
 * 
 * @param report 
 */
void tud_gamepad_report(hid_gamepad_report_t * report UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool tud_ready(void)
{
    WARN_UNIMPLEMENTED();
    return false;
}

/**
 * @brief Yield to rawdraw
 */
void taskYIELD(void)
{
    onTaskYield();
}

/**
 * @brief Run the given task immediately. The emulator only supports one task.
 * 
 * @param pvTaskCode The task to immediately run
 * @param pcName unused
 * @param usStackDepth unused
 * @param pvParameters unused
 * @param uxPriority unused
 * @param pxCreatedTask unused
 * @return 0 for success, 1 for failure
 */
BaseType_t xTaskCreate( TaskFunction_t pvTaskCode, const char * const pcName UNUSED,
    const uint32_t usStackDepth UNUSED, void * const pvParameters UNUSED, 
    UBaseType_t uxPriority UNUSED, TaskHandle_t * const pxCreatedTask UNUSED)
{
    static int tasksCreated = 0;

    tasksCreated++;
    if(tasksCreated > 1)
    {
        ESP_LOGE("TASK", "Emulator only supports one task!");
        return 1;
    }
    pvTaskCode(NULL);
    return 0;
}
