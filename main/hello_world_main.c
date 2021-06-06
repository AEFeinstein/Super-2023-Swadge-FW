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

#include "led_util.h"
#include "btn.h"
#include "i2c-conf.h"
#include "ssd1306.h"

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    initLeds();
    led_t leds[NUM_LEDS] = {0};

    initButtons();

    i2c_master_init();
    initOLED(true);
    clearDisplay();

    int16_t pxidx = 0;

    uint16_t hue = 0;
    while(1)
    {
        for(int i = 0; i < NUM_LEDS; i++)
        {
            uint16_t tmpHue = (hue + (60 * i)) % 360;
            led_strip_hsv2rgb(tmpHue, 100, 3, &leds[i].r, &leds[i].g, &leds[i].b);
        }
        hue = (hue + 1) % 360;
        setLeds(leds, NUM_LEDS);

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

        updateOLED(true);

        usleep(1);
    }
}
