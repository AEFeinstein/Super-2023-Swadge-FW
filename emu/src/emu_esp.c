//==============================================================================
// Includes
//==============================================================================

#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "emu_esp.h"
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
#include "esp_heap_caps.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_THREADS 10

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
    emu_loop();
	// Just sleep for ten ms
	usleep(1000);
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

/**
 * @brief Allocate a chunk of memory which has the given capabilities. The initialized value in the memory is set to zero.
 *
 * Equivalent semantics to libc calloc(), for capability-aware memory.
 *
 * In IDF, ``calloc(p)`` is equivalent to ``heap_caps_calloc(p, MALLOC_CAP_8BIT)``.
 *
 * @param n    Number of continuing chunks of memory to allocate
 * @param size Size, in bytes, of a chunk of memory to allocate
 * @param caps        Bitwise OR of MALLOC_CAP_* flags indicating the type
 *                    of memory to be returned
 *
 * @return A pointer to the memory allocated on success, NULL on failure
 */
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    return calloc(n, size);
}
