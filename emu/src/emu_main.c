//==============================================================================
// Includes
//==============================================================================

#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "esp_log.h"
#include "swadge_esp32.h"
#include "btn.h"

#include "list.h"

#include "emu_esp.h"
#include "sound.h"
#include "emu_display.h"
#include "emu_sound.h"
#include "emu_sensors.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"

//==============================================================================
// Defines
//==============================================================================

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MIN_LED_HEIGHT 64

#define BG_COLOR  0x191919FF // This color isn't part of the palette
#define DIV_COLOR 0x808080FF

//==============================================================================
// Function prototypes
//==============================================================================

void drawBitmapPixel(uint32_t * bitmapDisplay, int w, int h, int x, int y, uint32_t col);
void plotRoundedCorners(uint32_t * bitmapDisplay, int w, int h, int r, uint32_t col);

//==============================================================================
// Variables
//==============================================================================

static bool isRunning = true;

//==============================================================================
// Functions
//==============================================================================

/**
 * This function must be provided for rawdraw. Key events are received here
 *
 * Note that this is called on the main thread and the swadge task is on a
 * separate thread, so queue accesses must be mutexed
 *
 * @param keycode The key code, a lowercase ascii char
 * @param bDown true if the key was pressed, false if it was released
 */
void HandleKey( int keycode, int bDown )
{
    emuSensorHandleKey(keycode, bDown);
}

/**
 * @brief Handle mouse click events from rawdraw
 *
 * @param x The x coordinate of the mouse event
 * @param y The y coordinate of the mouse event
 * @param button The mouse button that was pressed or released
 * @param bDown true if the button was pressed, false if it was released
 */
void HandleButton( int x UNUSED, int y UNUSED, int button UNUSED, int bDown UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Handle mouse motion events from rawdraw
 *
 * @param x The x coordinate of the mouse event
 * @param y The y coordinate of the mouse event
 * @param mask A mask of mouse buttons that are currently held down
 */
void HandleMotion( int x UNUSED, int y UNUSED, int mask UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Free memory on exit
 */
void HandleDestroy()
{
    // Stop the main loop
    isRunning = false;

    // Upon exit, stop all tasks
    joinThreads();

    // Then free display memory
    deinitDisplayMemory();

    // Close sound
    deinitSound();

    // Free button queue
    deinitButtons();
}

/**
 * @brief Helper function to draw to a bitmap display
 * 
 * @param bitmapDisplay The display to draw to
 * @param w The width of the display
 * @param h The height of the display
 * @param x The X coordinate to draw a pixel at
 * @param y The Y coordinate to draw a pixel at
 * @param col The color to draw the pixel
 */
void drawBitmapPixel(uint32_t * bitmapDisplay, int w, int h, int x, int y, uint32_t col)
{
    if((y * w) + x < (w * h))
    {
        bitmapDisplay[(y * w) + x] = col;
    }
}

/**
 * @brief Helper functions to draw rounded corners to the display
 * 
 * @param bitmapDisplay The display to round the corners on
 * @param w The width of the display
 * @param h The height of the display
 * @param r The radius of the rounded corners
 * @param col The color to draw the rounded corners
 */
void plotRoundedCorners(uint32_t * bitmapDisplay, int w, int h, int r, uint32_t col)
{
    int or = r;
    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
        for(int xLine = 0; xLine <= (or + x); xLine++)
        {
            drawBitmapPixel(bitmapDisplay, w, h,     xLine    , h - (or - y) - 1, col); /* I.   Quadrant -x -y */
            drawBitmapPixel(bitmapDisplay, w, h, w - xLine - 1, h - (or - y) - 1, col); /* II.  Quadrant +x -y */
            drawBitmapPixel(bitmapDisplay, w, h,     xLine    ,     (or - y)    , col); /* III. Quadrant -x -y */
            drawBitmapPixel(bitmapDisplay, w, h, w - xLine - 1,     (or - y)    , col); /* IV.  Quadrant +x -y */
        }

        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1;    /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1;    /* -> x-step now */
        }
    } while (x < 0);
}

