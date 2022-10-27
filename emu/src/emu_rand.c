#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "esp_random.h"
#include "bootloader_random.h"
#include "esp_log.h"

#include "emu_esp.h"

static bool seeded = false;

/**
 * @brief  Get one random 32-bit word from hardware RNG
 *
 * If Wi-Fi or Bluetooth are enabled, this function returns true random numbers. In other
 * situations, if true random numbers are required then consult the ESP-IDF Programming
 * Guide "Random Number Generation" section for necessary prerequisites.
 *
 * This function automatically busy-waits to ensure enough external entropy has been
 * introduced into the hardware RNG state, before returning a new random number. This delay
 * is very short (always less than 100 CPU cycles).
 *
 * @return Random value between 0 and UINT32_MAX
 */
uint32_t esp_random(void)
{
    if (!seeded)
    {
        pid_t pid = getpid();
        seeded = true;
        srand(time(NULL) ^ pid);
    }
    return rand();
}

/**
 * @brief Fill a buffer with random bytes from hardware RNG
 *
 * @note This function is implemented via calls to esp_random(), so the same
 * constraints apply.
 *
 * @param buf Pointer to buffer to fill with random numbers.
 * @param len Length of buffer in bytes
 */
void esp_fill_random(void *buf, size_t len)
{
    if (!seeded)
    {
        pid_t pid = getpid();
        seeded = true;
        srand(time(NULL) ^ pid);
    }

    uint8_t * buf8 = (uint8_t*)buf;
    for(size_t i = 0; i < len; i++)
    {
        buf8[i] = rand();
    }
}

/**
 * @brief Enable an entropy source for RNG if RF is disabled
 *
 * The exact internal entropy source mechanism depends on the chip in use but
 * all SoCs use the SAR ADC to continuously mix random bits (an internal
 * noise reading) into the HWRNG. Consult the SoC Technical Reference
 * Manual for more information.
 *
 * Can also be used from app code early during operation, if true
 * random numbers are required before RF is initialised. Consult
 * ESP-IDF Programming Guide "Random Number Generation" section for
 * details.
 */
void bootloader_random_enable(void)
{
    // Nothing to emulate
    ESP_LOGI("RNG", "%s", __func__);
}

/**
 * @brief Disable entropy source for RNG
 *
 * Disables internal entropy source. Must be called after
 * bootloader_random_enable() and before RF features, ADC, or
 * I2S (ESP32 only) are initialized.
 *
 * Consult the ESP-IDF Programming Guide "Random Number Generation"
 * section for details.
 */
void bootloader_random_disable(void)
{
    // Nothing to emulate
    ESP_LOGI("RNG", "%s", __func__);
}

/**
 * @brief Fill buffer with 'length' random bytes
 *
 * @note If this function is being called from app code only, and never
 * from the bootloader, then it's better to call esp_fill_random().
 *
 * @param buffer Pointer to buffer
 * @param length This many bytes of random data will be copied to buffer
 */
void bootloader_fill_random(void *buffer, size_t length)
{
    esp_fill_random(buffer, length);
}
