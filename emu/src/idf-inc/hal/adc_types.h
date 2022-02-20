#ifndef _ADC_TYPES_H_
#define _ADC_TYPES_H_

/**
 * @brief ADC channels handle. See ``adc1_channel_t``, ``adc2_channel_t``.
 *
 * @note  For ESP32 ADC1, don't use `ADC_CHANNEL_8`, `ADC_CHANNEL_9`. See ``adc1_channel_t``.
 */
typedef enum {
    ADC_CHANNEL_0 = 0, /*!< ADC channel */
    ADC_CHANNEL_1,     /*!< ADC channel */
    ADC_CHANNEL_2,     /*!< ADC channel */
    ADC_CHANNEL_3,     /*!< ADC channel */
    ADC_CHANNEL_4,     /*!< ADC channel */
    ADC_CHANNEL_5,     /*!< ADC channel */
    ADC_CHANNEL_6,     /*!< ADC channel */
    ADC_CHANNEL_7,     /*!< ADC channel */
    ADC_CHANNEL_8,     /*!< ADC channel */
    ADC_CHANNEL_9,     /*!< ADC channel */
    ADC_CHANNEL_MAX,
} adc_channel_t;

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
/**
 * @brief ADC digital controller (DMA mode) output data format.
 *        Used to analyze the acquired ADC (DMA) data.
 * @note  ESP32: Only `type1` is valid. ADC2 does not support DMA mode.
 * @note  ESP32-S2: Member `channel` can be used to judge the validity of the ADC data,
 *                  because the role of the arbiter may get invalid ADC data.
 */
typedef struct {
    union {
        struct {
            uint16_t data:     12;  /*!<ADC real output data info. Resolution: 12 bit. */
            uint16_t channel:   4;  /*!<ADC channel index info. */
        } type1;
        struct {
            uint16_t data:     11;  /*!<ADC real output data info. Resolution: 11 bit. */
            uint16_t channel:   4;  /*!<ADC channel index info. For ESP32-S2:
                                        If (channel < ADC_CHANNEL_MAX), The data is valid.
                                        If (channel > ADC_CHANNEL_MAX), The data is invalid. */
            uint16_t unit:      1;  /*!<ADC unit index info. 0: ADC1; 1: ADC2.  */
        } type2;                    /*!<When the configured output format is 11bit. `ADC_DIGI_FORMAT_11BIT` */
        uint16_t val;               /*!<Raw data value */
    };
} adc_digi_output_data_t;

#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
/**
 * @brief ADC digital controller (DMA mode) output data format.
 *        Used to analyze the acquired ADC (DMA) data.
 */
typedef struct {
    union {
        struct {
            uint32_t data:          12; /*!<ADC real output data info. Resolution: 12 bit. */
            uint32_t reserved12:    1;  /*!<Reserved12. */
            uint32_t channel:       3;  /*!<ADC channel index info.
                                            If (channel < ADC_CHANNEL_MAX), The data is valid.
                                            If (channel > ADC_CHANNEL_MAX), The data is invalid. */
            uint32_t unit:          1;  /*!<ADC unit index info. 0: ADC1; 1: ADC2.  */
            uint32_t reserved17_31: 15; /*!<Reserved17. */
        } type2;                         /*!<When the configured output format is 12bit. `ADC_DIGI_FORMAT_11BIT` */
        uint32_t val;                   /*!<Raw data value */
    };
} adc_digi_output_data_t;

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
/**
 * @brief ADC digital controller (DMA mode) output data format.
 *        Used to analyze the acquired ADC (DMA) data.
 */
typedef struct {
    union {
        struct {
            uint32_t data:          13; /*!<ADC real output data info. Resolution: 13 bit. */
            uint32_t channel:       4;  /*!<ADC channel index info.
                                            If (channel < ADC_CHANNEL_MAX), The data is valid.
                                            If (channel > ADC_CHANNEL_MAX), The data is invalid. */
            uint32_t unit:          1;  /*!<ADC unit index info. 0: ADC1; 1: ADC2.  */
            uint32_t reserved17_31: 14; /*!<Reserved17. */
        } type2;                         /*!<When the configured output format is 12bit. `ADC_DIGI_FORMAT_11BIT` */
        uint32_t val;                   /*!<Raw data value */
    };
} adc_digi_output_data_t;
#endif

#endif