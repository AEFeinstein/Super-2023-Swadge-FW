#include <stdio.h>
#include <unistd.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "hal/gpio_types.h"
#include "hal/adc_types.h"
#include "driver/adc.h"
#include "driver/rmt.h"
#include "esp_timer.h"
#include "esp_efuse.h"
#include "esp_log.h"

#include "swadge_esp32.h"

#include "led_util.h"
#include "btn.h"
#include "touch_sensor.h"
#include "i2c-conf.h"
#include "ssd1306.h"
#include "hdw-tft.h"

#include "QMA6981.h"

#include "musical_buzzer.h"
#include "hdw-mic.h"

#include "esp_temperature_sensor.h"

#include "tinyusb.h"
#include "tusb_hid_gamepad.h"

#include "nvs_manager.h"
#include "spiffs_manager.h"

#include "espNowUtils.h"
#include "p2pConnection.h"

#include "swadgeMode.h"

#include "display.h"

#include "mode_main_menu.h"
#include "mode_demo.h"
#include "mode_fighter.h"

#ifdef EMU
#include "emu_esp.h"
#endif

//==============================================================================
// Function Prototypes
//==============================================================================

void mainSwadgeTask(void * arg);
void swadgeModeEspNowRecvCb(const uint8_t* mac_addr, const char* data, 
    uint8_t len, int8_t rssi);
void swadgeModeEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

//==============================================================================
// Variables
//==============================================================================

swadgeMode* swadgeModes[] =
{
    &modemainMenu,
    &modeFighter,
    &modeDemo
};

uint8_t swadgeModeIdx = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is
 * received. It routes through user_main.c, which knows what the current mode is
 */
void swadgeModeEspNowRecvCb(const uint8_t* mac_addr, const char* data, 
    uint8_t len, int8_t rssi)
{
    if(NULL != swadgeModes[swadgeModeIdx]->fnEspNowRecvCb)
    {
        swadgeModes[swadgeModeIdx]->fnEspNowRecvCb(mac_addr, data, len, rssi);
    }
}

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is sent
 * It routes through user_main.c, which knows what the current mode is
 */
void swadgeModeEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    if(NULL != swadgeModes[swadgeModeIdx]->fnEspNowSendCb)
    {
        swadgeModes[swadgeModeIdx]->fnEspNowSendCb(mac_addr, status);
    }
}

/**
 * Invoked when received GET_REPORT control request
 * Application must fill buffer report's content and return its length.
 * Return zero will cause the stack to STALL request
 * 
 * Unimplemented, and seemingly never called. Arguments are unclear.
 * Providing this function is necessary for compilation
 *
 * @param itf
 * @param report_id
 * @param report_type
 * @param buffer
 * @param reqlen
 * @return uint16_t
 */
uint16_t tud_hid_get_report_cb(uint8_t itf __attribute__((unused)),
    uint8_t report_id __attribute__((unused)),
    hid_report_type_t report_type __attribute__((unused)),
    uint8_t* buffer __attribute__((unused)),
    uint16_t reqlen __attribute__((unused)))
{
    return 0;
}

/**
 * Invoked when received SET_REPORT control request or
 * received data on OUT endpoint ( Report ID = 0, Type = 0 )
 * 
 * Unimplemented, and seemingly never called. Arguments are unclear.
 * Providing this function is necessary for compilation
 *
 * @param itf
 * @param report_id
 * @param report_type
 * @param buffer
 * @param bufsize
 */
void tud_hid_set_report_cb(uint8_t itf __attribute__((unused)),
    uint8_t report_id __attribute__((unused)),
    hid_report_type_t report_type __attribute__((unused)),
    uint8_t const* buffer __attribute__((unused)),
    uint16_t bufsize __attribute__((unused)))
{
    ;
}

/**
 * After all other components are initialized, the main task is created and the
 * FreeRTOS scheduler starts running.
 *
 * After doing some more initialization tasks (that require the scheduler to
 * have started), the main task runs the application-provided function app_main
 * in the firmware.
 *
 * The main task that runs app_main has a fixed RTOS priority (one higher than
 * the minimum) and a configurable stack size.
 *
 * Unlike normal FreeRTOS tasks (or embedded C main functions), the app_main
 * task is allowed to return. If this happens, The task is cleaned up and the
 * system will continue running with other RTOS tasks scheduled normally.
 * Therefore, it is possible to implement app_main as either a function that
 * creates other application tasks and then returns, or as a main application
 * task itself.
 */
