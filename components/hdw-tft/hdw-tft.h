#ifndef _HDW_TFT_H_
#define _HDW_TFT_H_

#include "hal/gpio_types.h"
#include "hal/spi_types.h"

// The pixel number in horizontal and vertical
#if defined(CONFIG_ST7735_160x80)
    #define TFT_WIDTH  160
    #define TFT_HEIGHT  80
#elif defined(CONFIG_ST7789_240x135)
    #define TFT_WIDTH  240
    #define TFT_HEIGHT 135
#elif defined(CONFIG_ST7789_240x240)
    #define TFT_WIDTH  240
    #define TFT_HEIGHT 240
#else
    #error "Please pick a screen size"
#endif

typedef union {
    struct __attribute__((packed)) {
        unsigned int r:5;
        unsigned int g:6;
        unsigned int b:5;
    } c;
    uint16_t val;
} tft_pixel_t;

void initTFT(spi_host_device_t spiHost, gpio_num_t sclk, gpio_num_t mosi,
             gpio_num_t dc, gpio_num_t cs, gpio_num_t rst, gpio_num_t backlight);
void draw_frame(void);
void drawPng(tft_pixel_t * png, uint16_t w, uint16_t h, uint16_t xOff, uint16_t yOff);

#endif