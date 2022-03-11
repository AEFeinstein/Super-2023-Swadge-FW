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
#include "esp_log.h"
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
#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

/* Bit number used to represent command and parameter */
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

/* Screen-specific configurations */
#if defined(CONFIG_ST7735_160x80)
    #define TFT_WIDTH         160
    #define TFT_HEIGHT         80
    #define LCD_PIXEL_CLOCK_HZ (40 * 1000*1000)
    #define X_OFFSET            1
    #define Y_OFFSET           26
    #define SWAP_XY          true
    #define MIRROR_X        false
    #define MIRROR_Y         true
#elif defined(CONFIG_ST7789_240x135)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        135
    #define LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
    #define X_OFFSET           40
    #define Y_OFFSET           52
    #define SWAP_XY          true
    #define MIRROR_X        false
    #define MIRROR_Y         true
#elif defined(CONFIG_ST7789_240x240)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        240
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
// Prototypes
//==============================================================================

void setPxTft(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t getPxTft(int16_t x, int16_t y);
void clearPxTft(void);
void drawDisplayTft(bool drawDiff);

//==============================================================================
// Variables
//==============================================================================

static esp_lcd_panel_handle_t panel_handle = NULL;
static rgba_pixel_disp_t * pixels = NULL
static uint16_t *s_lines[2] = {0};
// static uint64_t tFpsStart = 0;
// static int framesDrawn = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize a TFT display and return it through a pointer arg
 * 
 * @param disp    The display to initialize
 * @param spiHost The SPI host to use for this display
 * @param sclk    The GPIO for the SCLK pin
 * @param mosi    The GPIO for the MOSI pin
 * @param dc      The GPIO for the TFT SPI data or command selector pin
 * @param cs      The GPIO for the chip select pin
 * @param rst     The GPIO for the RESET pin
 * @param backlight The GPIO used to PWM control the backlight
 */
void initTFT(display_t * disp, spi_host_device_t spiHost, gpio_num_t sclk,
            gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs, gpio_num_t rst,
            gpio_num_t backlight)
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

    // FIll the handle for the initialized display
    disp->h = TFT_HEIGHT;
    disp->w = TFT_WIDTH;
    disp->setPx = setPxTft;
    disp->getPx = getPxTft;
    disp->clearPx = clearPxTft;
    disp->drawDisplay = drawDisplayTft;

    if(NULL == pixels)
    {
        pixels = malloc(sizeof(rgba_pixel_disp_t) * TFT_HEIGHT * TFT_WIDTH);
    }
}

/**
 * @brief Set a single pixel in the display, with bounds check
 * 
 * TODO handle transparency
 * 
 * @param x The x coordinate of the pixel to set
 * @param y The y coordinate of the pixel to set
 * @param px The color of the pixel to set
 */
void setPxTft(int16_t x, int16_t y, rgba_pixel_t px)
{
    if(0 <= x && x <= TFT_WIDTH && 0 <= y && y < TFT_HEIGHT && PX_OPAQUE == px.a)
    {
        pixels[(y * TFT_WIDTH) + x].px = px;
    }
}

/**
 * @brief Get a single pixel in the display
 * 
 * @param x The x coordinate of the pixel to get
 * @param y The y coordinate of the pixel to get
 * @return rgba_pixel_t The color of the given pixel, or black if out of bounds
 */
rgba_pixel_t getPxTft(int16_t x, int16_t y)
{
    if(0 <= x && x <= TFT_WIDTH && 0 <= y && y < TFT_HEIGHT)
    {
        return pixels[(y * TFT_WIDTH) + x].px;
    }
    rgba_pixel_t black = {.r = 0x00, .g = 0x00, .b = 0x00, .a = PX_OPAQUE};
    return black;
}

/**
 * @brief Clear all pixels in the display to black
 */
void clearPxTft(void)
{
    memset(pixels, 0, sizeof(rgba_pixel_t) * TFT_HEIGHT * TFT_WIDTH);
}

/**
 * @brief Send the current framebuffer to the TFT display over the SPI bus.
 * 
 * This function can be called as quickly as possible and will limit frames to
 * 30fps max
 *
 * Because the SPI driver handles transactions in the background, we can
 * calculate the next line while the previous one is being sent.
 * 
 * @param drawDiff unused
 */
void drawDisplayTft(bool drawDiff __attribute__((unused)))
{
    // Limit drawing to 30fps
    static uint64_t tLastDraw = 0;
    uint64_t tNow = esp_timer_get_time();
    if (tNow - tLastDraw > 33333)
    {
        tLastDraw = tNow;

        // Indexes of the line currently being sent to the LCD and the line we're calculating
        uint8_t sending_line = 0;
        uint8_t calc_line = 0;

        // Send the frame, ping ponging the send buffer
        for (uint16_t y = 0; y < TFT_HEIGHT; y += PARALLEL_LINES)
        {
            // Calculate a line
            uint16_t destIdx = 0;
            for (uint16_t yp = y; yp < y + PARALLEL_LINES; yp++)
            {
                for (uint16_t x = 0; x < TFT_WIDTH; x++)
                {
                    s_lines[calc_line][destIdx++] = SWAP(pixels[(yp * TFT_WIDTH) + x].val);
                }
            }

            sending_line = calc_line;
            calc_line = !calc_line;

            // Send the calculated data
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y,
                                      TFT_WIDTH, y + PARALLEL_LINES,
                                      s_lines[sending_line]);
        }

        // Debug printing for frames-per-second
        // framesDrawn++;
        // if (framesDrawn == 120)
        // {
        //     uint64_t tFpsEnd = esp_timer_get_time();
        //     ESP_LOGD("TFT", "%f FPS", 120 / ((tFpsEnd - tFpsStart) / 1000000.0f));
        //     tFpsStart = tFpsEnd;
        //     framesDrawn = 0;
        // }
    }
}
