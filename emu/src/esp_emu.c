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

typedef struct
{
    TaskFunction_t pvTaskCode;
    const char * pcName;
    uint32_t usStackDepth;
    void * pvParameters;
    UBaseType_t uxPriority;
} rtosTask_t;

static unsigned long boot_time_in_micros = 0;
static list_t * taskList = NULL;

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
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config)
{
    WARN_UNIMPLEMENTED();
    return ESP_FAIL;
}

/**
 * @brief TODO
 * 
 * @param report 
 */
void tud_gamepad_report(hid_gamepad_report_t * report)
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
    const uint32_t usStackDepth , void * const pvParameters , 
    UBaseType_t uxPriority , TaskHandle_t * const pxCreatedTask)
{
    // Init the list if necessary
    if(NULL == taskList)
    {
        taskList = list_new();
    }

    rtosTask_t * newTask = (rtosTask_t*)malloc(sizeof(rtosTask_t));
    newTask->pvTaskCode = pvTaskCode;
    newTask->pcName = pcName;
    newTask->usStackDepth = usStackDepth;
    newTask->pvParameters = pvParameters;
    newTask->uxPriority = uxPriority;

    *pxCreatedTask = NULL;

    // Make a node for this task, add it to the list
    list_node_t *taskNode = list_node_new(newTask);
    list_rpush(taskList, taskNode);

    return 0;
}
/**
 * @brief TODO create a thread for each task instead???
 * 
 */
void runAllTasks(void)
{
    list_node_t *node;
    list_iterator_t *it = list_iterator_new(taskList, LIST_HEAD);
    while ((node = list_iterator_next(it)))
    {
        ((rtosTask_t *)(node->val))->pvTaskCode(((rtosTask_t *)(node->val))->pvParameters);
    } 
}
