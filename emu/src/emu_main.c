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

#define MAX(x,y) ((x)>(y)?(x):(y))

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
    CNFGSetup( "Swadge S2 Emulator", TFT_WIDTH, TFT_HEIGHT + MAX(TFT_WIDTH, TFT_HEIGHT) + 1);

    // This is the 'main' that gets called when the ESP boots
    app_main();

    // Spin around waiting for a program exit afterwards
    while(1)
    {
        // Always handle inputs
        CNFGHandleInput();

        // Grey Background
        CNFGBGColor = 0x252525FF;
        CNFGClearFrame();

        // Get a lock on the display memory mutex (LED & TFT)
        lockDisplayMemoryMutex();

        // Get the LED memory
        uint8_t numLeds;
        led_t * leds = getLedMemory(&numLeds);

        // Draw simulated LEDs
        if (numLeds > 0 && NULL != leds)
        {
            for(int i = 0; i < numLeds; i++)
            {
                float angle1 = ( i      * 2 * M_PI) / numLeds;
                float angle2 = ((i + 1) * 2 * M_PI) / numLeds;
                RDPoint points[] =
                {
                    {
                        .x = (TFT_WIDTH / 2) + (TFT_WIDTH/2) * sin(angle1),
                        .y = 1 + (TFT_HEIGHT + (TFT_WIDTH/2)) + (TFT_WIDTH/2) * cos(angle1),
                    },
                    {
                        .x = (TFT_WIDTH / 2) + (TFT_WIDTH/2) * sin(angle2),
                        .y = 1 + (TFT_HEIGHT + (TFT_WIDTH/2)) + (TFT_WIDTH/2) * cos(angle2),
                    },
                    {
                        .x = TFT_WIDTH / 2,
                        .y = 1 + TFT_HEIGHT + (TFT_WIDTH/2)
                    }
                };

                // Draw filled polygon
                CNFGColor( (leds[i].r << 24) | (leds[i].g << 16) | (leds[i].b << 8) | 0xFF);
                CNFGTackPoly(points, ARRAY_SIZE(points));

                // Draw outline
                CNFGColor( 0x808080FF );
                CNFGTackSegment(points[0].x, points[0].y, points[1].x, points[1].y);
            }
        }

        // Draw dividing line
        CNFGColor( 0x808080FF );
        CNFGTackSegment(0, TFT_HEIGHT, TFT_WIDTH, TFT_HEIGHT);

        // Get the display memory
        uint16_t bitmapWidth, bitmapHeight;
        uint32_t * bitmapDisplay = getDisplayBitmap(&bitmapWidth, &bitmapHeight);

        if((0 != bitmapWidth) && (0 != bitmapHeight) && (NULL != bitmapDisplay))
        {
            // Update the display
            CNFGBlitImage(bitmapDisplay, 0, 0, bitmapWidth, bitmapHeight);
        }

        //Display the image and wait for time to display next frame.
        CNFGSwapBuffers();

        unlockDisplayMemoryMutex();

        // Sleep for ten ms
        usleep(10 * 1000);
    }

    return 0;
}
