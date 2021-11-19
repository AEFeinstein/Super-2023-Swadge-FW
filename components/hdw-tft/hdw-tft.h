#ifndef _HDW_TFT_H_
#define _HDW_TFT_H_

#include "hal/gpio_types.h"

typedef union {
    struct __attribute__((packed)) {
        unsigned int r:5;
        unsigned int g:6;
        unsigned int b:5;
    } c;
    uint16_t val;
} tft_pixel_t;

void initTFT(gpio_num_t sclk, gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs,
             gpio_num_t rst, gpio_num_t backlight);
void draw_frame(void);

#endif