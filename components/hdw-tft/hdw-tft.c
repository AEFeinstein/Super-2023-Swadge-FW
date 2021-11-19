/* LCD tjpgd example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_app_format.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lcd_conf.h"
#include "hdw-tft.h"

esp_lcd_panel_handle_t panel_handle = NULL;
tft_pixel_t pixels[EXAMPLE_LCD_V_RES][EXAMPLE_LCD_H_RES] = {0};
static uint16_t *s_lines[2] = {0};

/**
 * @brief TODO
 *
 */
void initTFT(gpio_num_t sclk, gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs,
             gpio_num_t rst, gpio_num_t backlight)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << backlight
    };
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    spi_bus_config_t buscfg = {
        .sclk_io_num = sclk,
        .mosi_io_num = mosi,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * EXAMPLE_LCD_H_RES * 2 + 8
    };

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = dc,
        .cs_gpio_num = cs,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = ESP_IMAGE_SPI_MODE_QIO,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = rst,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    // Initialize the LCD configuration

#if defined(_0_96)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle));
#elif defined(_1_14) || defined(_1_3)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
#else
#error "Please pick a screen size"
#endif

    // Turn off backlight to avoid unpredictable display on the LCD screen while initializing
    // the LCD panel driver. (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(backlight, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL));

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Turn on backlight (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(backlight, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL));

    // Allocate memory for the pixel buffers
    for (int i = 0; i < 2; i++) {
        s_lines[i] = heap_caps_malloc(EXAMPLE_LCD_H_RES * PARALLEL_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
        assert(s_lines[i] != NULL);
    }

    // Config the TFT
    esp_lcd_panel_swap_xy(panel_handle, SWAP_XY);
    esp_lcd_panel_mirror(panel_handle, MIRROR_X, MIRROR_Y);
    esp_lcd_panel_set_gap(panel_handle, X_OFFSET, Y_OFFSET);
    esp_lcd_panel_invert_color(panel_handle, true);

    printf("sizeof: %d\n", sizeof(tft_pixel_t));

    for(int y = 0; y < EXAMPLE_LCD_V_RES; y++) {
        for(int x = 0; x < EXAMPLE_LCD_H_RES; x++) {
            if(x < EXAMPLE_LCD_H_RES / 3) {
                pixels[y][x].c.r = 0x1F;
                pixels[y][x].c.g = 0x00;
                pixels[y][x].c.b = 0x00;
            }
            else if (x < (2*EXAMPLE_LCD_H_RES)/3) {
                pixels[y][x].c.r = 0x00;
                pixels[y][x].c.g = 0x3F;
                pixels[y][x].c.b = 0x00;
            }
            else {
                pixels[y][x].c.r = 0x00;
                pixels[y][x].c.g = 0x00;
                pixels[y][x].c.b = 0x1F;
            }
        }
    }
}

// Simple routine to generate some patterns and send them to the LCD. Because the
// SPI driver handles transactions in the background, we can calculate the next line
// while the previous one is being sent.
void draw_frame(void)
{
    static int framesDrawn = 0;
    static uint64_t tFpsStart = 0;
    if(0 == tFpsStart)
    {
        tFpsStart = esp_timer_get_time();
    }

    // Limit drawing to 60fps
    static uint64_t tLastDraw = 0;
    uint64_t tNow = esp_timer_get_time();
    if(tNow - tLastDraw > 16666)
    {
        tLastDraw = tNow;

        // Indexes of the line currently being sent to the LCD and the line we're calculating
        int sending_line = 0;
        int calc_line = 0;

        // Send the frame, ping ponging the send buffer
        for (int y = 0; y < EXAMPLE_LCD_V_RES; y += PARALLEL_LINES) {
            // Calculate a line
            int destIdx = 0;
            for (int yp = y; yp < y + PARALLEL_LINES; yp++) {
                for (int x = 0; x < EXAMPLE_LCD_H_RES; x++) {
#define SWAP(x) ((x>>8)|(x<<8))
                    s_lines[calc_line][destIdx++] = SWAP(pixels[yp][x].val);
                }
            }

            sending_line = calc_line;
            calc_line = !calc_line;

            // Send the calculated data
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 0 + EXAMPLE_LCD_H_RES, y + PARALLEL_LINES, s_lines[sending_line]);
        }

        framesDrawn++;
        if(framesDrawn == 120)
        {
            uint64_t tFpsEnd = esp_timer_get_time();
            printf("%f FPS\n", 120 / ((tFpsEnd - tFpsStart) / 1000000.0f));
            tFpsStart = tFpsEnd;
            framesDrawn = 0;
        }
    }
}
