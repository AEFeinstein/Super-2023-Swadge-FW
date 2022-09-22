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
#include "esp_lcd_panel_interface.h"
#include "driver/ledc.h"


//#define PROCPROFILE

#ifdef PROCPROFILE
void uart_tx_one_char( char c );
static inline uint32_t get_ccount()
{
    uint32_t ccount;
    asm volatile("rsr %0,ccount":"=a" (ccount));
    return ccount;
}
#endif

//==============================================================================
// Colors
//==============================================================================

const uint16_t paletteColors[] =
{
    0x0000,
    0x0600,
    0x0C00,
    0x1200,
    0x1800,
    0x1F00,
    0x8001,
    0x8601,
    0x8C01,
    0x9201,
    0x9801,
    0x9F01,
    0x0003,
    0x0603,
    0x0C03,
    0x1203,
    0x1803,
    0x1F03,
    0x8004,
    0x8604,
    0x8C04,
    0x9204,
    0x9804,
    0x9F04,
    0x0006,
    0x0606,
    0x0C06,
    0x1206,
    0x1806,
    0x1F06,
    0xC007,
    0xC607,
    0xCC07,
    0xD207,
    0xD807,
    0xDF07,
    0x0030,
    0x0630,
    0x0C30,
    0x1230,
    0x1830,
    0x1F30,
    0x8031,
    0x8631,
    0x8C31,
    0x9231,
    0x9831,
    0x9F31,
    0x0033,
    0x0633,
    0x0C33,
    0x1233,
    0x1833,
    0x1F33,
    0x8034,
    0x8634,
    0x8C34,
    0x9234,
    0x9834,
    0x9F34,
    0x0036,
    0x0636,
    0x0C36,
    0x1236,
    0x1836,
    0x1F36,
    0xC037,
    0xC637,
    0xCC37,
    0xD237,
    0xD837,
    0xDF37,
    0x0060,
    0x0660,
    0x0C60,
    0x1260,
    0x1860,
    0x1F60,
    0x8061,
    0x8661,
    0x8C61,
    0x9261,
    0x9861,
    0x9F61,
    0x0063,
    0x0663,
    0x0C63,
    0x1263,
    0x1863,
    0x1F63,
    0x8064,
    0x8664,
    0x8C64,
    0x9264,
    0x9864,
    0x9F64,
    0x0066,
    0x0666,
    0x0C66,
    0x1266,
    0x1866,
    0x1F66,
    0xC067,
    0xC667,
    0xCC67,
    0xD267,
    0xD867,
    0xDF67,
    0x0090,
    0x0690,
    0x0C90,
    0x1290,
    0x1890,
    0x1F90,
    0x8091,
    0x8691,
    0x8C91,
    0x9291,
    0x9891,
    0x9F91,
    0x0093,
    0x0693,
    0x0C93,
    0x1293,
    0x1893,
    0x1F93,
    0x8094,
    0x8694,
    0x8C94,
    0x9294,
    0x9894,
    0x9F94,
    0x0096,
    0x0696,
    0x0C96,
    0x1296,
    0x1896,
    0x1F96,
    0xC097,
    0xC697,
    0xCC97,
    0xD297,
    0xD897,
    0xDF97,
    0x00C0,
    0x06C0,
    0x0CC0,
    0x12C0,
    0x18C0,
    0x1FC0,
    0x80C1,
    0x86C1,
    0x8CC1,
    0x92C1,
    0x98C1,
    0x9FC1,
    0x00C3,
    0x06C3,
    0x0CC3,
    0x12C3,
    0x18C3,
    0x1FC3,
    0x80C4,
    0x86C4,
    0x8CC4,
    0x92C4,
    0x98C4,
    0x9FC4,
    0x00C6,
    0x06C6,
    0x0CC6,
    0x12C6,
    0x18C6,
    0x1FC6,
    0xC0C7,
    0xC6C7,
    0xCCC7,
    0xD2C7,
    0xD8C7,
    0xDFC7,
    0x00F8,
    0x06F8,
    0x0CF8,
    0x12F8,
    0x18F8,
    0x1FF8,
    0x80F9,
    0x86F9,
    0x8CF9,
    0x92F9,
    0x98F9,
    0x9FF9,
    0x00FB,
    0x06FB,
    0x0CFB,
    0x12FB,
    0x18FB,
    0x1FFB,
    0x80FC,
    0x86FC,
    0x8CFC,
    0x92FC,
    0x98FC,
    0x9FFC,
    0x00FE,
    0x06FE,
    0x0CFE,
    0x12FE,
    0x18FE,
    0x1FFE,
    0xC0FF,
    0xC6FF,
    0xCCFF,
    0xD2FF,
    0xD8FF,
    0xDFFF,
};

