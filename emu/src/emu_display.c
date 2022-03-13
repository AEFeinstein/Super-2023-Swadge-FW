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
// Palette
//==============================================================================

uint32_t paletteColorsEmu[] = 
{
    0xFF000000,
    0xFF000033,
    0xFF000066,
    0xFF000099,
    0xFF0000CC,
    0xFF0000FF,
    0xFF003300,
    0xFF003333,
    0xFF003366,
    0xFF003399,
    0xFF0033CC,
    0xFF0033FF,
    0xFF006600,
    0xFF006633,
    0xFF006666,
    0xFF006699,
    0xFF0066CC,
    0xFF0066FF,
    0xFF009900,
    0xFF009933,
    0xFF009966,
    0xFF009999,
    0xFF0099CC,
    0xFF0099FF,
    0xFF00CC00,
    0xFF00CC33,
    0xFF00CC66,
    0xFF00CC99,
    0xFF00CCCC,
    0xFF00CCFF,
    0xFF00FF00,
    0xFF00FF33,
    0xFF00FF66,
    0xFF00FF99,
    0xFF00FFCC,
    0xFF00FFFF,
    0xFF330000,
    0xFF330033,
    0xFF330066,
    0xFF330099,
    0xFF3300CC,
    0xFF3300FF,
    0xFF333300,
    0xFF333333,
    0xFF333366,
    0xFF333399,
    0xFF3333CC,
    0xFF3333FF,
    0xFF336600,
    0xFF336633,
    0xFF336666,
    0xFF336699,
    0xFF3366CC,
    0xFF3366FF,
    0xFF339900,
    0xFF339933,
    0xFF339966,
    0xFF339999,
    0xFF3399CC,
    0xFF3399FF,
    0xFF33CC00,
    0xFF33CC33,
    0xFF33CC66,
    0xFF33CC99,
    0xFF33CCCC,
    0xFF33CCFF,
    0xFF33FF00,
    0xFF33FF33,
    0xFF33FF66,
    0xFF33FF99,
    0xFF33FFCC,
    0xFF33FFFF,
    0xFF660000,
    0xFF660033,
    0xFF660066,
    0xFF660099,
    0xFF6600CC,
    0xFF6600FF,
    0xFF663300,
    0xFF663333,
    0xFF663366,
    0xFF663399,
    0xFF6633CC,
    0xFF6633FF,
    0xFF666600,
    0xFF666633,
    0xFF666666,
    0xFF666699,
    0xFF6666CC,
    0xFF6666FF,
    0xFF669900,
    0xFF669933,
    0xFF669966,
    0xFF669999,
    0xFF6699CC,
    0xFF6699FF,
    0xFF66CC00,
    0xFF66CC33,
    0xFF66CC66,
    0xFF66CC99,
    0xFF66CCCC,
    0xFF66CCFF,
    0xFF66FF00,
    0xFF66FF33,
    0xFF66FF66,
    0xFF66FF99,
    0xFF66FFCC,
    0xFF66FFFF,
    0xFF990000,
    0xFF990033,
    0xFF990066,
    0xFF990099,
    0xFF9900CC,
    0xFF9900FF,
    0xFF993300,
    0xFF993333,
    0xFF993366,
    0xFF993399,
    0xFF9933CC,
    0xFF9933FF,
    0xFF996600,
    0xFF996633,
    0xFF996666,
    0xFF996699,
    0xFF9966CC,
    0xFF9966FF,
    0xFF999900,
    0xFF999933,
    0xFF999966,
    0xFF999999,
    0xFF9999CC,
    0xFF9999FF,
    0xFF99CC00,
    0xFF99CC33,
    0xFF99CC66,
    0xFF99CC99,
    0xFF99CCCC,
    0xFF99CCFF,
    0xFF99FF00,
    0xFF99FF33,
    0xFF99FF66,
    0xFF99FF99,
    0xFF99FFCC,
    0xFF99FFFF,
    0xFFCC0000,
    0xFFCC0033,
    0xFFCC0066,
    0xFFCC0099,
    0xFFCC00CC,
    0xFFCC00FF,
    0xFFCC3300,
    0xFFCC3333,
    0xFFCC3366,
    0xFFCC3399,
    0xFFCC33CC,
    0xFFCC33FF,
    0xFFCC6600,
    0xFFCC6633,
    0xFFCC6666,
    0xFFCC6699,
    0xFFCC66CC,
    0xFFCC66FF,
    0xFFCC9900,
    0xFFCC9933,
    0xFFCC9966,
    0xFFCC9999,
    0xFFCC99CC,
    0xFFCC99FF,
    0xFFCCCC00,
    0xFFCCCC33,
    0xFFCCCC66,
    0xFFCCCC99,
    0xFFCCCCCC,
    0xFFCCCCFF,
    0xFFCCFF00,
    0xFFCCFF33,
    0xFFCCFF66,
    0xFFCCFF99,
    0xFFCCFFCC,
    0xFFCCFFFF,
    0xFFFF0000,
    0xFFFF0033,
    0xFFFF0066,
    0xFFFF0099,
    0xFFFF00CC,
    0xFFFF00FF,
    0xFFFF3300,
    0xFFFF3333,
    0xFFFF3366,
    0xFFFF3399,
    0xFFFF33CC,
    0xFFFF33FF,
    0xFFFF6600,
    0xFFFF6633,
    0xFFFF6666,
    0xFFFF6699,
    0xFFFF66CC,
    0xFFFF66FF,
    0xFFFF9900,
    0xFFFF9933,
    0xFFFF9966,
    0xFFFF9999,
    0xFFFF99CC,
    0xFFFF99FF,
    0xFFFFCC00,
    0xFFFFCC33,
    0xFFFFCC66,
    0xFFFFCC99,
    0xFFFFCCCC,
    0xFFFFCCFF,
    0xFFFFFF00,
    0xFFFFFF33,
    0xFFFFFF66,
    0xFFFFFF99,
    0xFFFFFFCC,
    0xFFFFFFFF,
};

