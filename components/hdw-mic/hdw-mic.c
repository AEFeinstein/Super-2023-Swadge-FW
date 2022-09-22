//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "hdw-mic.h"

//==============================================================================
// Defines
//==============================================================================

#define GET_UNIT(x) ((x>>3) & 0x1)

//==============================================================================
// Variables
//==============================================================================

adc_digi_convert_mode_t convMode;
adc_digi_output_format_t outputFormat;

static const char* TAG_DMA = "ADC DMA";
static const char* TAG_SNG = "ADC SNG";

//==============================================================================
// Continuous Functions
//==============================================================================

/**
 * @brief Initialize the ADC to continuously sample using DMA
 *
 * NOTE: this only supports sampling a single ADC at the moment
 *
 * @param adc1_chan_mask A bit set for each ADC1 channel to start sampling
 * @param adc2_chan_mask A bit set for each ADC2 channel to start sampling
 * @param channel        An array of adc_channel_t to start sampling
 * @param channel_num    The number of adc_channel_t to start sampling
 */
void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask,
                         adc_channel_t* channel, uint8_t channel_num)
{
    // Set up the ADC DMA
    adc_digi_init_config_t adc_dma_config =
    {
        .max_store_buf_size = BYTES_PER_READ * 2, // Max length of the converted data that driver can store before they are processed.
        .conv_num_each_intr = BYTES_PER_READ,     // Bytes of data that can be converted in 1 interrupt.
        .adc1_chan_mask = adc1_chan_mask, // Channel list of ADC1 to be initialized.
        .adc2_chan_mask = adc2_chan_mask, // Channel list of ADC2 to be initialized.
    };
    ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));

    // Set the conversion mode and output format based on the number of ADCs sampled
    if (adc1_chan_mask && adc2_chan_mask)
    {
        // Sample both ADC units
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
        convMode = ADC_CONV_ALTER_UNIT;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
        convMode = ADC_CONV_BOTH_UNIT;
#elif defined(CONFIG_IDF_TARGET_ESP32)
        // SP32 only supports ADC1 DMA mode
        return;
#endif
        outputFormat = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    }
    else if (adc1_chan_mask)
    {
        // Just unit 1
        convMode = ADC_CONV_SINGLE_UNIT_1;
        outputFormat = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    }
    else if (adc2_chan_mask)
    {
        // Just unit 2
        convMode = ADC_CONV_SINGLE_UNIT_2;
        outputFormat = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    }
    else
    {
        // Invalid
        return;
    }

    // Set up the digital controller
    adc_digi_configuration_t dig_cfg =
    {
#if defined(CONFIG_IDF_TARGET_ESP32)
        .conv_limit_en = 1,    // To limit ADC conversion times. Conversion stops after finishing conv_limit_num times conversion.
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32S3)
        .conv_limit_en = 0,
#endif
        .conv_limit_num = 250, // Set the upper limit of the number of ADC conversion triggers. Range: 1 ~ 255.
        .sample_freq_hz = 8000, // The expected ADC sampling frequency in Hz.
        // Range: 611Hz ~ 83333Hz, Fs = Fd / interval / 2
        // Fs: sampling frequency;
        // Fd: digital controller frequency, no larger than 5M for better performance
        // interval: interval between 2 measurement trigger signal, the smallest interval
        //           should not be smaller than the ADC measurement period, the largest
        //           interval should not be larger than 4095.
        //           See SOC_ADC_SAMPLE_FREQ_THRES_HIGH
        .conv_mode = convMode,  // ADC DMA conversion mode, see adc_digi_convert_mode_t.
        .format = outputFormat, // ADC DMA conversion output format, see adc_digi_output_format_t.
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++)
    {
        adc_pattern[i].atten = ADC_ATTEN_DB_0;      // Attenuation of this ADC channel.
        adc_pattern[i].channel = channel[i] & 0x7;  // ADC channel.
        adc_pattern[i].unit = GET_UNIT(channel[i]); // ADC unit.
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH; // ADC output bit width.

        ESP_LOGI(TAG_DMA, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG_DMA, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        ESP_LOGI(TAG_DMA, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
}

/**
 * @brief Deinitialize the sampling ADC. continuous_adc_read() must be called
 * after this to read samples
 */
void continuous_adc_deinit(void)
{
    continuous_adc_stop();
    adc_digi_deinitialize();
}

/**
 * @brief Start sampling the ADC
 */
void continuous_adc_start(void)
{
    ESP_ERROR_CHECK(adc_digi_start());
}

/**
 * @brief Stop sampling the ADC
 */
void continuous_adc_stop(void)
{
    ESP_ERROR_CHECK(adc_digi_stop());
}

/**
 * @brief Read ADC bytes from DMA for processing
 *
 * NOTE: this only supports sampling a single ADC at the moment
 *
 * @param outSamples A pointer to fill up with ADC samples. Must be at least
 *                   BYTES_PER_READ / sizeof(adc_digi_output_data_t) long
 * @return The number of samples read, which may be 0
 */
uint32_t continuous_adc_read(uint16_t* outSamples)
{
    uint32_t ret_num = 0;
    uint8_t result[BYTES_PER_READ] = {0};
    switch (adc_digi_read_bytes(result, BYTES_PER_READ, &ret_num, 0 /*ADC_MAX_DELAY*/))
    {
        case ESP_OK:
        case ESP_ERR_INVALID_STATE:
        {
            for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t))
            {
                *(outSamples++) = ((adc_digi_output_data_t*)(&result[i]))->type1.data;
            }
            // ESP_LOGI(TAG_DMA, "ADC DMA OK/INVALID: %d", (ret_num / sizeof(adc_digi_output_data_t)));
            return (ret_num / sizeof(adc_digi_output_data_t));
        }
        case ESP_ERR_TIMEOUT:
        default:
        {
            // ESP_LOGI(TAG_DMA, "ADC DMA ESP_ERR_TIMEOUT %d", (ret_num / sizeof(adc_digi_output_data_t)));
            return 0;
        }
    }
}

