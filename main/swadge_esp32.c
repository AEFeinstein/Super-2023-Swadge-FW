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

#ifndef EMU
    #include "soc/rtc_cntl_reg.h"
#endif

#define QMA7981

#if defined(QMA6981)
    #include "QMA6981.h"
#elif defined(QMA7981)
    #include "qma7981.h"
#endif

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
#include "jumper_menu.h"
#include "fighter_menu.h"
#include "mode_gamepad.h"
#include "mode_test.h"

#include "driver/gpio.h"

#include "settingsManager.h"

#if defined(EMU)
    #include "emu_esp.h"
#else
    #include "soc/dport_access.h"
    #include "soc/periph_defs.h"
    #include "hal/memprot_ll.h"
#endif

//==============================================================================
// Defines
//==============================================================================

// Make sure one, and only one, build config is enabled
#if (((defined(CONFIG_SWADGE_DEVKIT)    ? 1 : 0) + \
      (defined(CONFIG_SWADGE_PROTOTYPE) ? 1 : 0)) != 1)
#error "Please define CONFIG_SWADGE_DEVKIT or CONFIG_SWADGE_PROTOTYPE"
#endif

#define EXIT_TIME_US 1000000

//==============================================================================
// Function Prototypes
//==============================================================================

void mainSwadgeTask(void* arg);
void swadgeModeEspNowRecvCb(const uint8_t* mac_addr, const char* data,
                            uint8_t len, int8_t rssi);
void swadgeModeEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

//==============================================================================
// Variables
//==============================================================================

static RTC_DATA_ATTR swadgeMode* pendingSwadgeMode = NULL;
static swadgeMode* cSwadgeMode = &modeMainMenu;
static bool isSandboxMode = false;
static uint32_t frameRateUs = 33333;

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
    if(NULL != cSwadgeMode->fnEspNowRecvCb)
    {
        cSwadgeMode->fnEspNowRecvCb(mac_addr, data, len, rssi);
    }
}

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is sent
 * It routes through user_main.c, which knows what the current mode is
 */
void swadgeModeEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    if(NULL != cSwadgeMode->fnEspNowSendCb)
    {
        cSwadgeMode->fnEspNowSendCb(mac_addr, status);
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
    {
        return handle_advanced_usb_control_get( reqlen, buffer );
    }
    else if( report_id == 172 )
    {
        return handle_advanced_usb_terminal_get( reqlen, buffer );
    }
    else
    {
        return reqlen;
    }
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
    {
        handle_advanced_usb_control_set( bufsize, buffer );
    }
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
#if defined(CONFIG_SWADGE_DEVKIT)
    // Pull these GPIOs low immediately!!!
    // This prevents overheating on the devkit
    gpio_num_t toPullLow[] =
    {
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
#endif

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
    heap_caps_print_heap_info(MALLOC_CAP_EXEC | MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_PID2 |
                              MALLOC_CAP_PID3 | MALLOC_CAP_PID4 | MALLOC_CAP_PID5 | MALLOC_CAP_PID6 | MALLOC_CAP_PID7 | MALLOC_CAP_SPIRAM |
                              MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT | MALLOC_CAP_IRAM_8BIT | MALLOC_CAP_RETENTION | MALLOC_CAP_RTCRAM);
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

    /* Initialize USB peripheral */
    tinyusb_config_t tusb_cfg = {};
    tinyusb_driver_install(&tusb_cfg);

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
void mainSwadgeTask(void* arg __attribute((unused)))
{
    /* Initialize internal NVS. Do this first to get test mode status */
    initNvs(true);

#if !defined(EMU)
    /* Check why this ESP woke up */
    switch (esp_sleep_get_wakeup_cause())
    {
        case ESP_SLEEP_WAKEUP_TIMER:
        {
            /* Use the pending mode */
            if(NULL != pendingSwadgeMode)
            {
                cSwadgeMode = pendingSwadgeMode;
                pendingSwadgeMode = NULL;
            }
            else
            {
                cSwadgeMode = &modeMainMenu;
            }
            break;
        }
        default:
        {
#if !defined(CONFIG_SWADGE_DEVKIT)
            // If test mode was passed
            if(getTestModePassed())
#else
            // Ignore test mode for devkit
            if(true)
#endif
            {
                // Show the main menu
                cSwadgeMode = &modeMainMenu;
            }
            else
            {
                // Otherwise enter test mode
                cSwadgeMode = &modeTest;
            }
            break;
        }
    }
#endif

    /* Initialize SPIFFS */
    initSpiffs();

    initTemperatureSensor();

    /* Initialize non-i2c hardware peripherals */
#if defined(CONFIG_SWADGE_DEVKIT)
    initButtons(TIMER_GROUP_0, TIMER_0,
                8,
                GPIO_NUM_1,
                GPIO_NUM_0,
                GPIO_NUM_3,
                GPIO_NUM_2,
                GPIO_NUM_5,
                GPIO_NUM_45,
                GPIO_NUM_4,
                GPIO_NUM_15); // GPIO 46 doesn't work b/c it has a permanent pulldown
#elif defined(CONFIG_SWADGE_PROTOTYPE)
    initButtons(TIMER_GROUP_0, TIMER_0,
                8,
                GPIO_NUM_0,
                GPIO_NUM_4,
                GPIO_NUM_2,
                GPIO_NUM_1,
                GPIO_NUM_16,
                GPIO_NUM_15,
                GPIO_NUM_8,
                GPIO_NUM_5);
#endif

    // Same for CONFIG_SWADGE_DEVKIT and CONFIG_SWADGE_PROTOTYPE
    // Make sure to use a different timer than initButtons()
    buzzer_init(GPIO_NUM_40, RMT_CHANNEL_1, TIMER_GROUP_0, TIMER_1, getIsMuted());

#if defined(CONFIG_SWADGE_DEVKIT)
    initTouchSensor(0.2f, true, 6,
                    TOUCH_PAD_NUM9,   // GPIO_NUM_9
                    TOUCH_PAD_NUM10,  // GPIO_NUM_10
                    TOUCH_PAD_NUM11,  // GPIO_NUM_11
                    TOUCH_PAD_NUM12,  // GPIO_NUM_12
                    TOUCH_PAD_NUM13,  // GPIO_NUM_13
                    TOUCH_PAD_NUM14); // GPIO_NUM_14
#elif defined(CONFIG_SWADGE_PROTOTYPE)
    initTouchSensor(0.2f, true, 5,
                    TOUCH_PAD_NUM9,   // GPIO_NUM_9
                    TOUCH_PAD_NUM10,  // GPIO_NUM_10
                    TOUCH_PAD_NUM11,  // GPIO_NUM_11
                    TOUCH_PAD_NUM12,  // GPIO_NUM_12
                    TOUCH_PAD_NUM13); // GPIO_NUM_13
#endif

    // Same for CONFIG_SWADGE_DEVKIT and CONFIG_SWADGE_PROTOTYPE
    initLeds(GPIO_NUM_39, GPIO_NUM_18, RMT_CHANNEL_0, NUM_LEDS, getLedBrightness());

    if(NULL != cSwadgeMode->fnAudioCallback
#if defined(EMU)
            || true // Always init audio for the emulator
#endif
      )
    {
        /* Since the ADC2 is shared with the WIFI module, which has higher
         * priority, reading operation of adc2_get_raw() will fail between
         * esp_wifi_start() and esp_wifi_stop(). Use the return code to see
         * whether the reading is successful.
         */
#if defined(CONFIG_SWADGE_DEVKIT)
        static uint16_t adc1_chan_mask = BIT(2);
        static uint16_t adc2_chan_mask = 0;
        static adc_channel_t channel[] = {ADC1_CHANNEL_7}; // GPIO_NUM_8
#elif defined(CONFIG_SWADGE_PROTOTYPE)
        static uint16_t adc1_chan_mask = BIT(6);
        static uint16_t adc2_chan_mask = 0;
        static adc_channel_t channel[] = {ADC1_CHANNEL_6}; // GPIO_NUM_7
#endif
        continuous_adc_init(adc1_chan_mask, adc2_chan_mask, channel, sizeof(channel) / sizeof(adc_channel_t));
        continuous_adc_start();
    }
    else if (NULL != cSwadgeMode->fnBatteryCallback)
    {
#if defined(CONFIG_SWADGE_PROTOTYPE)
        // If the continuous ADC isn't set up, set up the one-shot one
        oneshot_adc_init(ADC_UNIT_1, ADC1_CHANNEL_5); /*!< ADC1 channel 5 is GPIO6  */
#endif
    }

    bool accelInitialized = false;
#if !defined(EMU)
    if(NULL != cSwadgeMode->fnAccelerometerCallback)
#endif
    {
        /* Initialize i2c peripherals */
#if defined(CONFIG_SWADGE_DEVKIT)
        i2c_master_init(
            GPIO_NUM_17, // SDA
            GPIO_NUM_18, // SCL
            GPIO_PULLUP_DISABLE, 1000000);
#elif defined(CONFIG_SWADGE_PROTOTYPE)
        i2c_master_init(
            GPIO_NUM_3,  // SDA
            GPIO_NUM_41, // SCL
            GPIO_PULLUP_DISABLE, 1000000);
#endif

#if defined(QMA6981)
        accelInitialized = QMA6981_setup();
#elif defined(QMA7981)
        accelInitialized = (ESP_OK == qma7981_init());
#endif
    }

#ifdef OLED_ENABLED
    display_t oledDisp;
    initOLED(&oledDisp, true, GPIO_NUM_21);
#endif

    /* Initialize SPI peripherals */
    display_t tftDisp;
#if defined(CONFIG_SWADGE_DEVKIT)
    initTFT(&tftDisp,
            SPI2_HOST,
            GPIO_NUM_36, // sclk
            GPIO_NUM_37, // mosi
            GPIO_NUM_21, // dc
            GPIO_NUM_34, // cs
            GPIO_NUM_38, // rst
            GPIO_NUM_7,  // backlight (dummy GPIO for now)
            false);      // binary backlight
#elif defined(CONFIG_SWADGE_PROTOTYPE)
    initTFT(&tftDisp,
            SPI2_HOST,
            GPIO_NUM_36, // sclk
            GPIO_NUM_37, // mosi
            GPIO_NUM_21, // dc
            GPIO_NUM_34, // cs
            GPIO_NUM_38, // rst
            GPIO_NUM_35, // backlight
            true);       // PWM backlight
#endif

    // Set the brightness from settings on boot
    setTFTBacklight(getTftIntensity());

    /* Initialize Wifi peripheral */
#if !defined(EMU)
    if(ESP_NOW == cSwadgeMode->wifiMode)
#endif
    {
        espNowInit(&swadgeModeEspNowRecvCb, &swadgeModeEspNowSendCb);
    }

    /* Enter the swadge mode */
    if(NULL != cSwadgeMode->fnEnterMode)
    {
        cSwadgeMode->fnEnterMode(&tftDisp);
    }

    int64_t time_exit_pressed = 0;

    /* Loop forever! */
#if defined(EMU)
    while(threadsShouldRun)
#else
    while(true)
#endif
    {
        // Process ESP NOW
        if(ESP_NOW == cSwadgeMode->wifiMode)
        {
            checkEspNowRxQueue();
        }

        // Process Accelerometer
        if(accelInitialized && NULL != cSwadgeMode->fnAccelerometerCallback)
        {
            accel_t accel = {0};
#if defined(QMA6981)
            QMA6981_poll(&accel);
#elif defined(QMA7981)
            qma7981_get_acce_int(&accel.x, &accel.y, &accel.z);
#endif
            cSwadgeMode->fnAccelerometerCallback(&accel);
        }

        // Process temperature sensor
        if(NULL != cSwadgeMode->fnTemperatureCallback)
        {
            cSwadgeMode->fnTemperatureCallback(readTemperatureSensor());
        }

        // Process button presses
        buttonEvt_t bEvt = {0};
        if(checkButtonQueue(&bEvt))
        {
            // Monitor start + select
            if((&modeMainMenu != cSwadgeMode) && (bEvt.state & START) && (bEvt.state & SELECT))
            {
                time_exit_pressed = esp_timer_get_time();
            }
            else
            {
                time_exit_pressed = 0;
            }

            if(NULL != cSwadgeMode->fnButtonCallback)
            {
                cSwadgeMode->fnButtonCallback(&bEvt);
            }
        }

        // Process touch events
        touch_event_t tEvt = {0};
        if(checkTouchSensor(&tEvt))
        {
            if(NULL != cSwadgeMode->fnTouchCallback)
            {
                cSwadgeMode->fnTouchCallback(&tEvt);
            }
        }

        // Process ADC samples
        if(NULL != cSwadgeMode->fnAudioCallback)
        {
            uint16_t micAmp = getMicAmplitude();
            uint16_t adcSamps[BYTES_PER_READ / sizeof(adc_digi_output_data_t)];
            uint32_t sampleCnt = 0;
            while(0 < (sampleCnt = continuous_adc_read(adcSamps)))
            {
                // Run all samples through an IIR filter
                for(uint32_t i = 0; i < sampleCnt; i++)
                {
                    static uint32_t samp_iir = 0;
                    int32_t sample = adcSamps[i];
                    samp_iir = samp_iir - (samp_iir >> 9) + sample;
                    int32_t newsamp = (sample - (samp_iir >> 9));
                    newsamp = (newsamp * micAmp);

                    if( newsamp <-32768 ) newsamp = -32768;
                    else if( newsamp > 32767) newsamp = 32767;

                    adcSamps[i] = newsamp;
                }
                cSwadgeMode->fnAudioCallback(adcSamps, sampleCnt);
            }
        }

        // Run the mode's event loop
        static int64_t tLastLoopUs = 0;
        if(0 == tLastLoopUs)
        {
            tLastLoopUs = esp_timer_get_time();
        }
        else
        {
            // Track the elapsed time between loop calls
            int64_t tNowUs = esp_timer_get_time();
            int64_t tElapsedUs = tNowUs - tLastLoopUs;
            tLastLoopUs = tNowUs;

            // Track time between battery reads
            if((NULL == cSwadgeMode->fnAudioCallback) &&
               (NULL != cSwadgeMode->fnBatteryCallback))
            {
                static uint64_t tAccumBatt = 10000000;
                tAccumBatt += tElapsedUs;
                // Read every 10s
                while(tAccumBatt >= 10000000)
                {
                    tAccumBatt -= 10000000;
                    cSwadgeMode->fnBatteryCallback(oneshot_adc_read());
                }
            }

            // Track the elapsed time between draw + fnMainLoop() calls
            static uint64_t tAccumDraw = 0;
            tAccumDraw += tElapsedUs;
            if(tAccumDraw >= frameRateUs)
            {
                // Decrement the accumulation
                tAccumDraw -= frameRateUs;

                // Call the mode's main loop
                if(NULL != cSwadgeMode->fnMainLoop)
                {
                    // Keep track of the time between main loop calls
                    static uint64_t tLastMainLoopCall = 0;
                    if(0 != tLastMainLoopCall)
                    {
                        cSwadgeMode->fnMainLoop(tNowUs - tLastMainLoopCall);
                    }
                    tLastMainLoopCall = tNowUs;
                }

                // If start & select  being held
                if(0 != time_exit_pressed)
                {
                    // Figure out for how long
                    int64_t tHeldUs = tNowUs - time_exit_pressed;
                    // If it has been held for more than the exit time
                    if(tHeldUs > EXIT_TIME_US)
                    {
                        // exit
                        switchToSwadgeMode(&modeMainMenu);
                    }
                    else
                    {
                        // Draw 'progress' bar for exiting
                        int16_t numPx = (tHeldUs * tftDisp.w) / EXIT_TIME_US;
                        fillDisplayArea(&tftDisp, 0, tftDisp.h - 10, numPx, tftDisp.h, c333);
                    }
                }

                // Draw the display at the given frame rate
#ifdef OLED_ENABLED
                oledDisp.drawDisplay(&oledDisp, true, cSwadgeMode->fnBackgroundDrawCallback);
#endif
                tftDisp.drawDisplay(&tftDisp, true, cSwadgeMode->fnBackgroundDrawCallback);
            }
#if defined(EMU)
            check_esp_timer(tElapsedUs);
#endif
        }

#if defined(EMU)
        // EMU needs to check this, actual hardware has an ISR
        buzzer_check_next_note();
#endif

        /* If the mode should be switched, do it now */
        if(NULL != pendingSwadgeMode)
        {
#if defined(EMU)
            int force_soft = 1;
#else
            int force_soft = 0;
#endif
            if( force_soft || isSandboxMode )
            {
                // Exit the current mode
                if(NULL != cSwadgeMode->fnExitMode)
                {
                    cSwadgeMode->fnExitMode();
                }

                buzzer_stop();

                // Switch the mode IDX
                cSwadgeMode = pendingSwadgeMode;
                pendingSwadgeMode = NULL;

                // Enter the next mode
                if(NULL != cSwadgeMode->fnEnterMode)
                {
                    cSwadgeMode->fnEnterMode(&tftDisp);
                }
            }
            else
            {
                // Deep sleep, wake up, and switch to pendingSwadgeMode

                // We have to do this otherwise the backlight can glitch
                disableTFTBacklight();

#ifndef EMU
                // Prevent bootloader on reboot if rebooting from originally bootloaded instance
                REG_WRITE(RTC_CNTL_OPTION1_REG, 0);

                // Only an issue if originally coming from bootloader.  This is actually a ROM function.
                // It prevents the USB from glitching out on the reboot after the reboot after coming
                // out of bootloader
                void chip_usb_set_persist_flags(uint32_t flags);
                chip_usb_set_persist_flags(1 << 31); // USBDC_PERSIST_ENA
#endif
                esp_sleep_enable_timer_wakeup(1);
                esp_deep_sleep_start();
            }
        }

        // Yield to let the rest of the RTOS run
        taskYIELD();
        // Note, the RTOS tick rate can be changed in idf.py menuconfig
        // (100hz by default)
    }

    if(NULL != cSwadgeMode->fnAudioCallback)
    {
        continuous_adc_stop();
        continuous_adc_deinit();
    }

    if(NULL != cSwadgeMode->fnExitMode)
    {
        cSwadgeMode->fnExitMode();
    }

#if defined(EMU)
    esp_timer_deinit();
#endif
}

/**
 * Set up variables to synchronously switch the swadge mode in the main loop
 *
 * @param mode The index of the mode to switch to
 */
void switchToSwadgeMode(swadgeMode* mode)
{
    pendingSwadgeMode = mode;
    isSandboxMode = false;
}

/**
 * Use a pointer to a swadge mode to jump to a dynamic swadge mode.
 *
 * @param mode Pointer to a valid swadgeMode struct.
 */
void overrideToSwadgeMode( swadgeMode* mode )
{
    pendingSwadgeMode = mode;
    isSandboxMode = true;
}

/**
 * Set the frame rate for all displays
 *
 * @param frameRate The time to wait between drawing frames, in microseconds
 */
void setFrameRateUs(uint32_t frameRate)
{
    frameRateUs = frameRate;
}