//==============================================================================
// Defines
//==============================================================================

#define SWAP(x) ((x>>8)|(x<<8))

/* To speed up transfers, every SPI transfer sends a bunch of lines. This define
 * specifies how many. More means more memory use, but less overhead for setting
 * up and finishing transfers. Make sure TFT_HEIGHT is dividable by this.
 */
#define PARALLEL_LINES 16

/* Binary backlight levels */
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
#elif defined(CONFIG_ST7735_128x160)
    // Mixture of docs + experimentation
    // This is the RB027D25N05A / RB017D14N05A (Actually the ST7735S, so inbetween a ST7735 and ST7789)
    #define TFT_WIDTH         160
    #define TFT_HEIGHT        128
    #define LCD_PIXEL_CLOCK_HZ (40 * 1000*1000)
    #define X_OFFSET            0
    #define Y_OFFSET            0
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
    #define Y_OFFSET           80
    #define SWAP_XY         false
    #define MIRROR_X         true
    #define MIRROR_Y         true
#elif defined(CONFIG_GC9307_240x280)
    // A beautiful rounded edges LCD RB017A1505A
    #define TFT_WIDTH         280
    #define TFT_HEIGHT        240
    #define LCD_PIXEL_CLOCK_HZ (100 * 1000 * 1000)
    #define X_OFFSET            20
    #define Y_OFFSET            0
    #define SWAP_XY         true
    #define MIRROR_X        true
    #define MIRROR_Y        true
#else
    #error "Please pick a screen size"
#endif

//==============================================================================
// Prototypes
//==============================================================================

void setPxTft(int16_t x, int16_t y, paletteColor_t px);
paletteColor_t getPxTft(int16_t x, int16_t y);
void clearPxTft(void);
void drawDisplayTft(display_t * disp,bool drawDiff,fnBackgroundDrawCallback_t cb);

//==============================================================================
// Variables
//==============================================================================

esp_lcd_panel_handle_t panel_handle = NULL;
static paletteColor_t * pixels = NULL;
static uint16_t *s_lines[2] = {0};
static gpio_num_t tftBacklightPin;

// static uint64_t tFpsStart = 0;
// static int framesDrawn = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Set TFT Backlight brightness.
 *
 * @param intensity    Sets the brightness 0-255
 *
 * @return value is 0 if OK nonzero if error.
 */
int setTFTBacklight(uint8_t intensity)
{
    esp_err_t e;
    if(intensity > CONFIG_TFT_MAX_BRIGHTNESS)
    {
        return ESP_ERR_INVALID_ARG;
    }
    e = ledc_set_duty(LEDC_LOW_SPEED_MODE, 1, 255 - intensity);
    if(e)
    {
        return e;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, 1);
}


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
 * @param isPwmBacklight true to set up the backlight as PWM, false to have it be on/off
 */
void initTFT(display_t* disp, spi_host_device_t spiHost, gpio_num_t sclk,
             gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs, gpio_num_t rst,
             gpio_num_t backlight, bool isPwmBacklight)
{
    tftBacklightPin = backlight;

    if(false == isPwmBacklight)
    {
        // Binary backlight
        gpio_config_t bk_gpio_config =
        {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << backlight
        };
        // Initialize the GPIO of backlight
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    }
    else
    {
        // PWM Backlight
        ledc_timer_config_t ledc_config_timer =
        {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .freq_hz = 50000,
            .timer_num = 0,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_config_timer));
    }

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
#elif defined(CONFIG_ST7789_240x135) || defined(CONFIG_ST7789_240x240) || defined(CONFIG_ST7735_128x160) || defined(CONFIG_GC9307_240x280)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
#else
#error "Please pick a screen size"
#endif

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Turn on backlight (Different LCD screens may need different levels)
    if(false == isPwmBacklight)
    {
        ESP_ERROR_CHECK(gpio_set_level(backlight, LCD_BK_LIGHT_ON_LEVEL));
    }
    else
    {
        ledc_channel_config_t ledc_config_backlight =
        {
            .gpio_num = backlight,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = 1,  //Not sure if 0 is used.
            .timer_sel = 0,
            .duty = 255, //Disable to start.
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_config_backlight));
        setTFTBacklight(CONFIG_TFT_DEFAULT_BRIGHTNESS);
    }

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

