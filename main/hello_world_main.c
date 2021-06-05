/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "hal/gpio_types.h"
#include "driver/rmt.h"

#include "led.h"
#include "btn.h"
#include "i2c.h"
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

    led_strip_t* leds = initLeds(GPIO_NUM_19, RMT_CHANNEL_0, 6);

    initButtons();

    i2c_master_init();
    initOLED(true);

    for(int16_t w = 0; w < OLED_WIDTH; w++)
    {
        for(int16_t h = 0; h < OLED_HEIGHT; h++)
        {
            if((w % 2) == (h % 2))
            {
                drawPixel(w, h, WHITE);
            }
        }
    }

    uint16_t hue = 0;
    while(1)
    {
        for(int i = 0; i < 6; i++)
        {
            uint16_t tmpHue = (hue + (60 * i)) % 360;
            uint32_t r, g, b;
            led_strip_hsv2rgb(tmpHue, 100, 3, &r, &g, &b);
            ESP_ERROR_CHECK(leds->set_pixel(leds, i, r, g, b));
        }
        ESP_ERROR_CHECK(leds->refresh(leds, 100));

        hue = (hue + 1) % 360;

        updateOLED(true);

        vTaskDelay(10 / portTICK_RATE_MS);
    }

    // for (int i = 10; i >= 0; i--) {
    //     printf("Restarting in %d seconds...\n", i);
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
    // printf("Restarting now.\n");
    // fflush(stdout);
    // esp_restart();
}
