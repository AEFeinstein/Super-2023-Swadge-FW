#ifndef _LED_H_
#define _LED_H_

#include <stdbool.h>
#include "hal/gpio_types.h"
#include "driver/rmt.h"
#include "led_strip.h"

led_strip_t * initLeds(gpio_num_t gpioNum, rmt_channel_t rmtTxChannel, uint16_t numLeds);
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b);

#endif