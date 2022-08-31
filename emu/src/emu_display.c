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

#include "hdw-tft.h"
#include "ssd1306.h"

//==============================================================================
// Palette
//==============================================================================

uint32_t paletteColorsEmu[216] =
{
    0x000000FF,
    0x000033FF,
    0x000066FF,
    0x000099FF,
    0x0000CCFF,
    0x0000FFFF,
    0x003300FF,
    0x003333FF,
    0x003366FF,
    0x003399FF,
    0x0033CCFF,
    0x0033FFFF,
    0x006600FF,
    0x006633FF,
    0x006666FF,
    0x006699FF,
    0x0066CCFF,
    0x0066FFFF,
    0x009900FF,
    0x009933FF,
    0x009966FF,
    0x009999FF,
    0x0099CCFF,
    0x0099FFFF,
    0x00CC00FF,
    0x00CC33FF,
    0x00CC66FF,
    0x00CC99FF,
    0x00CCCCFF,
    0x00CCFFFF,
    0x00FF00FF,
    0x00FF33FF,
    0x00FF66FF,
    0x00FF99FF,
    0x00FFCCFF,
    0x00FFFFFF,
    0x330000FF,
    0x330033FF,
    0x330066FF,
    0x330099FF,
    0x3300CCFF,
    0x3300FFFF,
    0x333300FF,
    0x333333FF,
    0x333366FF,
    0x333399FF,
    0x3333CCFF,
    0x3333FFFF,
    0x336600FF,
    0x336633FF,
    0x336666FF,
    0x336699FF,
    0x3366CCFF,
    0x3366FFFF,
    0x339900FF,
    0x339933FF,
    0x339966FF,
    0x339999FF,
    0x3399CCFF,
    0x3399FFFF,
    0x33CC00FF,
    0x33CC33FF,
    0x33CC66FF,
    0x33CC99FF,
    0x33CCCCFF,
    0x33CCFFFF,
    0x33FF00FF,
    0x33FF33FF,
    0x33FF66FF,
    0x33FF99FF,
    0x33FFCCFF,
    0x33FFFFFF,
    0x660000FF,
    0x660033FF,
    0x660066FF,
    0x660099FF,
    0x6600CCFF,
    0x6600FFFF,
    0x663300FF,
    0x663333FF,
    0x663366FF,
    0x663399FF,
    0x6633CCFF,
    0x6633FFFF,
    0x666600FF,
    0x666633FF,
    0x666666FF,
    0x666699FF,
    0x6666CCFF,
    0x6666FFFF,
    0x669900FF,
    0x669933FF,
    0x669966FF,
    0x669999FF,
    0x6699CCFF,
    0x6699FFFF,
    0x66CC00FF,
    0x66CC33FF,
    0x66CC66FF,
    0x66CC99FF,
    0x66CCCCFF,
    0x66CCFFFF,
    0x66FF00FF,
    0x66FF33FF,
    0x66FF66FF,
    0x66FF99FF,
    0x66FFCCFF,
    0x66FFFFFF,
    0x990000FF,
    0x990033FF,
    0x990066FF,
    0x990099FF,
    0x9900CCFF,
    0x9900FFFF,
    0x993300FF,
    0x993333FF,
    0x993366FF,
    0x993399FF,
    0x9933CCFF,
    0x9933FFFF,
    0x996600FF,
    0x996633FF,
    0x996666FF,
    0x996699FF,
    0x9966CCFF,
    0x9966FFFF,
    0x999900FF,
    0x999933FF,
    0x999966FF,
    0x999999FF,
    0x9999CCFF,
    0x9999FFFF,
    0x99CC00FF,
    0x99CC33FF,
    0x99CC66FF,
    0x99CC99FF,
    0x99CCCCFF,
    0x99CCFFFF,
    0x99FF00FF,
    0x99FF33FF,
    0x99FF66FF,
    0x99FF99FF,
    0x99FFCCFF,
    0x99FFFFFF,
    0xCC0000FF,
    0xCC0033FF,
    0xCC0066FF,
    0xCC0099FF,
    0xCC00CCFF,
    0xCC00FFFF,
    0xCC3300FF,
    0xCC3333FF,
    0xCC3366FF,
    0xCC3399FF,
    0xCC33CCFF,
    0xCC33FFFF,
    0xCC6600FF,
    0xCC6633FF,
    0xCC6666FF,
    0xCC6699FF,
    0xCC66CCFF,
    0xCC66FFFF,
    0xCC9900FF,
    0xCC9933FF,
    0xCC9966FF,
    0xCC9999FF,
    0xCC99CCFF,
    0xCC99FFFF,
    0xCCCC00FF,
    0xCCCC33FF,
    0xCCCC66FF,
    0xCCCC99FF,
    0xCCCCCCFF,
    0xCCCCFFFF,
    0xCCFF00FF,
    0xCCFF33FF,
    0xCCFF66FF,
    0xCCFF99FF,
    0xCCFFCCFF,
    0xCCFFFFFF,
    0xFF0000FF,
    0xFF0033FF,
    0xFF0066FF,
    0xFF0099FF,
    0xFF00CCFF,
    0xFF00FFFF,
    0xFF3300FF,
    0xFF3333FF,
    0xFF3366FF,
    0xFF3399FF,
    0xFF33CCFF,
    0xFF33FFFF,
    0xFF6600FF,
    0xFF6633FF,
    0xFF6666FF,
    0xFF6699FF,
    0xFF66CCFF,
    0xFF66FFFF,
    0xFF9900FF,
    0xFF9933FF,
    0xFF9966FF,
    0xFF9999FF,
    0xFF99CCFF,
    0xFF99FFFF,
    0xFFCC00FF,
    0xFFCC33FF,
    0xFFCC66FF,
    0xFFCC99FF,
    0xFFCCCCFF,
    0xFFCCFFFF,
    0xFFFF00FF,
    0xFFFF33FF,
    0xFFFF66FF,
    0xFFFF99FF,
    0xFFFFCCFF,
    0xFFFFFFFF,
};

