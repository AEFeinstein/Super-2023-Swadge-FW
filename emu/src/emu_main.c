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
        CNFGBGColor = 0x252525FF;
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
        CNFGColor( 0x808080FF );
        CNFGTackSegment(0, led_h, window_w, led_h);

        // Get the display memory
        uint16_t bitmapWidth, bitmapHeight;
        uint32_t * bitmapDisplay = getDisplayBitmap(&bitmapWidth, &bitmapHeight);

        if((0 != bitmapWidth) && (0 != bitmapHeight) && (NULL != bitmapDisplay))
        {
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
