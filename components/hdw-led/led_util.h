#ifndef _LED_H_
#define _LED_H_

#include <stdbool.h>
#include "hal/gpio_types.h"
#include "driver/rmt.h"
#include "led_strip.h"

#define LED_GPIO        GPIO_NUM_19
#define LED_RMT_CHANNEL RMT_CHANNEL_0
#define NUM_LEDS        6

typedef struct __attribute__((packed))
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
}
led_t;

void initLeds(void);
void setLeds(led_t* leds, uint8_t numLeds);

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t* r, uint8_t* g, uint8_t* b);

#endif