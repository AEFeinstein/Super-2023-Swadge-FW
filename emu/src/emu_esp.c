//==============================================================================
// Includes
//==============================================================================

#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "emu_esp.h"

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
#include "esp_heap_caps.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_THREADS 10

//==============================================================================
// Variables
//==============================================================================

uint8_t pthreadIdx = 0;
pthread_t threads[MAX_THREADS];
volatile bool threadsShouldRun = true;

//==============================================================================
// Function prototypes
//==============================================================================

void * runTaskInThread(void * taskFnPtr);

//==============================================================================
// Functions
//==============================================================================

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
 * @brief Initialize the tinyusb driver
 *
 * @param config How to configure the tinyusb driver
 * @return ESP_OK if the driver was installed, or some error if it was not
 */
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config UNUSED)
{
    WARN_UNIMPLEMENTED();
    return ESP_FAIL;
}

/**
 * @brief Send a USB HID gamepad report to the USB host
 *
 * @param report The report to send to the host
 */
void tud_gamepad_report(hid_gamepad_report_t * report UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Check if the USB host is ready to receive a report
 *
 * @return true if the USB host is ready, false if it is not
 */
bool tud_ready(void)
{
    WARN_UNIMPLEMENTED();
    return true;
}

/**
 * @brief Yield to rawdraw
 */
void taskYIELD(void)
{
	// Just sleep for ten ms
	usleep(1000);
}

/**
 * @brief Helper function to call a TaskFunction_t from a pthread
 *
 * @param taskFnPtr The TaskFunction_t to call
 */
void * runTaskInThread(void * taskFnPtr)
{
    // Cast and run!
    ((TaskFunction_t)taskFnPtr)(NULL);
    return NULL;
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
    if(pthreadIdx < MAX_THREADS)
    {
        pthread_create(&threads[pthreadIdx++], NULL, runTaskInThread, pvTaskCode);
    }
    else
    {
        ESP_LOGE("TASK", "Emulator only supports %d tasks!", MAX_THREADS);
    }

    return 0;
}

/**
 * @brief Raise a flag to stop all threads, then join them
 */
void joinThreads(void)
{
    // Tell threads to stop
    threadsShouldRun = false;

    // Wait for threads to actually stop
    for(uint8_t i = 0; i < pthreadIdx; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

/**
 * @brief Allocate a chunk of memory which has the given capabilities
 *
 * Equivalent semantics to libc malloc(), for capability-aware memory.
 *
 * In IDF, ``malloc(p)`` is equivalent to ``heap_caps_malloc(p, MALLOC_CAP_8BIT)``.
 *
 * @param size Size, in bytes, of the amount of memory to allocate
 * @param caps        Bitwise OR of MALLOC_CAP_* flags indicating the type
 *                    of memory to be returned
 *
 * @return A pointer to the memory allocated on success, NULL on failure
 */
void *heap_caps_malloc(size_t size, uint32_t caps __attribute__((unused)))
{
    return malloc(size);
}
