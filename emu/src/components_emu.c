#include "btn.h"
#include "musical_buzzer.h"
#include "espNowUtils.h"
#include "p2pConnection.h"
#include "i2c-conf.h"
#include "led_strip.h"
#include "led_util.h"
#include "nvs_manager.h"
#include "ssd1306.h"
#include "QMA6981.h"
#include "spiffs_manager.h"
#include "esp_temperature_sensor.h"
#include "hdw-tft.h"
#include "touch_sensor.h"

#include "esp_log.h"

//==============================================================================
// Buttons
//==============================================================================

/**
 * @brief TODO
 * 
 * @param numButtons 
 * @param ... 
 */
void initButtons(uint8_t numButtons, ...)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 */
void deinitializeButtons(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool checkButtonQueue(buttonEvt_t*)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

//==============================================================================
// Buzzer
//==============================================================================

/**
 * @brief TODO
 * 
 * @param gpio 
 * @param rmt 
 */
void buzzer_init(gpio_num_t gpio, rmt_channel_t rmt)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param song 
 */
void buzzer_play(const song_t* song)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 */
void buzzer_check_next_note(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 */
void buzzer_stop(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

//==============================================================================
// Touch Sensor
//==============================================================================

/**
 * @brief TODO
 * 
 * @param touchPadSensitivity 
 * @param denoiseEnable 
 * @param numTouchPads 
 * @param ... 
 */
void initTouchSensor(float touchPadSensitivity, bool denoiseEnable,
    uint8_t numTouchPads, ...)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool checkTouchSensor(touch_event_t *)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

//==============================================================================
// I2C
//==============================================================================

/**
 * @brief TODO
 * 
 * @param sda 
 * @param scl 
 * @param pullup 
 * @param clkHz 
 */
void i2c_master_init(gpio_num_t sda, gpio_num_t scl, gpio_pullup_t pullup, uint32_t clkHz)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

//==============================================================================
// LEDs
//==============================================================================

/**
 * @brief TODO
 * 
 * @param gpio 
 * @param rmt 
 * @param numLeds 
 */
void initLeds(gpio_num_t gpio, rmt_channel_t rmt, uint16_t numLeds)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param leds 
 * @param numLeds 
 */
void setLeds(led_t* leds, uint8_t numLeds)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param h 
 * @param s 
 * @param v 
 * @param r 
 * @param g 
 * @param b 
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t* r, uint8_t* g, uint8_t* b)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

//==============================================================================
// NVS
//==============================================================================

/**
 * @brief TODO
 * 
 * @param firstTry 
 * @return true 
 * @return false 
 */
bool initNvs(bool firstTry)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

/**
 * @brief TODO
 * 
 * @param key 
 * @param val 
 * @return true 
 * @return false 
 */
bool writeNvs32(const char* key, int32_t val)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

/**
 * @brief TODO
 * 
 * @param key 
 * @param outVal 
 * @return true 
 * @return false 
 */
bool readNvs32(const char* key, int32_t* outVal)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

//==============================================================================
// OLED
//==============================================================================

/**
 * @brief TODO
 * 
 * @param disp 
 * @param reset 
 * @param rst 
 * @return true 
 * @return false 
 */
bool initOLED(display_t * disp, bool reset, gpio_num_t rst)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

//==============================================================================
// SPIFFS
//==============================================================================

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool initSpiffs(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool deinitSpiffs(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

/**
 * @brief TODO
 * 
 * @param fname 
 * @param output 
 * @param outsize 
 * @return true 
 * @return false 
 */
bool spiffsReadFile(const char * fname, uint8_t ** output, size_t * outsize)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

//==============================================================================
// Temperature Sensor
//==============================================================================

/**
 * @brief TODO
 * 
 */
void initTemperatureSensor(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @return float 
 */
float readTemperatureSensor(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return 0;
}

//==============================================================================
// TFT
//==============================================================================

/**
 * @brief TODO
 * 
 * @param disp 
 * @param spiHost 
 * @param sclk 
 * @param mosi 
 * @param dc 
 * @param cs 
 * @param rst 
 * @param backlight 
 */
void initTFT(display_t * disp, spi_host_device_t spiHost, gpio_num_t sclk,
            gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs, gpio_num_t rst,
            gpio_num_t backlight)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

//==============================================================================
// Accelerometer
//==============================================================================

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool QMA6981_setup(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
    return false;
}

/**
 * @brief TODO
 * 
 * @param currentAccel 
 */
void QMA6981_poll(accel_t* currentAccel)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}
