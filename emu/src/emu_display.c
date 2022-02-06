//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "gpio_types.h"
#include "hal/spi_types.h"
#include "esp_log.h"

#include "emu_esp.h"
#include "display.h"
#include "emu_display.h"

//==============================================================================
// Variables
//==============================================================================

// Display memory
uint32_t * bitmapDisplay = NULL; //0xRRGGBBAA
uint32_t * constBitmapDisplay = NULL; //0xRRGGBBAA
int bitmapWidth = 0;
int bitmapHeight = 0;
pthread_mutex_t displayMutex = PTHREAD_MUTEX_INITIALIZER;

// LED state
uint8_t rdNumLeds = 0;
led_t * rdLeds = NULL;

//==============================================================================
// Function Prototypes
//==============================================================================

void emuSetPxTft(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t emuGetPxTft(int16_t x, int16_t y);
void emuClearPxTft(void);
void emuDrawDisplayTft(bool drawDiff);

void emuSetPxOled(int16_t x, int16_t y, rgba_pixel_t px);
rgba_pixel_t emuGetPxOled(int16_t x, int16_t y);
void emuClearPxOled(void);
void emuDrawDisplayOled(bool drawDiff);

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Lock the mutex that guards memory used by rawdraw on the main thread
 * and the swadge on a different thread
 */
void lockDisplayMemoryMutex(void)
{
    pthread_mutex_lock(&displayMutex);
}

/**
 * @brief Unlock the mutex that guards memory used by rawdraw on the main thread
 * and the swadge on a different thread
 */
void unlockDisplayMemoryMutex(void)
{
    pthread_mutex_unlock(&displayMutex);
}

/**
 * @brief Get a pointer to the display memory. This access must be guarded by
 * lockDisplayMemoryMutex() and unlockDisplayMemoryMutex()
 *
 * @param width A pointer to return the width of the display through
 * @param height A pointer to return the height of the display through
 * @return A pointer to the bitmap pixels for the display
 */
uint32_t * getDisplayBitmap(uint16_t * width, uint16_t * height)
{
    *width = bitmapWidth;
    *height = bitmapHeight;
    return constBitmapDisplay;
}

/**
 * @brief Get a pointer to the LED memory. This access must be guarded by
 * lockDisplayMemoryMutex() and unlockDisplayMemoryMutex()
 *
 * @param numLeds A pointer to return the number of led_t through
 * @return A pointer to the current LED state
 */
led_t * getLedMemory(uint8_t * numLeds)
{
    *numLeds = rdNumLeds;
    return rdLeds;
}

/**
 * @brief Free any memory allocated for the display
 */
void deinitDisplayMemory(void)
{
	pthread_mutex_lock(&displayMutex);
	if(NULL != bitmapDisplay)
	{
		free(bitmapDisplay);
	}
    if(NULL != constBitmapDisplay)
    {
        free(constBitmapDisplay);
    }
    if(NULL != rdLeds)
    {
        free(rdLeds);
    }
	pthread_mutex_unlock(&displayMutex);
}

//==============================================================================
// TFT
//==============================================================================

/**
 * @brief Initialize a TFT display and return it through a pointer arg
 *
 * @param disp The display to initialize
 * @param spiHost UNUSED
 * @param sclk UNUSED
 * @param mosi UNUSED
 * @param dc UNUSED
 * @param cs UNUSED
 * @param rst UNUSED
 * @param backlight UNUSED
 */
void initTFT(display_t * disp, spi_host_device_t spiHost UNUSED,
    gpio_num_t sclk UNUSED, gpio_num_t mosi UNUSED, gpio_num_t dc UNUSED,
    gpio_num_t cs UNUSED, gpio_num_t rst UNUSED, gpio_num_t backlight UNUSED)
{
    WARN_UNIMPLEMENTED();

	// ARGB pixels
	pthread_mutex_lock(&displayMutex);
	bitmapDisplay = malloc(sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	memset(bitmapDisplay, 0, sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	constBitmapDisplay = malloc(sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	memset(constBitmapDisplay, 0, sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	pthread_mutex_unlock(&displayMutex);
	bitmapWidth = TFT_WIDTH;
	bitmapHeight = TFT_HEIGHT;

    // Rawdraw initialized in main

    disp->w = TFT_WIDTH;
    disp->h = TFT_HEIGHT;
    disp->getPx = emuGetPxTft;
    disp->setPx = emuSetPxTft;
    disp->clearPx = emuClearPxTft;
    disp->drawDisplay = emuDrawDisplayTft;
}

/**
 * @brief Set a single pixel on the emulated TFT. This converts from 5 bit color
 * to 8 bit color
 *
 * @param x The X coordinate of the pixel to set
 * @param y The Y coordinate of the pixel to set
 * @param px The pixel to set, in 15 bit color with 1 alpha channel
 */
void emuSetPxTft(int16_t x, int16_t y, rgba_pixel_t px)
{
    if(0 <= x && x < TFT_WIDTH && 0 <= y && y < TFT_HEIGHT)
    {
        // Convert from 15 bit to 24 bit color
        if(PX_OPAQUE == px.a)
        {
            uint8_t r8 = ((px.r * 0xFF) / 0x1F) & 0xFF;
            uint8_t g8 = ((px.g * 0xFF) / 0x1F) & 0xFF;
            uint8_t b8 = ((px.b * 0xFF) / 0x1F) & 0xFF;
            pthread_mutex_lock(&displayMutex);
            bitmapDisplay[(bitmapWidth * y) + x] = (0xFF << 24) | (r8 << 16) | (g8 << 8) | (b8);
            pthread_mutex_unlock(&displayMutex);
        }
    }
}

/**
 * @brief Get a pixel from the emulated TFT. This converts 8 bit color to 5 bit
 * color
 *
 * @param x The X coordinate of the pixel to get
 * @param y The Y coordinate of the pixel to get
 * @return The pixel at the given coordinate
 */
rgba_pixel_t emuGetPxTft(int16_t x, int16_t y)
{
    if(0 <= x && x < TFT_WIDTH && 0 <= y && y < TFT_HEIGHT)
    {
        pthread_mutex_lock(&displayMutex);
        uint32_t argb = bitmapDisplay[(bitmapWidth * y) + x];
        pthread_mutex_unlock(&displayMutex);
        rgba_pixel_t px;
        px.r = (((argb & 0xFF0000) >> 16) * 0x1F) / 0xFF; // 5 bit
        px.g = (((argb & 0x00FF00) >>  8) * 0x1F) / 0xFF; // 5 bit
        px.b = (((argb & 0x0000FF) >>  0) * 0x1F) / 0xFF; // 5 bit
        return px;
    }
    else
    {
        rgba_pixel_t px = {0};
        return px;
    }
}

/**
 * @brief Clear the entire display to opaque black in one call
 */
void emuClearPxTft(void)
{
	pthread_mutex_lock(&displayMutex);
	memset(bitmapDisplay, 0x00, sizeof(uint32_t) * bitmapWidth * bitmapHeight);
	pthread_mutex_unlock(&displayMutex);
}

/**
 * @brief Called when the Swadge wants to draw a new display. Note, this is
 * called from a pthread, so it raises a flag to draw on the main thread
 *
 * @param drawDiff unused, the whole display is always drawn
 */
void emuDrawDisplayTft(bool drawDiff UNUSED)
{
	/* Copy the current framebuffer to memory that won't be modified by the
     * Swadge mode. rawdraw will use this non-changing bitmap to draw
     */
	pthread_mutex_lock(&displayMutex);
    memcpy(constBitmapDisplay, bitmapDisplay, sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	pthread_mutex_unlock(&displayMutex);
}

//==============================================================================
// OLED
//==============================================================================

/**
 * @brief Initialie a null OLED since the OLED isn't emulated
 *
 * @param disp The display to initialize
 * @param reset unused
 * @param rst_gpio unused
 * @return true
 */
bool initOLED(display_t * disp, bool reset UNUSED, gpio_num_t rst UNUSED)
{
    disp->w = 0;
    disp->h = 0;
    disp->getPx = emuGetPxOled;
    disp->setPx = emuSetPxOled;
    disp->clearPx = emuClearPxOled;
    disp->drawDisplay = emuDrawDisplayOled;

    return true;
}

/**
 * @brief Set a single pixel on the emulated OLED. This converts from 1 bit
 * color to 8 bit color
 *
 * @param x The X coordinate of the pixel to set
 * @param y The Y coordinate of the pixel to set
 * @param px The pixel to set, in 15 bit color with 1 alpha channel
 */
void emuSetPxOled(int16_t x UNUSED, int16_t y UNUSED, rgba_pixel_t px UNUSED)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief Get a pixel from the emulated TFT. This converts 8 bit color to 1 bit
 * color
 *
 * @param x The X coordinate of the pixel to get
 * @param y The Y coordinate of the pixel to get
 * @return The pixel at the given coordinate
 */
rgba_pixel_t emuGetPxOled(int16_t x UNUSED, int16_t y UNUSED)
{
	WARN_UNIMPLEMENTED();
	rgba_pixel_t px = {.r=0x00, .g = 0x00, .b = 0x00, .a = PX_OPAQUE};
	return px;
}

/**
 * @brief Clear the entire display to opaque black in one call
 */
void emuClearPxOled(void)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief Called when the Swadge wants to draw a new display. Note, this is
 * called from a pthread, so it raises a flag to draw on the main thread
 *
 * @param drawDiff unused, the whole display is always drawn
 */
void emuDrawDisplayOled(bool drawDiff UNUSED)
{
	WARN_UNIMPLEMENTED();
}

//==============================================================================
// LEDs
//==============================================================================

/**
 * @brief Initialize the emulated LEDs
 *
 * @param gpio unused
 * @param rmt unused
 * @param numLeds The number of LEDs to display
 */
void initLeds(gpio_num_t gpio UNUSED, rmt_channel_t rmt UNUSED, uint16_t numLeds)
{
    // If the LEDs haven't been initialized yet
    if(NULL == rdLeds)
    {
        // Allocate some LED memory
        pthread_mutex_lock(&displayMutex);
        rdLeds = malloc(sizeof(led_t) * numLeds);
        pthread_mutex_unlock(&displayMutex);
        // Save the number of LEDs
        rdNumLeds = numLeds;
    }
}

/**
 * @brief Set the color for an emulated LED strip
 *
 * @param leds    The color of the LEDs
 * @param numLeds The number of LEDs to set
 */
void setLeds(led_t* leds, uint8_t numLeds)
{
	pthread_mutex_lock(&displayMutex);
	memcpy(rdLeds, leds, sizeof(led_t) * numLeds);
	pthread_mutex_unlock(&displayMutex);
}

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
