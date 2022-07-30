#include "esp_sleep.h"

/**
 * TODO implement
 * 
 * @brief Enable wakeup by timer
 * @param time_in_us  time before wakeup, in microseconds
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if value is out of range (TBD)
 */
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us)
{
    return ESP_ERR_INVALID_ARG;
}

/**
 * TODO implement
 * 
 * @brief Enter deep sleep with the configured wakeup options
 *
 * This function does not return.
 */
void esp_deep_sleep_start(void)
{
    while(1){;}
}

/**
 * TODO implement
 * 
 * @brief Get the wakeup source which caused wakeup from sleep
 *
 * @return cause of wake up from last sleep (deep sleep or light sleep)
 */
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void)
{
    return ESP_SLEEP_WAKEUP_UNDEFINED;
}