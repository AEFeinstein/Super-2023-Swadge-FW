#include <string.h>
#include <stdio.h>

#include "esp_emu.h"
#include "emu_main.h"

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
bool checkButtonQueue(buttonEvt_t* evt)
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
bool checkTouchSensor(touch_event_t * evt)
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
bool initOLED(display_t * disp, bool reset UNUSED, gpio_num_t rst UNUSED)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);

    disp->w = 0;
    disp->h = 0;
    disp->getPx = emuGetPxOled;
    disp->setPx = emuSetPxOled;
    disp->clearPx = emuClearPxOled;
    disp->drawDisplay = emuDrawDisplayOled;
    
    return true;
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
 * @brief Read a file from SPIFFS into an output array. Files that are in the
 * spiffs_image folder before compilation and flashing will automatically
 * be included in the firmware
 *
 * @param fname   The name of the file to load
 * @param output  A pointer to a pointer to return the read data in. This memory
 *                will be allocated with calloc(). Must be NULL to start
 * @param outsize A pointer to a size_t to return how much data was read
 * @return true if the file was read successfully, false otherwise
 */
bool spiffsReadFile(const char * fname, uint8_t ** output, size_t * outsize)
{
    // Make sure the output pointer is NULL to begin with
    if(NULL != *output)
    {
        ESP_LOGE("SPIFFS", "output not NULL");
        return false;
    }

    // Read and display the contents of a small text file
    ESP_LOGD("SPIFFS", "Reading %s", fname);

    // Open for reading the given file
    char fnameFull[128] = "./spiffs_image/";
    strcat(fnameFull, fname);
    FILE* f = fopen(fnameFull, "rb");
    if (f == NULL) {
        ESP_LOGE("SPIFFS", "Failed to open %s", fnameFull);
        return false;
    }

    // Get the file size
    fseek(f, 0L, SEEK_END);
    *outsize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    // Read the file into an array
    *output = (uint8_t*)calloc((*outsize + 1), sizeof(uint8_t));
    fread(*output, *outsize, 1, f);

    // Close the file
    fclose(f);

    // Display the read contents from the file
    ESP_LOGD("SPIFFS", "Read from %s: %d bytes", fname, *outsize);
    return true;
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
void initTFT(display_t * disp, spi_host_device_t spiHost UNUSED,
    gpio_num_t sclk UNUSED, gpio_num_t mosi UNUSED, gpio_num_t dc UNUSED,
    gpio_num_t cs UNUSED, gpio_num_t rst UNUSED, gpio_num_t backlight UNUSED)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);

/* Screen-specific configurations */
#if defined(CONFIG_ST7735_160x80)
    #define TFT_WIDTH         160
    #define TFT_HEIGHT         80
#elif defined(CONFIG_ST7789_240x135)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        135
#elif defined(CONFIG_ST7789_240x240)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        240
#else
    #error "Please pick a screen size"
#endif

    initRawDraw(TFT_WIDTH, TFT_HEIGHT);

    disp->w = TFT_WIDTH;
    disp->h = TFT_HEIGHT;
    disp->getPx = emuGetPxTft;
    disp->setPx = emuSetPxTft;
    disp->clearPx = emuClearPxTft;
    disp->drawDisplay = emuDrawDisplayTft;
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

//==============================================================================
// ESP Now
//==============================================================================

/**
 * @brief TODO
 * 
 * @param recvCb 
 * @param sendCb 
 */
void espNowInit(hostEspNowRecvCb_t recvCb, hostEspNowSendCb_t sendCb)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 */
void espNowDeinit(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param data 
 * @param len 
 */
void espNowSend(const uint8_t* data, uint8_t len)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 */
void checkEspNowRxQueue(void)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

//==============================================================================
// P2P Connection
//==============================================================================

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param msgId 
 * @param conCbFn 
 * @param msgRxCbFn 
 * @param connectionRssi 
 */
void p2pInitialize(p2pInfo* p2p, char* msgId,
                   p2pConCbFn conCbFn,
                   p2pMsgRxCbFn msgRxCbFn, int8_t connectionRssi)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 */
void p2pDeinit(p2pInfo* p2p)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 */
void p2pStartConnection(p2pInfo* p2p)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param msg 
 * @param payload 
 * @param len 
 * @param msgTxCbFn 
 */
void p2pSendMsg(p2pInfo* p2p, char* msg, char* payload, uint16_t len, p2pMsgTxCbFn msgTxCbFn)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param mac_addr 
 * @param status 
 */
void p2pSendCb(p2pInfo* p2p, const uint8_t* mac_addr, esp_now_send_status_t status)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param mac_addr 
 * @param data 
 * @param len 
 * @param rssi 
 */
void p2pRecvCb(p2pInfo* p2p, const uint8_t* mac_addr, const uint8_t* data, uint8_t len, int8_t rssi)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @return playOrder_t 
 */
playOrder_t p2pGetPlayOrder(p2pInfo* p2p)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param order 
 */
void p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order)
{
    ESP_LOGE("EMU", "%s UNIMPLEMENTED", __func__);
}