//==============================================================================
// Variables
//==============================================================================

// Display memory
uint32_t * bitmapDisplay = NULL; //0xRRGGBBAA
uint32_t * constBitmapDisplay = NULL; //0xRRGGBBAA
int bitmapWidth = 0;
int bitmapHeight = 0;
int displayMult = 1;
pthread_mutex_t displayMutex = PTHREAD_MUTEX_INITIALIZER;

// LED state
uint8_t rdNumLeds = 0;
led_t * rdLeds = NULL;
uint8_t ledBrightness = 0;

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
 * Set a multiplier to draw the TFT to the window at
 *
 * @param multiplier The multipler for the display, no less than 1
 */
void setDisplayBitmapMultiplier(uint8_t multiplier)
{
    lockDisplayMemoryMutex();

    displayMult = multiplier;

    // Reallocate bitmapDisplay
    free(bitmapDisplay);
    bitmapDisplay = calloc((multiplier * TFT_WIDTH) * (multiplier * TFT_HEIGHT),
        sizeof(uint32_t));

    // Reallocate constBitmapDisplay
    free(constBitmapDisplay);
    constBitmapDisplay = calloc((multiplier * TFT_WIDTH) * (multiplier * TFT_HEIGHT),
        sizeof(uint32_t));

    unlockDisplayMemoryMutex();
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
    *width = (bitmapWidth * displayMult);
    *height = (bitmapHeight * displayMult);
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
 * @param isPwmBacklight UNUSED
 */
void initTFT(display_t * disp, spi_host_device_t spiHost UNUSED,
    gpio_num_t sclk UNUSED, gpio_num_t mosi UNUSED, gpio_num_t dc UNUSED,
    gpio_num_t cs UNUSED, gpio_num_t rst UNUSED, gpio_num_t backlight UNUSED,
    bool isPwmBacklight UNUSED)
{
    WARN_UNIMPLEMENTED();

	// ARGB pixels
	pthread_mutex_lock(&displayMutex);

    bitmapWidth = TFT_WIDTH;
    bitmapHeight = TFT_HEIGHT;

    // Set up underlying bitmap
    if(NULL == bitmapDisplay)
    {
        bitmapDisplay = calloc(TFT_WIDTH * TFT_HEIGHT, sizeof(uint32_t));
    }

    // This may be setup by the emulator already
    if(NULL == constBitmapDisplay)
    {
        constBitmapDisplay = calloc(TFT_WIDTH * TFT_HEIGHT, sizeof(uint32_t));
        displayMult = 1;        
    }
	pthread_mutex_unlock(&displayMutex);

    // Rawdraw initialized in main

    disp->w = TFT_WIDTH;
    disp->h = TFT_HEIGHT;
    disp->getPx = emuGetPxTft;
    disp->setPx = emuSetPxTft;
    disp->clearPx = emuClearPxTft;
    disp->drawDisplay = emuDrawDisplayTft;
}

/**
 * @brief TODO
 * 
 * @param intensity 
 */
void setTFTBacklight(uint8_t intensity UNUSED)
{
	WARN_UNIMPLEMENTED();
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
            for(uint16_t mY = 0; mY < displayMult; mY++)
            {
                for(uint16_t mX = 0; mX < displayMult; mX++)
                {
                    int dstX = ((x * displayMult) + mX);
                    int dstY = ((y * displayMult) + mY);
                    bitmapDisplay[(dstY * (TFT_WIDTH * displayMult)) + dstX] = paletteColorsEmu[px];
                }
            }
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
        int srcX = (x * displayMult);
        int srcY = (y * displayMult);
        uint32_t argb = bitmapDisplay[(srcY * (TFT_WIDTH * displayMult)) + srcX];
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
    for(uint32_t idx = 0; idx < TFT_HEIGHT * displayMult * TFT_WIDTH * displayMult; idx++)
    {
        bitmapDisplay[idx] = 0x000000FF;
    }
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
    memcpy(constBitmapDisplay, bitmapDisplay, sizeof(uint32_t) * TFT_HEIGHT * displayMult * TFT_WIDTH * displayMult);
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
 * @param brightness The initial LED brightness, 0 (off) to 8 (max bright)
 */
void initLeds(gpio_num_t gpio UNUSED, rmt_channel_t rmt UNUSED, uint16_t numLeds, uint8_t brightness)
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
        // Save the brightness
        ledBrightness = (8 - brightness);
    }
}

/**
 * Set the global LED brightness
 * 
 * @param brightness 0 (off) to 8 (max bright)
 */

void setLedBrightness(uint8_t brightness)
{
    // Bound
    if(brightness > 8)
    {
        brightness = 8;
    }
    // Set a value to rshift by
    ledBrightness = (8 - brightness);
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
    for(int i = 0; i < numLeds; i++)
    {
        rdLeds[i].r = leds[i].r >> ledBrightness;
        rdLeds[i].g = leds[i].g >> ledBrightness;
        rdLeds[i].b = leds[i].b >> ledBrightness;
    }
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
