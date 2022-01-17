#ifndef _HDW_TFT_H_
#define _HDW_TFT_H_

#include "hal/gpio_types.h"
#include "hal/spi_types.h"

#include "../../main/display/display.h"

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

void initTFT(display_t * disp, spi_host_device_t spiHost, gpio_num_t sclk,
            gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs, gpio_num_t rst,
            gpio_num_t backlight);
void clearTFT(void);
void draw_frame(void);

#endif