//==============================================================================
// Variables
//==============================================================================

// Display memory
uint32_t * bitmapDisplay = NULL; //0xAARRGGBB
uint32_t * constBitmapDisplay = NULL; //0xAARRGGBB
int bitmapWidth = 0;
int bitmapHeight = 0;
pthread_mutex_t displayMutex = PTHREAD_MUTEX_INITIALIZER;

// LED state
uint8_t rdNumLeds = 0;
led_t * rdLeds = NULL;

//==============================================================================
// Function Prototypes
//==============================================================================

void emuSetPxTft(int16_t x, int16_t y, paletteColor_t px);
paletteColor_t emuGetPxTft(int16_t x, int16_t y);
void emuClearPxTft(void);
void emuDrawDisplayTft(bool drawDiff);

void emuSetPxOled(int16_t x, int16_t y, paletteColor_t px);
paletteColor_t emuGetPxOled(int16_t x, int16_t y);
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
void emuSetPxTft(int16_t x, int16_t y, paletteColor_t px)
{
    if(0 <= x && x < TFT_WIDTH && 0 <= y && y < TFT_HEIGHT)
    {
        // Convert from 8 bit to 24 bit color
        if(cTransparent != px)
        {
            pthread_mutex_lock(&displayMutex);
            bitmapDisplay[(bitmapWidth * y) + x] = paletteColorsEmu[px];
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
paletteColor_t emuGetPxTft(int16_t x, int16_t y)
{
    if(0 <= x && x < TFT_WIDTH && 0 <= y && y < TFT_HEIGHT)
    {
        pthread_mutex_lock(&displayMutex);
        uint32_t argb = bitmapDisplay[(bitmapWidth * y) + x];
        pthread_mutex_unlock(&displayMutex);

        for(uint8_t i = 0; i < (sizeof(paletteColorsEmu) / sizeof(paletteColorsEmu[0])); i++)
        {
            if (argb == paletteColorsEmu[i])
            {
                return i;
            }
        }
    }
    return c000;
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
void emuSetPxOled(int16_t x UNUSED, int16_t y UNUSED, paletteColor_t px UNUSED)
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
paletteColor_t emuGetPxOled(int16_t x UNUSED, int16_t y UNUSED)
{
	WARN_UNIMPLEMENTED();
    return c000;
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
