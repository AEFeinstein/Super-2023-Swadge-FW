#ifndef _LED_H_
#define _LED_H_

#include <stdbool.h>
#include <stdint.h>
#include "hal/gpio_types.h"
#include "driver/rmt.h"
#include "led_strip.h"

typedef struct __attribute__((packed))
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
}
led_t;

void initLeds(gpio_num_t gpio, gpio_num_t gpioAlt, rmt_channel_t rmt, uint16_t numLeds, uint8_t brightness);
void setLedBrightness(uint8_t brightness);
void setLeds(led_t* leds, uint8_t numLeds);

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t* r, uint8_t* g, uint8_t* b);

#endif