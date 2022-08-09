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
#include "esp_sleep.h"

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

#include "display.h"

#include "advanced_usb_control.h"

#include "mode_main_menu.h"
#include "mode_demo.h"
#include "fighter_menu.h"
#include "mode_gamepad.h"

#include "driver/gpio.h"

#if defined(EMU)
#include "emu_esp.h"
#else
#include "soc/dport_access.h"
#include "soc/periph_defs.h"
#include "hal/memprot_ll.h"
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
    &modeMainMenu,
    &modeFighter,
    &modeDemo,
    &modeGamepad,
    0,
};

static RTC_DATA_ATTR uint8_t pendingSwadgeModeIdx = 0;
static uint8_t swadgeModeIdx = 0;
static bool shouldSwitchSwadgeMode = false;

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
uint16_t tud_hid_get_report_cb(uint8_t itf,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t* buffer,
    uint16_t reqlen)
{
#if !defined(EMU)
    if( report_id == 170 || report_id == 171 )
        return handle_advanced_usb_control_get( reqlen, buffer );
    else if( report_id == 172 )
        return handle_advanced_usb_terminal_get( reqlen, buffer );
    else
        return reqlen;
#else
    return 0;
#endif
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
void tud_hid_set_report_cb(uint8_t itf,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const* buffer,
    uint16_t bufsize )
{
#if !defined(EMU)
    if( report_id >= 170 && report_id <= 171 )
        handle_advanced_usb_control_set( bufsize, buffer );
#endif
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
    // Pull these GPIOs low immediately!!!
    // This prevents overheating on the devkit
    gpio_num_t toPullLow[] = {
        GPIO_NUM_33, // TFTATN
        GPIO_NUM_35, // TFTATP
        GPIO_NUM_41, // SP-
        GPIO_NUM_42  // SP+
    };
    for(uint8_t i = 0; i < ARRAY_SIZE(toPullLow); i++)
    {
        gpio_config_t atn_gpio_config =
        {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << toPullLow[i]
        };
        ESP_ERROR_CHECK(gpio_config(&atn_gpio_config));
        ESP_ERROR_CHECK(gpio_set_level(toPullLow[i], 0));
    }

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
    heap_caps_print_heap_info(MALLOC_CAP_EXEC | MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_PID2 | MALLOC_CAP_PID3 | MALLOC_CAP_PID4 | MALLOC_CAP_PID5 | MALLOC_CAP_PID6 | MALLOC_CAP_PID7 | MALLOC_CAP_SPIRAM | MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT | MALLOC_CAP_IRAM_8BIT | MALLOC_CAP_RETENTION | MALLOC_CAP_RTCRAM);
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
#if !defined(EMU)
    /* Check why this ESP woke up */
    switch (esp_sleep_get_wakeup_cause())
    {
        case ESP_SLEEP_WAKEUP_TIMER:
        {
            /* Use the pending mode */
            swadgeModeIdx = pendingSwadgeModeIdx;
            break;
        }
        default:
        {
            /* Reset swadgeModeIdx */
            swadgeModeIdx = 0;
            break;
        }
    }
#endif

    /* Initialize internal NVS */
    initNvs(true);

    /* Initialize SPIFFS */
    initSpiffs();

    initTemperatureSensor();

    /* Initialize non-i2c hardware peripherals */
    initButtons(8,
        GPIO_NUM_1,
        GPIO_NUM_0,
        GPIO_NUM_3,
        GPIO_NUM_2,
        GPIO_NUM_5,
        GPIO_NUM_45,
        GPIO_NUM_4,
        GPIO_NUM_15); // GPIO 46 doesn't work b/c it has a permanent pulldown

    initTouchSensor(0.2f, true, 6,
        TOUCH_PAD_NUM9,   // GPIO_NUM_9
        TOUCH_PAD_NUM10,  // GPIO_NUM_10
        TOUCH_PAD_NUM11,  // GPIO_NUM_11
        TOUCH_PAD_NUM12,  // GPIO_NUM_12
        TOUCH_PAD_NUM13,  // GPIO_NUM_13
        TOUCH_PAD_NUM14); // GPIO_NUM_14

    initLeds(GPIO_NUM_39, RMT_CHANNEL_0, NUM_LEDS);
    buzzer_init(GPIO_NUM_40, RMT_CHANNEL_1);

#if !defined(EMU)
    if(NULL != swadgeModes[swadgeModeIdx]->fnAudioCallback)
#endif
    {
        /* Since the ADC2 is shared with the WIFI module, which has higher
         * priority, reading operation of adc2_get_raw() will fail between
         * esp_wifi_start() and esp_wifi_stop(). Use the return code to see
         * whether the reading is successful.
         */
        static uint16_t adc1_chan_mask = BIT(2);
        static uint16_t adc2_chan_mask = 0;
        static adc_channel_t channel[] = {ADC1_CHANNEL_7}; // GPIO_NUM_8
        continuous_adc_init(adc1_chan_mask, adc2_chan_mask, channel, sizeof(channel) / sizeof(adc_channel_t));
        continuous_adc_start();
    }

    bool accelInitialized = false;
#if !defined(EMU)
    if(NULL != swadgeModes[swadgeModeIdx]->fnAccelerometerCallback)
#endif
    {
        /* Initialize i2c peripherals */
        i2c_master_init(
            GPIO_NUM_17, // SDA
            GPIO_NUM_18, // SCL
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
            GPIO_NUM_36, // sclk
            GPIO_NUM_37, // mosi
            GPIO_NUM_21, // dc
            GPIO_NUM_34, // cs
            GPIO_NUM_38, // rst
            GPIO_NUM_7); // backlight (dummy GPIO for now)

    /* Initialize USB peripheral */
    tinyusb_config_t tusb_cfg = {};
    tinyusb_driver_install(&tusb_cfg);

    /* Initialize Wifi peripheral */
#if !defined(EMU)
    if(ESP_NOW == swadgeModes[swadgeModeIdx]->wifiMode)
#endif
    {
        espNowInit(&swadgeModeEspNowRecvCb, &swadgeModeEspNowSendCb);
    }

    /* Enter the swadge mode */
    if(NULL != swadgeModes[swadgeModeIdx]->fnEnterMode)
    {
        swadgeModes[swadgeModeIdx]->fnEnterMode(&tftDisp);
    }

    /* Loop forever! */
#if defined(EMU)
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
        if(accelInitialized && NULL != swadgeModes[swadgeModeIdx]->fnAccelerometerCallback)
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

#if defined(EMU)
            check_esp_timer(tElapsedUs);
#endif
        }

        // Update outputs
#ifdef OLED_ENABLED
        oledDisp.drawDisplay(true);
#endif
        tftDisp.drawDisplay(true);
        buzzer_check_next_note();

        /* If the mode should be switched, do it now */
        if(shouldSwitchSwadgeMode)
        {
#if defined(EMU)
            int force_soft = 1;
#else
            int force_soft = 0;
#endif
            if( force_soft || 
                pendingSwadgeModeIdx == 0 ||
                pendingSwadgeModeIdx == getNumSwadgeModes() )
            {
                // Exit the current mode
                if(NULL != swadgeModes[swadgeModeIdx]->fnExitMode)
                {
                    swadgeModes[swadgeModeIdx]->fnExitMode();
                }

                // Switch the mode IDX
                swadgeModeIdx = pendingSwadgeModeIdx;
                shouldSwitchSwadgeMode = false;

                // Enter the next mode
                if(NULL != swadgeModes[swadgeModeIdx]->fnEnterMode)
                {
                    swadgeModes[swadgeModeIdx]->fnEnterMode(&tftDisp);
                }
            }
            else
            {
                // Deep sleep, wake up, and switch to pendingSwadgeModeIdx
                esp_sleep_enable_timer_wakeup(1);
                esp_deep_sleep_start();
            }
        }

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

#if defined(EMU)
    esp_timer_deinit();
#endif
}

/**
 * @return The total number of Swadge modes, including the main menu (mode 0)
 */
uint8_t getNumSwadgeModes(void)
{
    return ARRAY_SIZE(swadgeModes)-1;
}

/**
 * Set up variables to synchronously switch the swadge mode in the main loop
 * 
 * @param mode The index of the mode to switch to
 */
void switchToSwadgeMode(uint8_t mode)
{
    pendingSwadgeModeIdx = mode;
    shouldSwitchSwadgeMode = true;
}

/**
 * Use a pointer to a swadge mode to jump to a dynamic swadge mode.
 * 
 * @param mode Pointer to a valid swadgeMode struct.
 */
void overrideToSwadgeMode( swadgeMode* mode )
{
    int sandboxSwadgeMode = getNumSwadgeModes();
    swadgeModes[sandboxSwadgeMode] = mode;
    switchToSwadgeMode( sandboxSwadgeMode );
}

