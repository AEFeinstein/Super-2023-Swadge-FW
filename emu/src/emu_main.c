#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "esp_log.h"
#include "swadge_esp32.h"
#include "btn.h"

#include "list.h"

#include "emu_main.h"
#include "esp_emu.h"
#include "sound.h"
#include "emu_display.h"
#include "emu_sound.h"
#include "emu_sensors.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#define MAX(x,y) ((x)>(y)?(x):(y))

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
 * @brief
 *
 * @param x
 * @param y
 * @param button
 * @param bDown
 */
void HandleButton( int x, int y, int button, int bDown )
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param mask
 */
void HandleMotion( int x, int y, int mask )
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
 * @brief TODO
 *
 * @param argc
 * @param argv
 * @return int
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
		
		// Only redraw the screen when requested by the thread
		if(shouldDrawTft)
		{
			// Lower flag
			shouldDrawTft = false;

			// Grey Background
			CNFGBGColor = 0x252525FF;
			CNFGClearFrame();

			// Get a lock on the display memory mutex (LED & TFT)
			lockDisplayMemoryMutex();

			// Get the display memory
			uint16_t bitmapWidth, bitmapHeight;
			uint32_t * bitmapDisplay = getDisplayBitmap(&bitmapWidth, &bitmapHeight);

			// Update the display
			CNFGUpdateScreenWithBitmap(bitmapDisplay, bitmapWidth, bitmapHeight);

			// Get the LED memory
			uint8_t numLeds;
			led_t * leds = getLedMemory(&numLeds);

			// Draw simulated LEDs
			if (numLeds > 0)
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

			unlockDisplayMemoryMutex();

			// Draw dividing line
			CNFGColor( 0x808080FF ); 
			CNFGTackSegment(0, TFT_HEIGHT, TFT_WIDTH, TFT_HEIGHT);

			//Display the image and wait for time to display next frame.
			CNFGSwapBuffers();
		}

		// Sleep for a ms
        usleep(1);
    }

	return 0;
}

/**
 * @brief When the main task yields, rest
 * 
 */
void onTaskYield(void)
{
	// Just sleep for a ms
	usleep(1);
}
