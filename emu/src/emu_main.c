#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "list.h"

#include "emu_main.h"
#include "esp_emu.h"
#include "esp_log.h"
#include "swadge_esp32.h"
#include "sound.h"
#include "emu_display.h"
#include "emu_sound.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#define MAX(x,y) ((x)>(y)?(x):(y))

// Input queues for buttons
char inputKeys[32];
uint32_t buttonState = 0;
list_t * buttonQueue;
pthread_mutex_t buttonQueueMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Set up keyboard keys to act as pushbuttons
 * 
 * @param numButtons The number of keyboard keys to set up, up to 32
 * @param keyOrder The keys which should be used, as lowercase letters
 */
void setInputKeys(uint8_t numButtons, char * keyOrder)
{
	memcpy(inputKeys, keyOrder, numButtons);
	buttonState = 0;
	buttonQueue = list_new();
}

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
	// Check keycode against initialized keys
	for(uint8_t idx = 0; idx < ARRAY_SIZE(inputKeys); idx++)
	{
		// If this matches
		if(keycode == inputKeys[idx])
		{
			// Set or clear the button
			if(bDown)
			{
				// Check if button was already pressed
				if(buttonState & (1 << idx))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, set it!
					buttonState |= (1 << idx);
				}
			}
			else
			{
				// Check if button was already released
				if(0 == (buttonState & (1 << idx)))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, clear it!
					buttonState &= ~(1 << idx);
				}
			}

			// Create a new event
			buttonEvt_t * evt = malloc(sizeof(buttonEvt_t));
			evt->button = idx;
			evt->down = bDown;
			evt->state = buttonState;

			// Add the event to the list, guarded by a mutex
			pthread_mutex_lock(&buttonQueueMutex);
			list_node_t * buttonNode = list_node_new(evt);
			list_rpush(buttonQueue, buttonNode); 
			pthread_mutex_unlock(&buttonQueueMutex);
			
			break;
		}
	}
}

/**
 * Check if there were any queued input events and return them if there were
 * 
 * @param evt Return the event through this pointer argument
 * @return true if an event occurred and was returned, false otherwise
 */
bool checkInputKeys(buttonEvt_t * evt)
{
	// Check the queue, guarded by a mutex
	pthread_mutex_lock(&buttonQueueMutex);
	list_node_t * node = list_lpop(buttonQueue);
	pthread_mutex_unlock(&buttonQueueMutex);

	// No events
	if(NULL == node)
	{
		memset(evt, 0, sizeof(buttonEvt_t));
		return false;
	}
	else
	{
		// Copy the event to the arg
		memcpy(evt, node->val, sizeof(buttonEvt_t));
		// Free everything
		free(node->val);
		free(node);
		// Return that an event occurred
		return true;
	}
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
	free(buttonQueue);
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
        usleep(1000);
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
	usleep(1000);
}