void app_main(void)
{
#ifdef PRINT_STATS
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGD("MAIN", "This is %s chip with %d CPU core(s), WiFi%s%s, ",
                CONFIG_IDF_TARGET,
                chip_info.cores,
                (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGD("MAIN", "silicon revision %d, ", chip_info.revision);

    ESP_LOGD("MAIN", "%dMB %s flash", spi_flash_get_chip_size() / (1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGD("MAIN", "Minimum free heap size: %d bytes", esp_get_minimum_free_heap_size());
#endif

    /* The ESP32-C3 can enumerate as a USB CDC device using pins 18 and 19
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/usb-serial-jtag-console.html
     *
     * This is enabled or disabled with idf.py menuconfig -> "Channel for console output"
     *
     * When USB JTAG is enabled, the ESP32-C3 will halt and wait for a reception
     * any time it tries to transmit bytes on the UART. This means a program
     * won't run if a console isn't open, because the IDF tries to print stuff
     * on boot.
     * To get around this, use the efuse bit to permanently disable logging
     */
    // esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_OFF);
    esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_ON);

    // Set up timers
    esp_timer_init();

    // Create a task for the swadge, then return
    TaskHandle_t xHandle = NULL;
    xTaskCreate(mainSwadgeTask, "SWADGE", 8192, NULL, 
        tskIDLE_PRIORITY /*configMAX_PRIORITIES / 2*/, &xHandle);
}

/**
 * @brief This is the task for the swadge. It sets up all peripherals and runs
 * the firmware in a while(1) loop
 * 
 * @param arg unused
 */
void mainSwadgeTask(void * arg __attribute((unused)))
{
    /* Initialize internal NVS */
    initNvs(true);

    /* Initialize SPIFFS */
    initSpiffs();

    initTemperatureSensor();

    /* Initialize non-i2c hardware peripherals */
    initButtons(8,
        GPIO_NUM_4,
        GPIO_NUM_5,
        GPIO_NUM_6,
        GPIO_NUM_7,
        GPIO_NUM_15,
        GPIO_NUM_16,
        GPIO_NUM_17,
        GPIO_NUM_18);

    initTouchSensor(0.2f, true, 6,
        TOUCH_PAD_NUM9,   // GPIO_NUM_9
        TOUCH_PAD_NUM10,  // GPIO_NUM_10
        TOUCH_PAD_NUM11,  // GPIO_NUM_11
        TOUCH_PAD_NUM12,  // GPIO_NUM_12
        TOUCH_PAD_NUM13,  // GPIO_NUM_13
        TOUCH_PAD_NUM14); // GPIO_NUM_14

    initLeds(GPIO_NUM_8, RMT_CHANNEL_0, NUM_LEDS);
    buzzer_init(GPIO_NUM_35, RMT_CHANNEL_1);

    if(NULL != swadgeModes[swadgeModeIdx]->fnAudioCallback)
    {
        static uint16_t adc1_chan_mask = BIT(2);
        static uint16_t adc2_chan_mask = 0;
        static adc_channel_t channel[] = {ADC_CHANNEL_1}; // GPIO_NUM_3
        continuous_adc_init(adc1_chan_mask, adc2_chan_mask, channel, sizeof(channel) / sizeof(adc_channel_t));
        continuous_adc_start();
    }

    bool accelInitialized = false;
    if(NULL != swadgeModes[swadgeModeIdx]->fnAccelerometerCallback)
    {
        /* Initialize i2c peripherals */
        i2c_master_init(
            GPIO_NUM_33,
            GPIO_NUM_34,
            GPIO_PULLUP_DISABLE, 1000000);
        accelInitialized = QMA6981_setup();
    }
#ifdef OLED_ENABLED
    display_t oledDisp;
    initOLED(&oledDisp, true, GPIO_NUM_21);
#endif

    /* Initialize SPI peripherals */
    display_t tftDisp;
    initTFT(&tftDisp,
            SPI2_HOST,
            GPIO_NUM_37,  // sclk
            GPIO_NUM_38,  // mosi
            GPIO_NUM_41,  // dc
            GPIO_NUM_39,  // cs
            GPIO_NUM_40,  // rst
            GPIO_NUM_42); // backlight

    /* Initialize USB peripheral */
    tinyusb_config_t tusb_cfg = {};
    tinyusb_driver_install(&tusb_cfg);

    /* Initialize Wifi peripheral */
    if(ESP_NOW == swadgeModes[swadgeModeIdx]->wifiMode)
    {
        espNowInit(&swadgeModeEspNowRecvCb, &swadgeModeEspNowSendCb);
    }

    /* Enter the swadge mode */
    if(NULL != swadgeModes[swadgeModeIdx]->fnEnterMode)
    {
        swadgeModes[swadgeModeIdx]->fnEnterMode(&tftDisp);
    }

    /* Loop forever! */
#ifdef EMU
    while(threadsShouldRun)
#else
    while(true)
#endif
    {
        // Process ESP NOW
        if(ESP_NOW == swadgeModes[swadgeModeIdx]->wifiMode)
        {
            checkEspNowRxQueue();
        }
        
        // Process Accelerometer
        if(accelInitialized)
        {
            accel_t accel = {0};
            QMA6981_poll(&accel);
            swadgeModes[swadgeModeIdx]->fnAccelerometerCallback(&accel);
        }

        // Process temperature sensor
        if(NULL != swadgeModes[swadgeModeIdx]->fnTemperatureCallback)
        {
            swadgeModes[swadgeModeIdx]->fnTemperatureCallback(readTemperatureSensor());
        }

        // Process button presses
        buttonEvt_t bEvt = {0};
        if(checkButtonQueue(&bEvt))
        {
            if(NULL != swadgeModes[swadgeModeIdx]->fnButtonCallback)
            {
                swadgeModes[swadgeModeIdx]->fnButtonCallback(&bEvt);
            }
        }

        // Process touch events
        touch_event_t tEvt = {0};
        if(checkTouchSensor(&tEvt))
        {
            if(NULL != swadgeModes[swadgeModeIdx]->fnTouchCallback)
            {
                swadgeModes[swadgeModeIdx]->fnTouchCallback(&tEvt);
            }
        }

        // Process ADC samples
        if(NULL != swadgeModes[swadgeModeIdx]->fnAudioCallback)
        {
            uint16_t adcSamps[BYTES_PER_READ / sizeof(adc_digi_output_data_t)];
            uint32_t sampleCnt = 0;
            while(0 < (sampleCnt = continuous_adc_read(adcSamps)))
            {
                swadgeModes[swadgeModeIdx]->fnAudioCallback(adcSamps, sampleCnt);
            }
        }

        // Run the mode's event loop
        static int64_t tLastCallUs = 0;
        if(0 == tLastCallUs)
        {
            tLastCallUs = esp_timer_get_time();
        }
        else
        {
            int64_t tNowUs = esp_timer_get_time();
            int64_t tElapsedUs = tNowUs - tLastCallUs;
            tLastCallUs = tNowUs;

            if(NULL != swadgeModes[swadgeModeIdx]->fnMainLoop)
            {
                swadgeModes[swadgeModeIdx]->fnMainLoop(tElapsedUs);
            }

#ifdef EMU
            check_esp_timer(tElapsedUs);
#endif
        }

        // Update outputs
#ifdef OLED_ENABLED
        oledDisp.drawDisplay(true);
#endif
        tftDisp.drawDisplay(true);
        buzzer_check_next_note();

        // Yield to let the rest of the RTOS run
        taskYIELD();
        // Note, the RTOS tick rate can be changed in idf.py menuconfig
        // (100hz by default)
    }

    if(NULL != swadgeModes[swadgeModeIdx]->fnAudioCallback)
    {
        continuous_adc_stop();
        continuous_adc_deinit();
    }

    if(NULL != swadgeModes[swadgeModeIdx]->fnExitMode)
    {
        swadgeModes[swadgeModeIdx]->fnExitMode();
    }

#ifdef EMU
    esp_timer_deinit();
#endif
}