#if defined(CONFIG_GC9307_240x280) || defined(CONFIG_ST7735_128x160)
    typedef struct
    {
        esp_lcd_panel_t base;
        esp_lcd_panel_io_handle_t io;
        int reset_gpio_num;
        bool reset_level;
        int x_gap;
        int y_gap;
        unsigned int bits_per_pixel;
        uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
        uint8_t colmod_cal; // save surrent value of LCD_CMD_COLMOD register
    } st7789_panel_internal_t;
    st7789_panel_internal_t* st7789 = __containerof(panel_handle, st7789_panel_internal_t, base);
    esp_lcd_panel_io_handle_t io = st7789->io;
#endif

#if defined(CONFIG_GC9307_240x280)
    esp_lcd_panel_invert_color(panel_handle, false);
    // NOTE: the following call would override settings set by esp_lcd_panel_swap_xy() and esp_lcd_panel_mirror()
    // Both of the prior functions write to the 0x36 register
    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[])
    {
        0xE8
    }, 1 ); //MX, MY, RGB mode  (MADCTL)
    esp_lcd_panel_io_tx_param(io, 0x35, (uint8_t[])
    {
        0x00
    }, 1 ); // "tear effect" testing sync pin.
#elif defined(CONFIG_ST7735_128x160)
    esp_lcd_panel_io_tx_param(io, 0xB1, (uint8_t[])
    {
        0x05, 0x3C, 0x3C
    }, 3 );
    esp_lcd_panel_io_tx_param(io, 0xB2, (uint8_t[])
    {
        0x05, 0x3C, 0x3C
    }, 3 );
    esp_lcd_panel_io_tx_param(io, 0xB3, (uint8_t[])
    {
        0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C
    }, 6 );
    esp_lcd_panel_io_tx_param(io, 0xB4, (uint8_t[])
    {
        0x00
    }, 1 ); //00 Dot inversion,  //07 column inversion
    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[])
    {
        0xa0
    }, 1 ); //MX, MY, RGB mode  (MADCTL)
    esp_lcd_panel_io_tx_param(io, 0xE0, (uint8_t[])
    {
        0x04, 0x22, 0x07, 0x0A, 0x2E, 0x30, 0x25, 0x2A, 0x28, 0x26, 0x2E, 0x3A, 0x00, 0x01, 0x03, 0x13
    }, 16 );
    esp_lcd_panel_io_tx_param(io, 0xE1, (uint8_t[])
    {
        0x04, 0x16, 0x06, 0x0D, 0x2D, 0x26, 0x23, 0x27, 0x27, 0x25, 0x2D, 0x3B, 0x00, 0x01, 0x04, 0x13
    }, 16 );
    esp_lcd_panel_io_tx_param(io, 0x20, (uint8_t[])
    {
        0
    }, 0 ); // buffer color inversion
#else
    esp_lcd_panel_invert_color(panel_handle, true);
#endif

    // FIll the handle for the initialized display
    disp->h = TFT_HEIGHT;
    disp->w = TFT_WIDTH;
    disp->setPx = setPxTft;
    disp->getPx = getPxTft;
    disp->clearPx = clearPxTft;
    disp->drawDisplay = drawDisplayTft;

    if(NULL == pixels)
    {
        pixels = (paletteColor_t*)malloc(sizeof(paletteColor_t) * TFT_HEIGHT * TFT_WIDTH);
    }
    disp->pxFb = pixels;
}

/**
 * @brief Disable the backlight (for powerdown)
 *
 */
