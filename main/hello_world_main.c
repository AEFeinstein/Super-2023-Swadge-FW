/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <unistd.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "hal/gpio_types.h"
#include "driver/rmt.h"
#include "esp_timer.h"
#include "esp_efuse.h"

#include "led_util.h"
#include "btn.h"
#include "touch_sensor.h"
#include "i2c-conf.h"
#include "ssd1306.h"

#include "QMA6981.h"

#include "musical_buzzer.h"

#include "esp_temperature_sensor.h"

#include "tinyusb.h"
#include "tusb_hid_gamepad.h"

#include "nvs_manager.h"
#include "spiffs_manager.h"

#include "espNowUtils.h"
#include "p2pConnection.h"

#include "swadgeMode.h"
#include "mode_snake.h"

/******************************************************************************/
// Defines
/******************************************************************************/

// #define TEST_ACCEL
// #define TEST_BZR
// #define TEST_ESP_NOW
// #define TEST_LEDS
// #define TEST_NVR
// #define TEST_OLED
#define TEST_SPIFFS
// #define TEST_TEMPERATURE

#define MAIN_DEBUG_PRINT
#ifdef MAIN_DEBUG_PRINT
#define main_printf(...) printf(__VA_ARGS__)
#else
#define main_printf(...)
#endif

/******************************************************************************/
// Function Prototypes
/******************************************************************************/
void mainSwadgeTask(void * arg);

/******************************************************************************/
// Variables
/******************************************************************************/

#ifdef TEST_BZR
/**
 * @brief Musical Notation: Beethoven's Ode to joy
 */
static const musicalNote_t notation[] =
{
    {740, 400}, {740, 600}, {784, 400}, {880, 400},
    {880, 400}, {784, 400}, {740, 400}, {659, 400},
    {587, 400}, {587, 400}, {659, 400}, {740, 400},
    {740, 400}, {740, 200}, {659, 200}, {659, 800},

    {740, 400}, {740, 600}, {784, 400}, {880, 400},
    {880, 400}, {784, 400}, {740, 400}, {659, 400},
    {587, 400}, {587, 400}, {659, 400}, {740, 400},
    {659, 400}, {659, 200}, {587, 200}, {587, 800},

    {659, 400}, {659, 400}, {740, 400}, {587, 400},
    {659, 400}, {740, 200}, {784, 200}, {740, 400}, {587, 400},
    {659, 400}, {740, 200}, {784, 200}, {740, 400}, {659, 400},
    {587, 400}, {659, 400}, {440, 400}, {440, 400},

    {740, 400}, {740, 600}, {784, 400}, {880, 400},
    {880, 400}, {784, 400}, {740, 400}, {659, 400},
    {587, 400}, {587, 400}, {659, 400}, {740, 400},
    {659, 400}, {659, 200}, {587, 200}, {587, 800},
};
#endif

/******************************************************************************/
// Functions
/******************************************************************************/

#ifdef TEST_ESP_NOW
p2pInfo p;

void testConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    main_printf("%s :: %d\n", __func__, evt);
}

void testMsgRxCbFn(p2pInfo* p2p, const char* msg, const uint8_t* payload, uint8_t len)
{
    main_printf("%s :: %d\n", __func__, len);
}

void testMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    main_printf("%s :: %d\n", __func__, status);
}

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is received
 * It routes through user_main.c, which knows what the current mode is
 */
void swadgeModeEspNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, uint8_t len, int8_t rssi)
{
    p2pRecvCb(&p, mac_addr, data, len, rssi);
}

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is sent
 * It routes through user_main.c, which knows what the current mode is
 */
void swadgeModeEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    p2pSendCb(&p, mac_addr, status);
}
#endif

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
    main_printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
                CONFIG_IDF_TARGET,
                chip_info.cores,
                (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    main_printf("silicon revision %d, ", chip_info.revision);

    main_printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    main_printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
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
    xTaskCreate(mainSwadgeTask, "SWADGE", 4096, NULL, tskIDLE_PRIORITY /*configMAX_PRIORITIES / 2*/, xHandle);
}

