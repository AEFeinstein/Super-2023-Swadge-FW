//==============================================================================
// Includes
//==============================================================================

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
#include "hdw-tft.h"

//==============================================================================
// Defines
//==============================================================================

#define SWAP(x) ((x>>8)|(x<<8))

/* To speed up transfers, every SPI transfer sends a bunch of lines. This define
 * specifies how many. More means more memory use, but less overhead for setting
 * up and finishing transfers. Make sure TFT_HEIGHT is dividable by this.
 */
#define PARALLEL_LINES 16

/* Backlight levels */
#define LCD_BK_LIGHT_ON_LEVEL  0
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

/* Bit number used to represent command and parameter */
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

/* Screen-specific configurations */
#if defined(CONFIG_ST7735_160x80)
	#define LCD_PIXEL_CLOCK_HZ (40 * 1000*1000)
	#define X_OFFSET            1
	#define Y_OFFSET           26
	#define SWAP_XY          true
	#define MIRROR_X        false
	#define MIRROR_Y         true
#elif defined(CONFIG_ST7789_240x135)
	#define LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
	#define X_OFFSET           40
	#define Y_OFFSET           52
	#define SWAP_XY          true
	#define MIRROR_X        false
	#define MIRROR_Y         true
#elif defined(CONFIG_ST7789_240x240)
	#define LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
	#define X_OFFSET            0
	#define Y_OFFSET            0
	#define SWAP_XY         false
	#define MIRROR_X        false
	#define MIRROR_Y        false
#else
	#error "Please pick a screen size"
#endif

//==============================================================================
// Variables
//==============================================================================

esp_lcd_panel_handle_t panel_handle = NULL;
tft_pixel_t pixels[TFT_HEIGHT][TFT_WIDTH] = {0};
static uint16_t *s_lines[2] = {0};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 * @param h
 * @param s
 * @param v
 * @param px
 */
void hsv2rgb(uint16_t h, float s, float v, tft_pixel_t *px)
{
    float hh, p, q, t, ff;
    uint16_t i;

    hh = (h % 360) / 60.0f;
    i = (uint16_t)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch (i)
    {
		case 0:
		{
			px->c.r = v * 0x1F;
			px->c.g = t * 0x3F;
			px->c.b = p * 0x1F;
			break;
		}
		case 1:
		{
			px->c.r = q * 0x1F;
			px->c.g = v * 0x3F;
			px->c.b = p * 0x1F;
			break;
		}
		case 2:
		{
			px->c.r = p * 0x1F;
			px->c.g = v * 0x3F;
			px->c.b = t * 0x1F;
			break;
		}
		case 3:
		{
			px->c.r = p * 0x1F;
			px->c.g = q * 0x3F;
			px->c.b = v * 0x1F;
			break;
		}
		case 4:
		{
			px->c.r = t * 0x1F;
			px->c.g = p * 0x3F;
			px->c.b = v * 0x1F;
			break;
		}
		case 5:
		default:
		{
			px->c.r = v * 0x1F;
			px->c.g = p * 0x3F;
			px->c.b = q * 0x1F;
			break;
		}
    }
}

/**
 * @brief TODO
 *
 * @param spiHost
 * @param sclk
 * @param mosi
 * @param dc
 * @param cs
 * @param rst
 * @param backlight
 */
void initTFT(spi_host_device_t spiHost, gpio_num_t sclk, gpio_num_t mosi,
             gpio_num_t dc, gpio_num_t cs, gpio_num_t rst, gpio_num_t backlight)
{
    gpio_config_t bk_gpio_config =
    {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << backlight
    };
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    spi_bus_config_t buscfg =
    {
        .sclk_io_num = sclk,
        .mosi_io_num = mosi,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * TFT_WIDTH * 2 + 8
    };

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(spiHost, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config =
    {
        .dc_gpio_num = dc,
        .cs_gpio_num = cs,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = ESP_IMAGE_SPI_MODE_QIO,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spiHost, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config =
    {
        .reset_gpio_num = rst,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    // Initialize the LCD configuration

#if defined(CONFIG_ST7735_160x80)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle));
#elif defined(CONFIG_ST7789_240x135) || defined(CONFIG_ST7789_240x240)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
#else
	#error "Please pick a screen size"
#endif

    // Turn off backlight to avoid unpredictable display on the LCD screen while initializing
    // the LCD panel driver. (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(backlight, LCD_BK_LIGHT_OFF_LEVEL));

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Turn on backlight (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(backlight, LCD_BK_LIGHT_ON_LEVEL));

    // Allocate memory for the pixel buffers
    for (int i = 0; i < 2; i++)
    {
        s_lines[i] = heap_caps_malloc(TFT_WIDTH * PARALLEL_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
        assert(s_lines[i] != NULL);
    }

    // Config the TFT
    esp_lcd_panel_swap_xy(panel_handle, SWAP_XY);
    esp_lcd_panel_mirror(panel_handle, MIRROR_X, MIRROR_Y);
    esp_lcd_panel_set_gap(panel_handle, X_OFFSET, Y_OFFSET);
    esp_lcd_panel_invert_color(panel_handle, true);

    for (int y = 0; y < TFT_HEIGHT; y++)
    {
        for (int x = 0; x < TFT_WIDTH; x++)
        {
            hsv2rgb((x * 360) / TFT_WIDTH, y / (float)(TFT_HEIGHT-1), 1, &pixels[y][x]);
        }
    }
}

/**
 * @brief TODO
 *
 * Simple routine to generate some patterns and send them to the LCD. Because
 * the SPI driver handles transactions in the background, we can calculate the
 * next line while the previous one is being sent.
 */
void draw_frame(void)
{
    static int framesDrawn = 0;
    static uint64_t tFpsStart = 0;
    if (0 == tFpsStart)
    {
        tFpsStart = esp_timer_get_time();
    }

    // Limit drawing to 60fps
    static uint64_t tLastDraw = 0;
    uint64_t tNow = esp_timer_get_time();
    if (tNow - tLastDraw > 16666)
    {
        tLastDraw = tNow;

        // Indexes of the line currently being sent to the LCD and the line we're calculating
        int sending_line = 0;
        int calc_line = 0;

		static int xOffset=0, yOffset=0;

        // Send the frame, ping ponging the send buffer
        for (int y = 0; y < TFT_HEIGHT; y += PARALLEL_LINES)
        {
            // Calculate a line
            int destIdx = 0;
            for (int yp = y; yp < y + PARALLEL_LINES; yp++)
            {
                for (int x = 0; x < TFT_WIDTH; x++)
                {
                    s_lines[calc_line][destIdx++] = SWAP(pixels[(yp + yOffset) % TFT_HEIGHT][(x + xOffset) % TFT_WIDTH].val);
                }
            }

            sending_line = calc_line;
            calc_line = !calc_line;

            // Send the calculated data
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y,
                                      TFT_WIDTH, y + PARALLEL_LINES,
                                      s_lines[sending_line]);
        }

		xOffset = (xOffset + 1) % TFT_WIDTH;
		yOffset = (yOffset + 1) % TFT_HEIGHT;

        framesDrawn++;
        if (framesDrawn == 120)
        {
            uint64_t tFpsEnd = esp_timer_get_time();
            printf("%f FPS\n", 120 / ((tFpsEnd - tFpsStart) / 1000000.0f));
            tFpsStart = tFpsEnd;
            framesDrawn = 0;
        }
    }
}