void disableTFTBacklight(void)
{
    gpio_reset_pin( tftBacklightPin );
    gpio_set_level( tftBacklightPin, 0 );
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
void setPxTft(int16_t x, int16_t y, paletteColor_t px)
{
    if(0 <= x && x <= TFT_WIDTH && 0 <= y && y < TFT_HEIGHT && cTransparent != px)
    {
        pixels[y * TFT_WIDTH + x] = px;
    }
}

/**
 * @brief Get a single pixel in the display
 *
 * @param x The x coordinate of the pixel to get
 * @param y The y coordinate of the pixel to get
 * @return paletteColor_t The color of the given pixel, or black if out of bounds
 */
paletteColor_t getPxTft(int16_t x, int16_t y)
{
    if(0 <= x && x <= TFT_WIDTH && 0 <= y && y < TFT_HEIGHT)
    {
        return pixels[y * TFT_WIDTH + x];
    }
    return c000;
}

/**
 * @brief Clear all pixels in the display to black
 */
void clearPxTft(void)
{
    memset(pixels, 0, sizeof(paletteColor_t) * TFT_HEIGHT * TFT_WIDTH);
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

void drawDisplayTft(display_t * disp, bool drawDiff __attribute__((unused)), fnBackgroundDrawCallback_t fnBackgroundDrawCallback)
{
    // Indexes of the line currently being sent to the LCD and the line we're calculating
    uint8_t sending_line = 0;
    uint8_t calc_line = 0;

#ifdef PROCPROFILE
    uint32_t start, mid, final;
    uart_tx_one_char('f');
#endif

    // Send the frame, ping ponging the send buffer
    for (uint16_t y = 0; y < TFT_HEIGHT; y += PARALLEL_LINES)
    {
        // Calculate a line

#ifdef PROCPROFILE
        start = get_ccount();
#endif

        // Naive approach is ~100k cycles, later optimization at 60k cycles @ 160 MHz
        // If you quad-pixel it, so you operate on 4 pixels at the same time, you can get it down to 37k cycles.
        // Also FYI - I tried going palette-less, it only saved 18k per chunk (1.6ms per frame)
        uint32_t * outColor = (uint32_t*)s_lines[calc_line];
        uint32_t * inColor = (uint32_t*)&pixels[y*TFT_WIDTH];
        for (uint16_t x = 0; x < TFT_WIDTH/4*PARALLEL_LINES; x++)
        {
            uint32_t colors = *(inColor++);
            uint32_t word1 = paletteColors[(colors>> 0)&0xff] | (paletteColors[(colors>> 8)&0xff]<<16);
            uint32_t word2 = paletteColors[(colors>>16)&0xff] | (paletteColors[(colors>>24)&0xff]<<16);
            outColor[0] = word1;
            outColor[1] = word2;
            outColor += 2;
        }

#ifdef PROCPROFILE
        uart_tx_one_char('g');
        mid = get_ccount();
#endif

        sending_line = calc_line;
        calc_line = !calc_line;

        if( y != 0 && fnBackgroundDrawCallback )
        {
            fnBackgroundDrawCallback( disp, 0, y, TFT_WIDTH, PARALLEL_LINES, y/PARALLEL_LINES, TFT_HEIGHT/PARALLEL_LINES );
        }

        // (When operating @ 160 MHz)
        // This code takes 35k cycles when y == 0, but
        // this code takes ~~100k~~ 125k cycles when y != 0...
        // TODO NOTE:
        //  *** You have 780us here, to do whatever you want.  For free. ***
        //  You should avoid when y == 0, but that means you get 14 chunks
        //  every frame.
        //
        // This is because esp_lcd_panel_draw_bitmap blocks until the chunk
        // of frames has been sent.

        // Send the calculated data
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y,
                                  TFT_WIDTH, y + PARALLEL_LINES,
                                  s_lines[sending_line]);

        if( y == 0 && fnBackgroundDrawCallback )
        {
            fnBackgroundDrawCallback( disp, 0, y, TFT_WIDTH, PARALLEL_LINES, y/PARALLEL_LINES, TFT_HEIGHT/PARALLEL_LINES );
        }

#ifdef PROCPROFILE
        final = get_ccount();
        uart_tx_one_char('h');
#endif
    }

#ifdef PROCPROFILE
    uart_tx_one_char('i');
    //ESP_LOGI( "tft", "%d/%d", mid - start, final - mid );
#endif

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

