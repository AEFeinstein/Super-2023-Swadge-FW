#include <stdio.h>
#include "led_util.h"

led_strip_t* ledStrip = NULL;
uint16_t maxNumLeds = 0;

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t* r, uint8_t* g, uint8_t* b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
        case 0:
            *r = rgb_max;
            *g = rgb_min + rgb_adj;
            *b = rgb_min;
            break;
        case 1:
            *r = rgb_max - rgb_adj;
            *g = rgb_max;
            *b = rgb_min;
            break;
        case 2:
            *r = rgb_min;
            *g = rgb_max;
            *b = rgb_min + rgb_adj;
            break;
        case 3:
            *r = rgb_min;
            *g = rgb_max - rgb_adj;
            *b = rgb_max;
            break;
        case 4:
            *r = rgb_min + rgb_adj;
            *g = rgb_min;
            *b = rgb_max;
            break;
        default:
            *r = rgb_max;
            *g = rgb_min;
            *b = rgb_max - rgb_adj;
            break;
    }
}

/**
 * @brief TODO
 *
 */
void initLeds(gpio_num_t gpio, rmt_channel_t rmt, uint16_t numLeds)
{
    ledStrip = led_strip_init(rmt, gpio, numLeds);
    maxNumLeds = numLeds;
}

/**
 * TODO
 *
 * @param leds
 * @param numLeds
 */
void setLeds(led_t* leds, uint8_t numLeds)
{
    if(numLeds > maxNumLeds)
    {
        numLeds = maxNumLeds;
    }

    for(int i = 0; i < numLeds; i++)
    {
        ledStrip->set_pixel(ledStrip, i, leds[i].r, leds[i].g, leds[i].b);
    }
    ledStrip->refresh(ledStrip, 100);
}