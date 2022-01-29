#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "esp_emu.h"
#include "emu_main.h"
#include <list.h>
#include <cJSON.h>

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
 * @brief Set up the keyboard to act as input buttons
 *
 * @param numButtons The number of buttons to initialize
 * @param ... A list of GPIOs, which are ignored
 */
void initButtons(uint8_t numButtons, ...)
{
    // The order in which keys are initialized
    // Note that the actuall number of buttons initialized may be less than this
    char keyOrder[] = {'w', 's', 'a', 'd', 'i', 'k', 'j', 'l'};
    setInputKeys(numButtons, keyOrder);
}

/**
 * @brief Do nothing
 *
 */
void deinitializeButtons(void)
{
    ;
}

/**
 * @brief Check the input queue for any events
 *
 * @return true if there was an event, false if there wasn't
 */
bool checkButtonQueue(buttonEvt_t* evt)
{
    return checkInputKeys(evt);
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 * @param song
 */
void buzzer_play(const song_t* song)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 */
void buzzer_check_next_note(void)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 */
void buzzer_stop(void)
{
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool checkTouchSensor(touch_event_t * evt)
{
    WARN_UNIMPLEMENTED();
    return false;
}

//==============================================================================
// I2C
//==============================================================================

/**
 * @brief Do Nothing
 *
 * @param sda unused
 * @param scl unused
 * @param pullup unused
 * @param clkHz unused
 */
void i2c_master_init(gpio_num_t sda UNUSED, gpio_num_t scl UNUSED,
    gpio_pullup_t pullup UNUSED, uint32_t clkHz UNUSED)
{
    ; // Nothing to du
}

//==============================================================================
// LEDs
//==============================================================================

/**
 * @brief TODO
 *
 * @param gpio unused
 * @param rmt unused
 * @param numLeds The number of LEDs to display
 */
void initLeds(gpio_num_t gpio UNUSED, rmt_channel_t rmt UNUSED, uint16_t numLeds)
{
    initRawdrawLeds(numLeds);
}

/**
 * @brief TODO
 *
 * @param leds
 * @param numLeds
 */
void setLeds(led_t* leds, uint8_t numLeds)
{
    setRawdrawLeds(leds, numLeds);
}

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 * @param h The input hue
 * @param s The input saturation
 * @param v The input value
 * @param r The output red
 * @param g The output green
 * @param b The output blue
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t* r,
    uint8_t* g, uint8_t* b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
        case 0:
            *r = rgb_max;
            *g = rgb_min + rgb_adj;
            *b = rgb_min;
            break;
        case 1:
            *r = rgb_max - rgb_adj;
            *g = rgb_max;
            *b = rgb_min;
            break;
        case 2:
            *r = rgb_min;
            *g = rgb_max;
            *b = rgb_min + rgb_adj;
            break;
        case 3:
            *r = rgb_min;
            *g = rgb_max - rgb_adj;
            *b = rgb_max;
            break;
        case 4:
            *r = rgb_min + rgb_adj;
            *g = rgb_min;
            *b = rgb_max;
            break;
        default:
            *r = rgb_max;
            *g = rgb_min;
            *b = rgb_max - rgb_adj;
            break;
    }
}

//==============================================================================
// NVS
//==============================================================================

#define NVS_JSON_FILE "nvs.json"

/**
 * @brief Initialize NVS by making sure the file exists
 *
 * @param firstTry unused
 * @return true if the file exists or was created, false otherwise
 */
bool initNvs(bool firstTry)
{
    // Check if the json file exists
    if( access( NVS_JSON_FILE, F_OK ) != 0 )
    {
        FILE * nvsFile = fopen(NVS_JSON_FILE, "wb");
        if(NULL != nvsFile)
        {
            if(1 == fwrite("{}", sizeof("{}"), 1, nvsFile))
            {
                // Wrote successfully
                fclose(nvsFile);
                return true;
            }
            else
            {
                // Failed to write
                fclose(nvsFile);
                return false;
            }
        }
        else
        {
            // Couldn't open file
            return false;
        }
    }
    else
    {
        // File exists
        return true;
    }
}

/**
 * @brief Write a 32 bit value to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param val The value to write
 * @return true if the value was written, false if it was not
 */