//==============================================================================
// Oneshot Functions
//==============================================================================

//ADC Attenuation
#define ADC_EXAMPLE_ATTEN           ADC_ATTEN_DB_11

//ADC Calibration
#if CONFIG_IDF_TARGET_ESP32
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif

static esp_adc_cal_characteristics_t adc_chars;
adc_unit_t osAdcUnit;
adc1_channel_t osAdcChan;
bool cali_enable = false;

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
static bool adc_calibration_init(adc_unit_t osAdcUnit)
{
    bool retCali = false;
    esp_err_t ret;

    ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
    if (ret == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG_SNG, "Calibration scheme not supported, skip software calibration");
    }
    else if (ret == ESP_ERR_INVALID_VERSION)
    {
        ESP_LOGW(TAG_SNG, "eFuse not burnt, skip software calibration");
    }
    else if (ret == ESP_OK)
    {
        retCali = true;
        esp_adc_cal_characterize(osAdcUnit, ADC_EXAMPLE_ATTEN, ADC_WIDTH_BIT_DEFAULT, 0, &adc_chars);
    }
    else
    {
        ESP_LOGE(TAG_SNG, "Invalid arg");
    }

    return retCali;
}

/**
 * @brief Initialize a GPIO for oneshot ADC measurements
 * Only one GPIO may be initialized at a time
 * 
 * @param adcUnit The unit for the GPIO
 * @param adcChan The channel for the GPIO
 */
void oneshot_adc_init(adc_unit_t adcUnit, adc1_channel_t adcChan)
{
    // Save the unit and channel
    osAdcUnit = adcUnit;
    osAdcChan = adcChan;

    // Try to calibrate
    cali_enable = adc_calibration_init(osAdcUnit);

    switch(osAdcUnit)
    {
        case ADC_UNIT_1:
        {
            // ADC1 config
            ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
            ESP_ERROR_CHECK(adc1_config_channel_atten(osAdcChan, ADC_EXAMPLE_ATTEN));
            break;
        }
        case ADC_UNIT_2:
        {
            // ADC2 config
            ESP_ERROR_CHECK(adc2_config_channel_atten(osAdcChan, ADC_EXAMPLE_ATTEN));
            break;
        }
        default:
        case ADC_UNIT_BOTH:
        case ADC_UNIT_ALTER:
        case ADC_UNIT_MAX:
        {
            break;
        }
    }
}

/**
 * @brief Read an analog value from the oneshot ADC
 * 
 * @return uint32_t The analog value in mV
 */
uint32_t oneshot_adc_read(void)
{
    esp_err_t ret = ESP_OK;
    uint32_t voltage = 0;
    int adc_raw;

    switch(osAdcUnit)
    {
        case ADC_UNIT_1:
        {
            adc_raw = adc1_get_raw(osAdcChan);
            // ESP_LOGI(TAG_SNG, "raw  data: %d", adc_raw);
            if (cali_enable)
            {
                voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
                // ESP_LOGI(TAG_SNG, "cali data: %d mV", voltage);
            }
            break;
        }
        case ADC_UNIT_2:
        {
            do {
                ret = adc2_get_raw(osAdcChan, ADC_WIDTH_BIT_DEFAULT, &adc_raw);
            } while (ret == ESP_ERR_INVALID_STATE);
            ESP_ERROR_CHECK(ret);

            // ESP_LOGI(TAG_SNG, "raw  data: %d", adc_raw);
            if (cali_enable)
            {
                voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
                // ESP_LOGI(TAG_SNG, "cali data: %d mV", voltage);
            }
            break;
        }
        default:
        case ADC_UNIT_BOTH:
        case ADC_UNIT_ALTER:
        case ADC_UNIT_MAX:
        {
            break;
        }
    }

    return voltage;
}
