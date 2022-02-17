#ifndef _ADC_H_
#define _ADC_H_

#if defined(CONFIG_IDF_TARGET_ESP32)
/**** `adc1_channel_t` will be deprecated functions, combine into `adc_channel_t` ********/
typedef enum {
    ADC1_CHANNEL_0 = 0, /*!< ADC1 channel 0 is GPIO36 */
    ADC1_CHANNEL_1,     /*!< ADC1 channel 1 is GPIO37 */
    ADC1_CHANNEL_2,     /*!< ADC1 channel 2 is GPIO38 */
    ADC1_CHANNEL_3,     /*!< ADC1 channel 3 is GPIO39 */
    ADC1_CHANNEL_4,     /*!< ADC1 channel 4 is GPIO32 */
    ADC1_CHANNEL_5,     /*!< ADC1 channel 5 is GPIO33 */
    ADC1_CHANNEL_6,     /*!< ADC1 channel 6 is GPIO34 */
    ADC1_CHANNEL_7,     /*!< ADC1 channel 7 is GPIO35 */
    ADC1_CHANNEL_MAX,
} adc1_channel_t;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) // TODO ESP32-S3 channels are wrong IDF-1776
/**** `adc1_channel_t` will be deprecated functions, combine into `adc_channel_t` ********/
typedef enum {
    ADC1_CHANNEL_0 = 0, /*!< ADC1 channel 0 is GPIO1  */
    ADC1_CHANNEL_1,     /*!< ADC1 channel 1 is GPIO2  */
    ADC1_CHANNEL_2,     /*!< ADC1 channel 2 is GPIO3  */
    ADC1_CHANNEL_3,     /*!< ADC1 channel 3 is GPIO4  */
    ADC1_CHANNEL_4,     /*!< ADC1 channel 4 is GPIO5  */
    ADC1_CHANNEL_5,     /*!< ADC1 channel 5 is GPIO6  */
    ADC1_CHANNEL_6,     /*!< ADC1 channel 6 is GPIO7  */
    ADC1_CHANNEL_7,     /*!< ADC1 channel 7 is GPIO8  */
    ADC1_CHANNEL_8,     /*!< ADC1 channel 8 is GPIO9  */
    ADC1_CHANNEL_9,     /*!< ADC1 channel 9 is GPIO10 */
    ADC1_CHANNEL_MAX,
} adc1_channel_t;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
/**** `adc1_channel_t` will be deprecated functions, combine into `adc_channel_t` ********/
typedef enum {
    ADC1_CHANNEL_0 = 0, /*!< ADC1 channel 0 is GPIO0 */
    ADC1_CHANNEL_1,     /*!< ADC1 channel 1 is GPIO1 */
    ADC1_CHANNEL_2,     /*!< ADC1 channel 2 is GPIO2 */
    ADC1_CHANNEL_3,     /*!< ADC1 channel 3 is GPIO3 */
    ADC1_CHANNEL_4,     /*!< ADC1 channel 4 is GPIO4 */
    ADC1_CHANNEL_MAX,
} adc1_channel_t;
#endif // defined(CONFIG_IDF_TARGET_*)

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) // TODO ESP32-S3 channels are wrong IDF-1776
/**** `adc2_channel_t` will be deprecated functions, combine into `adc_channel_t` ********/
typedef enum {
    ADC2_CHANNEL_0 = 0, /*!< ADC2 channel 0 is GPIO4  (ESP32), GPIO11 (ESP32-S2) */
    ADC2_CHANNEL_1,     /*!< ADC2 channel 1 is GPIO0  (ESP32), GPIO12 (ESP32-S2) */
    ADC2_CHANNEL_2,     /*!< ADC2 channel 2 is GPIO2  (ESP32), GPIO13 (ESP32-S2) */
    ADC2_CHANNEL_3,     /*!< ADC2 channel 3 is GPIO15 (ESP32), GPIO14 (ESP32-S2) */
    ADC2_CHANNEL_4,     /*!< ADC2 channel 4 is GPIO13 (ESP32), GPIO15 (ESP32-S2) */
    ADC2_CHANNEL_5,     /*!< ADC2 channel 5 is GPIO12 (ESP32), GPIO16 (ESP32-S2) */
    ADC2_CHANNEL_6,     /*!< ADC2 channel 6 is GPIO14 (ESP32), GPIO17 (ESP32-S2) */
    ADC2_CHANNEL_7,     /*!< ADC2 channel 7 is GPIO27 (ESP32), GPIO18 (ESP32-S2) */
    ADC2_CHANNEL_8,     /*!< ADC2 channel 8 is GPIO25 (ESP32), GPIO19 (ESP32-S2) */
    ADC2_CHANNEL_9,     /*!< ADC2 channel 9 is GPIO26 (ESP32), GPIO20 (ESP32-S2) */
    ADC2_CHANNEL_MAX,
} adc2_channel_t;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
/**** `adc2_channel_t` will be deprecated functions, combine into `adc_channel_t` ********/
typedef enum {
    ADC2_CHANNEL_0 = 0, /*!< ADC2 channel 0 is GPIO5 */
    ADC2_CHANNEL_MAX,
} adc2_channel_t;
#endif

#endif