/**
 * @brief TODO
 *
 * @param itf
 * @param report_id
 * @param report_type
 * @param buffer
 * @param reqlen
 * @return uint16_t
 */
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t* buffer,
                               uint16_t reqlen)
{
    // TODO figure it out
    return 0;
}

/**
 * @brief TODO
 *
 * @param itf
 * @param report_id
 * @param report_type
 * @param buffer
 * @param bufsize
 */
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize)
{
    // TODO figure it out
}

/**
 * This is the task for the swadge. It sets up all peripherals and runs the
 * firmware in a while(1) loop
 */
void mainSwadgeTask(void * arg)
{
    /* Initialize internal NVS */
    initNvs(true);

    /* Initialize SPIFFS */
    initSpiffs();

    /* Initialize non-i2c hardware peripherals */
    initButtons(); // TODO GPIOs in args somehow
    initLeds   (GPIO_NUM_8, RMT_CHANNEL_0, NUM_LEDS);
    buzzer_init(GPIO_NUM_9, RMT_CHANNEL_1);
    initTemperatureSensor();

    touch_pad_t touchPads[] = {TOUCH_PAD_NUM12};
    float touchPadSensitivities[] = {0.2f};
    initTouchSensor(sizeof(touchPads) / sizeof(touchPads[0]), touchPads,
        touchPadSensitivities, true, TOUCH_PAD_MAX);

    /* Initialize i2c peripherals */
#define I2C_ENABLED
#ifdef I2C_ENABLED
    i2c_master_init(GPIO_NUM_5, GPIO_NUM_6, GPIO_PULLUP_DISABLE, 1000000);
    initOLED(true); // TODO reset GPIO in arg?
    QMA6981_setup();
#endif

    /* the configuration using default values */
    tinyusb_config_t tusb_cfg = {};
    /* This calls tusb_init() and sets up a task to spin tud_task() */
    tinyusb_driver_install(&tusb_cfg);

    /*************************
     * One-time tests
     ************************/
#ifdef TEST_BZR
    buzzer_play(notation, sizeof(notation) / sizeof(notation[0]));
#endif

#ifdef TEST_NVR
#define MAGIC_VAL 0x01
    int32_t magicVal = 0;
    if((false == readNvs32("magicVal", &magicVal)) || (MAGIC_VAL != magicVal))
    {
        main_printf("Writing magic val\n");
        writeNvs32("magicVal", 0x01);
        writeNvs32("testVal", 0x01);
    }
#endif

#ifdef TEST_ESP_NOW
    espNowInit(&swadgeModeEspNowRecvCb, &swadgeModeEspNowSendCb);
    p2pInitialize(&p, "tst", testConCbFn, testMsgRxCbFn, -10);
    p2pStartConnection(&p);
#endif

#ifdef TEST_LEDS
    led_t leds[NUM_LEDS] = {0};
    int16_t pxidx = 0;
    uint16_t hue = 0;
#endif

#ifdef TEST_SPIFFS
    spiffsTest();
#endif

    /* Enter the swadge mode */
    snakeMode.fnEnterMode();

    /* Loop forever! */
    while(1)
    {
        // Print the word "loop" every five seconds
        uint64_t dbgTimeUs = esp_timer_get_time();
        static uint64_t lastDbg = 0;
        if(dbgTimeUs - lastDbg >= 5000000)
        {
            lastDbg = dbgTimeUs;
            main_printf("loop\n");
        }

        /* Twice a second push out some USB data */
        uint64_t usbTimeUs = esp_timer_get_time();
        static uint64_t lastUsb = 0;
        /* Don't send until USB is ready */
        if(tud_ready() && usbTimeUs - lastUsb >= 500000)
        {
            lastUsb = usbTimeUs;

            // Static variables to track button and hat position
            static hid_gamepad_button_bm_t btnPressed = GAMEPAD_BUTTON_A;
            static hid_gamepad_hat_t hatDir = GAMEPAD_HAT_CENTERED;

            // Build and send the state over USB
            hid_gamepad_report_t report =
            {
                .buttons = btnPressed,
                .hat = hatDir
            };
            tud_gamepad_report(&report);

            // Move to the next button
            btnPressed <<= 1;
            if(btnPressed > GAMEPAD_BUTTON_THUMBR)
            {
                btnPressed = GAMEPAD_BUTTON_A;
            }

            // Move to the next hat dir
            hatDir++;
            if(hatDir > GAMEPAD_HAT_UP_LEFT)
            {
                hatDir = GAMEPAD_HAT_CENTERED;
            }
        }

        /*************************
         * Looped tests
         ************************/
#ifdef TEST_TEMPERATURE
        uint64_t tempTimeUs = esp_timer_get_time();
        static uint64_t lastTmp = 0;
        if(tempTimeUs - lastTmp >= 1000000)
        {
            lastTmp = tempTimeUs;
            main_printf("temperature %fc\n", readTemperatureSensor());
        }
#endif

#ifdef TEST_ESP_NOW
        checkEspNowRxQueue();
#endif

#ifdef TEST_NVR
        int32_t write = 0xAEF;
        int32_t read = 0;
        if(readNvs32("testVal", &read))
        {
            if (read != write)
            {
                main_printf("nvs read  %04x\n", read);
                main_printf("nvs write %04x\n", write);
                writeNvs32("testVal", write);
            }
        }
#endif

#ifdef TEST_LEDS
        for(int i = 0; i < NUM_LEDS; i++)
        {
            uint16_t tmpHue = (hue + (60 * i)) % 360;
            led_strip_hsv2rgb(tmpHue, 100, 3, &leds[i].r, &leds[i].g, &leds[i].b);
        }
        hue = (hue + 1) % 360;
        setLeds(leds, NUM_LEDS);
#endif

#ifdef TEST_OLED
        switch(getPixel(pxidx % OLED_WIDTH, pxidx / OLED_WIDTH))
        {
        default:
        case BLACK:
        {
            drawPixel(pxidx % OLED_WIDTH, pxidx / OLED_WIDTH, WHITE);
            break;
        }
        case WHITE:
        {
            drawPixel(pxidx % OLED_WIDTH, pxidx / OLED_WIDTH, BLACK);
            break;
        }
        }
        pxidx = (pxidx + 1) % (OLED_WIDTH * OLED_HEIGHT);
#endif

#if defined(I2C_ENABLED) && defined(TEST_ACCEL)
        accel_t accel = {0};
        QMA6981_poll(&accel);
        uint64_t cTimeUs = esp_timer_get_time();
        static uint64_t lastAccelPrint = 0;
        if(cTimeUs - lastAccelPrint >= 1000000)
        {
            lastAccelPrint = cTimeUs;
            main_printf("%4d %4d %4d\n", accel.x, accel.y, accel.z);
        }
        if(NULL != snakeMode.fnAccelerometerCallback)
        {
            snakeMode.fnAccelerometerCallback(&accel);
        }
#endif

        // Process button presses
        buttonEvt_t bEvt = {0};
        if(checkButtonQueue(&bEvt))
        {
            if(NULL != snakeMode.fnButtonCallback)
            {
                snakeMode.fnButtonCallback(bEvt.state, bEvt.button, bEvt.down);
            }
        }

        checkTouchSensor();

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

            if(NULL != snakeMode.fnMainLoop)
            {
                snakeMode.fnMainLoop(tElapsedUs);
            }
        }

        // Update outputs
#ifdef I2C_ENABLED
        updateOLED(true);
#endif
        buzzer_check_next_note();

        // Yield to let the rest of the RTOS run
        taskYIELD();
        // TODO note, the RTOS tick rate can be changed in idf.py menuconfig (100hz by default)
    }
}
