/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/adc.h"

#include "hdw-mic.h"

#define TIMES              256
#define GET_UNIT(x)        ((x>>3) & 0x1)

#if CONFIG_IDF_TARGET_ESP32
    #define ADC_RESULT_BYTE     2
    #define ADC_CONV_LIMIT_EN   1                       //For ESP32, this should always be set to 1
#elif CONFIG_IDF_TARGET_ESP32S2
    #define ADC_RESULT_BYTE     2
    #define ADC_CONV_LIMIT_EN   0
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
    #define ADC_RESULT_BYTE     4
    #define ADC_CONV_LIMIT_EN   0
#elif CONFIG_IDF_TARGET_ESP32S3
    #define ADC_RESULT_BYTE     4
    #define ADC_CONV_LIMIT_EN   0
#endif

adc_digi_convert_mode_t convMode;
adc_digi_output_format_t outputFormat;

// #if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32H2
//     static uint16_t adc1_chan_mask = BIT(2) | BIT(3);
//     static uint16_t adc2_chan_mask = BIT(0);
//     static adc_channel_t channel[3] = {ADC1_CHANNEL_2, ADC1_CHANNEL_3, (ADC2_CHANNEL_0 | 1 << 3)};
// #elif CONFIG_IDF_TARGET_ESP32S2
//     static uint16_t adc1_chan_mask = BIT(2) | BIT(3);
//     static uint16_t adc2_chan_mask = BIT(0);
//     static adc_channel_t channel[3] = {ADC1_CHANNEL_2, ADC1_CHANNEL_3, (ADC2_CHANNEL_0 | 1 << 3)};
// #elif CONFIG_IDF_TARGET_ESP32
//     static uint16_t adc1_chan_mask = BIT(7);
//     static uint16_t adc2_chan_mask = 0;
//     static adc_channel_t channel[1] = {ADC1_CHANNEL_7};
// #endif

static const char *TAG = "ADC DMA";

/**
 * @brief TODO
 * 
 * TODO call adc_digi_deinitialize(); too somewhere
 *
 * @param adc1_chan_mask A bit set for each ADC1 channel to start sampling
 * @param adc2_chan_mask A bit set for each ADC1 channel to start sampling
 * @param channel        An array of adc_channel_t to start sampling
 * @param channel_num    The number of adc_channel_t to start sampling
 */
void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask,
                         adc_channel_t *channel, uint8_t channel_num)
{
    adc_digi_init_config_t adc_dma_config =
    {
        .max_store_buf_size = 1024,
        .conv_num_each_intr = TIMES,
        .adc1_chan_mask = adc1_chan_mask,
        .adc2_chan_mask = adc2_chan_mask,
    };
    ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));

    if(adc1_chan_mask && adc2_chan_mask)
    {
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
        convMode = ADC_CONV_ALTER_UNIT;
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        convMode = ADC_CONV_BOTH_UNIT;
#elif CONFIG_IDF_TARGET_ESP32
        // SP32 only supports ADC1 DMA mode
        return;
#endif
        outputFormat = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    }
    else if(adc1_chan_mask)
    {
        convMode = ADC_CONV_SINGLE_UNIT_1;
        outputFormat = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    }
    else if(adc2_chan_mask)
    {
        convMode = ADC_CONV_SINGLE_UNIT_2;
        outputFormat = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    }
    else
    {
        return;
    }

    adc_digi_configuration_t dig_cfg =
    {
        .conv_limit_en = ADC_CONV_LIMIT_EN,
        .conv_limit_num = 250,
        .sample_freq_hz = 8000,
        .conv_mode = convMode,
        .format = outputFormat,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++)
    {
        uint8_t unit = GET_UNIT(channel[i]);
        uint8_t ch = channel[i] & 0x7;
        adc_pattern[i].atten = ADC_ATTEN_DB_0;
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
    adc_digi_start(); // TODO call adc_digi_stop(); or break out into separate funcs
}

#if !CONFIG_IDF_TARGET_ESP32
static bool check_valid_data(const adc_digi_output_data_t *data)
{
    if(outputFormat == ADC_DIGI_OUTPUT_FORMAT_TYPE1)
    {
        return true;
    }

    const unsigned int unit = data->type2.unit;
    if (unit > 2)
    {
        return false;
    }
    if (data->type2.channel >= SOC_ADC_CHANNEL_NUM(unit))
    {
        return false;
    }
    return true;
}
#endif

/**
 * @brief TODO
 *
 */
void mic_main(void)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[TIMES] = {0};
    memset(result, 0xcc, TIMES);

    ret = adc_digi_read_bytes(result, TIMES, &ret_num, ADC_MAX_DELAY);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)
    {
        if (ret == ESP_ERR_INVALID_STATE)
        {
            /**
             * @note 1
             * Issue:
             * As an example, we simply print the result out, which is super slow. Therefore the conversion is too
             * fast for the task to handle. In this condition, some conversion results lost.
             *
             * Reason:
             * When this error occurs, you will usually see the task watchdog timeout issue also.
             * Because the conversion is too fast, whereas the task calling `adc_digi_read_bytes` is slow.
             * So `adc_digi_read_bytes` will hardly block. Therefore Idle Task hardly has chance to run. In this
             * example, we add a `vTaskDelay(1)` below, to prevent the task watchdog timeout.
             *
             * Solution:
             * Either decrease the conversion speed, or increase the frequency you call `adc_digi_read_bytes`
             */
        }

        ESP_LOGI("TASK:", "ret is %x, ret_num is %d", ret, ret_num);
        for (int i = 0; i < ret_num; i += ADC_RESULT_BYTE)
        {
            // TODO deliver samples to swadge mode
            adc_digi_output_data_t *p = (void*)&result[i];
// #if CONFIG_IDF_TARGET_ESP32
//             ESP_LOGI(TAG, "Unit: %d, Channel: %d, Value: %x", 1, p->type1.channel, p->type1.data);
// #else
//             if (convMode == ADC_CONV_BOTH_UNIT || convMode == ADC_CONV_ALTER_UNIT)
//             {
//                 if (check_valid_data(p))
//                 {
//                     ESP_LOGI(TAG, "Unit: %d,_Channel: %d, Value: %x", p->type2.unit+1, p->type2.channel, p->type2.data);
//                 }
//                 else
//                 {
//                     // abort();
//                     ESP_LOGI(TAG, "Invalid data [%d_%d_%x]", p->type2.unit+1, p->type2.channel, p->type2.data);
//                 }
//             }
//             else if (convMode == ADC_CONV_SINGLE_UNIT_2)
//             {
//                 ESP_LOGI(TAG, "Unit: %d, Channel: %d, Value: %x", 2, p->type1.channel, p->type1.data);
//             }
//             else if (convMode == ADC_CONV_SINGLE_UNIT_1)
//             {
//                 ESP_LOGI(TAG, "Unit: %d, Channel: %d, Value: %x", 1, p->type1.channel, p->type1.data);
//             }
// #endif
        }
        //See `note 1`
        // vTaskDelay(1);
    }
    else if (ret == ESP_ERR_TIMEOUT)
    {
        /**
         * ``ESP_ERR_TIMEOUT``: If ADC conversion is not finished until Timeout, you'll get this return error.
         * Here we set Timeout ``portMAX_DELAY``, so you'll never reach this branch.
         */
        ESP_LOGW(TAG, "No data, increase timeout or reduce conv_num_each_intr");
        // vTaskDelay(1000);
    }
}