/**
 * @brief The main emulator function. This initializes rawdraw and calls
 * app_main(), then spins in a loop updating the rawdraw UI
 *
 * @param argc unused
 * @param argv unused
 * @return 0 on success, a nonzero value for any errors
 */
int main(int argc UNUSED, char ** argv UNUSED)
{
    // First initialize rawdraw
    // Screen-specific configurations
    // Save window dimensions from the last loop
    short lastWindow_w = 0;
    short lastWindow_h = 0;
    int16_t led_h = MIN_LED_HEIGHT;
    CNFGSetup( "Swadge S2 Emulator", (TFT_WIDTH * 2), (TFT_HEIGHT * 2) + led_h + 1);

    // This is the 'main' that gets called when the ESP boots
    app_main();

    // Spin around waiting for a program exit afterwards
    while(1)
    {
        // Always handle inputs
        CNFGHandleInput();

        // If not running anymore, don't handle graphics
        // Must be checked after handling input, before graphics
        if(!isRunning)
        {
            break;
        }

        // Grey Background
        CNFGBGColor = BG_COLOR;
        CNFGClearFrame();

        // Get the current window dimensions
        short window_w, window_h;
        CNFGGetDimensions(&window_w, &window_h);

        // If the dimensions changed
        if((lastWindow_h != window_h) || (lastWindow_w != window_w))
        {
            // Figure out how much the TFT should be scaled by
            uint8_t widthMult = window_w / TFT_WIDTH;
            if(0 == widthMult)
            {
                widthMult = 1;
            }
            uint8_t heightMult = (window_h - MIN_LED_HEIGHT - 1) / TFT_HEIGHT;
            if(0 == heightMult)
            {
                heightMult = 1;
            }
            uint8_t screenMult = MIN(widthMult, heightMult);

            // LEDs take up the rest of the vertical space
            led_h = window_h - 1 - (screenMult * TFT_HEIGHT);

            // Set the multiplier
            setDisplayBitmapMultiplier(screenMult);

            // Save for the next loop
            lastWindow_w = window_w;
            lastWindow_h = window_h;
        }

        // Get a lock on the display memory mutex (LED & TFT)
        lockDisplayMemoryMutex();

        // Get the LED memory
        uint8_t numLeds;
        led_t * leds = getLedMemory(&numLeds);

        // Draw simulated LEDs
        if (numLeds > 0 && NULL != leds)
        {
            short led_w = window_w / numLeds;
            for(int i = 0; i < numLeds; i++)
            {
                CNFGColor( (leds[i].r << 24) | (leds[i].g << 16) | (leds[i].b << 8) | 0xFF);
                if(i == numLeds - 1)
                {
                    CNFGTackRectangle(i * led_w, 0, window_w, led_h);
                }
                else
                {
                    CNFGTackRectangle(i * led_w, 0, (i + 1) * led_w, led_h);
                }
            }
        }

        // Draw dividing line
        CNFGColor( DIV_COLOR );
        CNFGTackSegment(0, led_h, window_w, led_h);

        // Get the display memory
        uint16_t bitmapWidth, bitmapHeight;
        uint32_t * bitmapDisplay = getDisplayBitmap(&bitmapWidth, &bitmapHeight);

        if((0 != bitmapWidth) && (0 != bitmapHeight) && (NULL != bitmapDisplay))
        {
#if defined(CONFIG_GC9307_240x280)
            plotRoundedCorners(bitmapDisplay, bitmapWidth, bitmapHeight, (bitmapWidth / TFT_WIDTH) * 29, BG_COLOR);
#endif
            // Update the display, centered
            CNFGBlitImage(bitmapDisplay,
                (window_w - bitmapWidth) / 2, 
                (led_h + 1) + ((window_h - (led_h + 1) - bitmapHeight) / 2),
                bitmapWidth, bitmapHeight);
        }

        //Display the image and wait for time to display next frame.
        CNFGSwapBuffers();

        unlockDisplayMemoryMutex();

        // Sleep for 1 ms
        usleep(1000);
    }

    return 0;
}
