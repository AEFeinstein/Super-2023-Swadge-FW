//==============================================================================
// Includes
//==============================================================================

#include <stdio.h>
#include "led_util.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_LED_BRIGHTNESS 7

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
    ledBrightness = (MAX_LED_BRIGHTNESS - brightness);
}

/**
 * Set the global LED brightness
 *
 * @param brightness 0 (off) to MAX_LED_BRIGHTNESS (max bright)
 */
void setLedBrightness(uint8_t brightness)
{
    // Bound
    if(brightness > MAX_LED_BRIGHTNESS)
    {
        brightness = MAX_LED_BRIGHTNESS;
    }
    // Set a value to rshift by
    ledBrightness = (MAX_LED_BRIGHTNESS - brightness);
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
