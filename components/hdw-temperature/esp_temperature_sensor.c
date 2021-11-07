#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_temperature_sensor.h"

/* Note: ESP32 don't support temperature sensor */

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3
#include "driver/temp_sensor.h"

/**
 * @brief TODO
 *
 */
void initTemperatureSensor(void)
{
    // Initialize touch pad peripheral, it will start a timer to run a filter
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor_get_config(&temp_sensor);
    temp_sensor.dac_offset = TSENS_DAC_DEFAULT; // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();
}

/**
 * @brief TODO
 *
 * @return float
 */
float readTemperatureSensor(void)
{
    float tsens_out;
    if(ESP_OK == temp_sensor_read_celsius(&tsens_out))
    {
        return tsens_out;
    }
    return -1;
}

#else

/**
 * @brief TODO
 *
 */
void initTemperatureSensor(void)
{
    return;
}

/**
 * @brief TODO
 *
 * @return float
 */
float readTemperatureSensor(void)
{
    return 0;
}

#endif