bool writeNvs32(const char* key, int32_t val)
{
    // Open the file
    FILE * nvsFile = fopen(NVS_JSON_FILE, "rb");
    if(NULL != nvsFile)
    {
        // Get the file size
        fseek(nvsFile, 0L, SEEK_END);
        size_t fsize = ftell(nvsFile);
        fseek(nvsFile, 0L, SEEK_SET);

        // Read the file
        char fbuf[fsize + 1];
        fbuf[fsize] = 0;
        if(fsize == fread(fbuf, 1, fsize, nvsFile))
        {
            // Close the file
            fclose(nvsFile);

            // Parse the JSON
            cJSON * json = cJSON_Parse(fbuf);

            // Check if the key alredy exists
            cJSON * jsonIter;
            bool keyExists = false;
            cJSON_ArrayForEach(jsonIter, json)
            {
                if(0 == strcmp(jsonIter->string, key))
                {
                    keyExists = true;
                }
            }

            // Add or replace the item
            cJSON * jsonVal = cJSON_CreateNumber(val);
            if(keyExists)
            {
                cJSON_ReplaceItemInObject(json, key, jsonVal);
            }
            else
            {
                cJSON_AddItemToObject(json, key, jsonVal);
            }

            // Write the new JSON back to the file
            FILE * nvsFileW = fopen(NVS_JSON_FILE, "wb");
            if(NULL != nvsFileW)
            {
                char * jsonStr = cJSON_Print(json);
                fprintf(nvsFileW, "%s", jsonStr);
                free(jsonStr);

                fclose(nvsFileW);
                return true;
            }
            else
            {
                // Couldn't open file to write
            }
        }
        else
        {
            // Couldn't read file
            fclose(nvsFile);
        }
    }
    else
    {
        // couldn't open file to read
    }
    return false;
}

/**
 * @brief Read a 32 bit value from NVS with a given string key
 * 
 * @param key The key for the value to read
 * @param outVal The value that was read
 * @return true if the value was read, false if it was not
 */
bool readNvs32(const char* key, int32_t* outVal)
{
    // Open the file
    FILE * nvsFile = fopen(NVS_JSON_FILE, "rb");
    if(NULL != nvsFile)
    {
        // Get the file size
        fseek(nvsFile, 0L, SEEK_END);
        size_t fsize = ftell(nvsFile);
        fseek(nvsFile, 0L, SEEK_SET);

        // Read the file
        char fbuf[fsize + 1];
        fbuf[fsize] = 0;
        if(fsize == fread(fbuf, 1, fsize, nvsFile))
        {
            // Close the file
            fclose(nvsFile);

            // Parse the JSON
            cJSON * json = cJSON_Parse(fbuf);
            cJSON * jsonIter = json;

            // Find the requested key
            char *current_key = NULL;
            cJSON_ArrayForEach(jsonIter, json)
            {
                current_key = jsonIter->string;
                if (current_key != NULL)
                {
                    // If the key matches
                    if(0 == strcmp(current_key, key))
                    {
                        // Return the value
                        *outVal = (int32_t)cJSON_GetNumberValue(jsonIter);
                        return true;
                    }
                }
            }
        }
        else
        {
            fclose(nvsFile);
        }
    }
    return false;
}

//==============================================================================
// OLED
//==============================================================================

/**
 * @brief Initialie a null OLED since the OLED isn't emulated
 *
 * @param disp The display to initialize
 * @param reset true to reset the OLED using the RST line, false to leave it alone
 * @param rst_gpio The GPIO for the reset pin
 * @return true if it initialized, false if it failed
 */
bool initOLED(display_t * disp, bool reset UNUSED, gpio_num_t rst UNUSED)
{
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
 * @brief Do nothing, the normal file system replaces SPIFFS well
 *
 * @return true
 */
bool initSpiffs(void)
{
    return true;
}

/**
 * @brief Do nothing, the normal file system replaces SPIFFS well
 *
 * @return true
 */
bool deinitSpiffs(void)
{
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 * @return float
 */
float readTemperatureSensor(void)
{
    WARN_UNIMPLEMENTED();
    return 0;
}

//==============================================================================
// TFT
//==============================================================================

/**
 * @brief Initialize a TFT display and return it through a pointer arg
 *
 * @param disp The display to initialize
 * @param spiHost UNUSED
 * @param sclk UNUSED
 * @param mosi UNUSED
 * @param dc UNUSED
 * @param cs UNUSED
 * @param rst UNUSED
 * @param backlight UNUSED
 */
void initTFT(display_t * disp, spi_host_device_t spiHost UNUSED,
    gpio_num_t sclk UNUSED, gpio_num_t mosi UNUSED, gpio_num_t dc UNUSED,
    gpio_num_t cs UNUSED, gpio_num_t rst UNUSED, gpio_num_t backlight UNUSED)
{
    WARN_UNIMPLEMENTED();

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

    // Rawdraw initialized in main

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
    WARN_UNIMPLEMENTED();
    return false;
}

/**
 * @brief TODO
 *
 * @param currentAccel
 */
void QMA6981_poll(accel_t* currentAccel)
{
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 */
void espNowDeinit(void)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param data 
 * @param len 
 */
void espNowSend(const uint8_t* data, uint8_t len)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 */
void checkEspNowRxQueue(void)
{
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 */
void p2pDeinit(p2pInfo* p2p)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 */
void p2pStartConnection(p2pInfo* p2p)
{
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @return playOrder_t 
 */
playOrder_t p2pGetPlayOrder(p2pInfo* p2p)
{
    WARN_UNIMPLEMENTED();
    return NOT_SET;
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param order 
 */
void p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order)
{
    WARN_UNIMPLEMENTED();
}
