#include "sdkconfig.h"

/* Note: ESP32 don't support temperature sensor */
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3

#include "esp_err.h"
#include "esp_temperature_sensor.h"
#include "driver/temp_sensor.h"

/**
 * @brief Initialize the ESP's onboard temperature sensor
 */
void initTemperatureSensor(void)
{
    // Initialize temperature sensor peripheral
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(temp_sensor_get_config(&temp_sensor));
    // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
    temp_sensor.dac_offset = TSENS_DAC_DEFAULT;
    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());
}

/**
 * @brief Get a temperature reading from the ESP's onboard temperature sensor
 *
 * @return A floating point temperature
 */
float readTemperatureSensor(void)
{
    float tsens_out;
    if(ESP_OK == temp_sensor_read_celsius(&tsens_out))
    {
        return tsens_out;
    }
    return -274; // Impossible! One degree below absolute zero
}

#else

/**
 * @brief Dummy function for ESPs that don't have a temperature sensor
 */
void initTemperatureSensor(void)
{
    return;
}

/**
 * @brief Dummy function for ESPs that don't have a temperature sensor
 *
 * @return always 0
 */
float readTemperatureSensor(void)
{
    return 0;
}

#endif