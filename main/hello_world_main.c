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
#include "i2c-conf.h"
#include "ssd1306.h"

#include "QMA6981.h"

#include "musical_buzzer.h"

#include "nvs_manager.h"

#include "espNowUtils.h"
#include "p2pConnection.h"

#include "swadgeMode.h"
#include "mode_snake.h"

#define MAIN_DEBUG_PRINT
#ifdef MAIN_DEBUG_PRINT
    #define main_printf(...) printf(__VA_ARGS__)
#else
    #define main_printf(...)
#endif

p2pInfo p;

/**
 * @brief Musical Notation: Beethoven's Ode to joy
 */
// static const musicalNote_t notation[] =
// {
//     {740, 400}, {740, 600}, {784, 400}, {880, 400},
//     // {880, 400}, {784, 400}, {740, 400}, {659, 400},
//     // {587, 400}, {587, 400}, {659, 400}, {740, 400},
//     // {740, 400}, {740, 200}, {659, 200}, {659, 800},

//     // {740, 400}, {740, 600}, {784, 400}, {880, 400},
//     // {880, 400}, {784, 400}, {740, 400}, {659, 400},
//     // {587, 400}, {587, 400}, {659, 400}, {740, 400},
//     // {659, 400}, {659, 200}, {587, 200}, {587, 800},

//     // {659, 400}, {659, 400}, {740, 400}, {587, 400},
//     // {659, 400}, {740, 200}, {784, 200}, {740, 400}, {587, 400},
//     // {659, 400}, {740, 200}, {784, 200}, {740, 400}, {659, 400},
//     // {587, 400}, {659, 400}, {440, 400}, {440, 400},

//     // {740, 400}, {740, 600}, {784, 400}, {880, 400},
//     // {880, 400}, {784, 400}, {740, 400}, {659, 400},
//     // {587, 400}, {587, 400}, {659, 400}, {740, 400},
//     // {659, 400}, {659, 200}, {587, 200}, {587, 800},
// };

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

void app_main(void)
{
    main_printf("Hello world!\n");

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

    esp_timer_init();

    initLeds();

    initButtons();

    i2c_master_init();

    initOLED(true);
    clearDisplay();

    QMA6981_setup();

    // buzzer_init(GPIO_NUM_9, RMT_CHANNEL_1);
    // buzzer_play(notation, sizeof(notation) / sizeof(notation[0]));

    initNvs(true);

#define MAGIC_VAL 0x01
    int32_t magicVal = 0;
    if((false == readNvs32("magicVal", &magicVal)) || (MAGIC_VAL != magicVal))
    {
        main_printf("Writing magic val\n");
        writeNvs32("magicVal", 0x01);
        writeNvs32("testVal", 0x01);
    }

    espNowInit(&swadgeModeEspNowRecvCb, &swadgeModeEspNowSendCb);

    p2pInitialize(&p, "tst", testConCbFn, testMsgRxCbFn, -10);
    p2pStartConnection(&p);

    // led_t leds[NUM_LEDS] = {0};
    // int16_t pxidx = 0;
    // uint16_t hue = 0;

    snakeMode.fnEnterMode();

    while(1)
    {
        checkEspNowRxQueue();

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

        buttonEvt_t bEvt = {0};
        if(checkButtonQueue(&bEvt))
        {
            if(NULL != snakeMode.fnButtonCallback)
            {
                snakeMode.fnButtonCallback(bEvt.state, bEvt.button, bEvt.down);
            }
        }

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

        // // for(int i = 0; i < NUM_LEDS; i++)
        // // {
        // //     uint16_t tmpHue = (hue + (60 * i)) % 360;
        // //     led_strip_hsv2rgb(tmpHue, 100, 3, &leds[i].r, &leds[i].g, &leds[i].b);
        // // }
        // // hue = (hue + 1) % 360;
        // // setLeds(leds, NUM_LEDS);

        // // switch(getPixel(pxidx % OLED_WIDTH, pxidx / OLED_WIDTH))
        // // {
        // //     default:
        // //     case BLACK:
        // //     {
        // //         drawPixel(pxidx % OLED_WIDTH, pxidx / OLED_WIDTH, WHITE);
        // //         break;
        // //     }
        // //     case WHITE:
        // //     {
        // //         drawPixel(pxidx % OLED_WIDTH, pxidx / OLED_WIDTH, BLACK);
        // //         break;
        // //     }
        // // }
        // // pxidx = (pxidx + 1) % (OLED_WIDTH * OLED_HEIGHT);

        updateOLED(true);

        // accel_t accel = {0};
        // QMA6981_poll(&accel);
        // uint64_t cTimeUs = esp_timer_get_time();
        // static uint64_t lastAccelPrint = 0;
        // if(cTimeUs - lastAccelPrint >= 1000000)
        // {
        //     lastAccelPrint = cTimeUs;
        //     main_printf("%4d %4d %4d\n", accel.x, accel.y, accel.z);
        // }
        // if(NULL != snakeMode.fnAccelerometerCallback)
        // {
        //     snakeMode.fnAccelerometerCallback(&accel);
        // }

        // buzzer_check_next_note();

        // usleep(1);
        vTaskDelay(1); // TODO task-ify this loop?
    }
}
