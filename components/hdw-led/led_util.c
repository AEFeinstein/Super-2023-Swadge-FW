//==============================================================================
// Includes
//==============================================================================

#include <stdio.h>
#include "led_util.h"

//==============================================================================
// Variables
//==============================================================================

led_strip_t* ledStrip = NULL;
uint16_t maxNumLeds = 0;
uint8_t ledBrightness = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 * @param h The input hue
 * @param s The input saturation
 * @param v The input value
 * @param r The output red
 * @param g The output green
 * @param b The output blue
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t* r,
    uint8_t* g, uint8_t* b)
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
 * @brief Initialize a strip of RGB LEDs
 * 
 * @param gpio    The GPIO the LEDs are connect to
 * @param rmt     The RMT channel to control the LEDs with
 * @param numLeds The maximum number of LEDs in the strip
 * @param brightness The initial LED brightness, 0 (off) to 8 (max bright)
 */
void initLeds(gpio_num_t gpio, gpio_num_t gpioAlt, rmt_channel_t rmt, uint16_t numLeds, uint8_t brightness)
{
    ledStrip = led_strip_init(rmt, gpio, gpioAlt, numLeds);
    maxNumLeds = numLeds;
    ledBrightness = (7 - brightness);
}

/**
 * Set the global LED brightness
 * 
 * @param brightness 0 (off) to 8 (max bright)
 */
void setLedBrightness(uint8_t brightness)
{
    // Bound
    if(brightness > 7)
    {
        brightness = 7;
    }
    // Set a value to rshift by
    ledBrightness = (7 - brightness);
}

/**
 * @brief Set the color for an LED strip
 *
 * @param leds    The color of the LEDs 
 * @param numLeds The number of LEDs to set
 */
void setLeds(led_t* leds, uint8_t numLeds)
{
    // Make sure the number of LEDs is in bounds
    if(numLeds > maxNumLeds)
    {
        numLeds = maxNumLeds;
    }

    // Set each LED
    for(int i = 0; i < numLeds; i++)
    {
        ledStrip->set_pixel(ledStrip, i,
            leds[i].r >> ledBrightness,
            leds[i].g >> ledBrightness,
            leds[i].b >> ledBrightness);
    }

    // Push the data to the strip
    ledStrip->refresh(ledStrip, 100);
}
